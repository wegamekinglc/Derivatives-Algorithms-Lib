#include <dal/platform/platform.hpp>

#include <chrono>
#include <cmath>
#include <exception>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>

#include <dal/benchmarks/bench.hpp>
#include <dal/curve/curveinstrumentevaluation_internal.hpp>
#include <dal/curve/depositscript.hpp>
#include <dal/curve/scriptcurvequoterisk.hpp>
#include <dal/math/aad/statistics.hpp>
#include <dal/math/buffercapacity.hpp>
#include <dal/platform/initall.hpp>
#include <dal/time/dateutils.hpp>

#include "compoundedoisfixture.hpp"
#include "scriptcurveperf.hpp"

namespace {
    using namespace Dal;
    using Clock_ = std::chrono::steady_clock;
    constexpr int WARMUPS = 3, SAMPLES = 20;

    CurveInstrumentCalibrationSpec_ Sample(int count) {
        const Date_ today(2026, 10, 21);
        CurveInstrumentCalibrationSpec_ spec;
        spec.geometry_.today_ = today;
        spec.geometry_.ccy_ = "USD";
        spec.geometry_.parameterization_ = CurveParameterization_::Value_::LOG_DISCOUNT;
        spec.geometry_.knotDates_ = {today};
        spec.valuation_.evaluationDate_ = today;
        spec.valuation_.fixings_.reset(new MarketFixingSnapshot_());
        spec.market_.routes_ = {{"USD.OIS"}};
        for (int i = 0; i < count; ++i) {
            const auto end = Date::AddMonths(today, 12 * (i + 1));
            const auto id = String::FromInt(i);
            const auto instrument =
                NewDepositScriptInstrument(Deposit_(today, end, .02, DayBasis_("ACT_360")), {"pv" + id, Ccy_("USD"), "USD.OIS", 1.});
            spec.geometry_.knotDates_.push_back(end);
            spec.observations_.push_back(NewCalibrationObservation(
                "o" + id, instrument, "ParRate", NewCalibrationMarketQuote("q" + id, .025 + .0005 * i, CalibrationValueUnit_::Parse("Rate")), 1e-13));
        }
        return spec;
    }
    CurveInstrumentCalibrationOptions_ Options() {
        CurveInstrumentCalibrationOptions_ result;
        result.jacobianMode_ = CalibrationDerivative_::Value_::AAD;
        return result;
    }
    CurveInstrumentCalibrationSpec_ Bumped(CurveInstrumentCalibrationSpec_ spec, int column, double bump) {
        const auto& row = *spec.observations_[column];
        const auto quote = NewCalibrationMarketQuote(row.Quote()->Id(), row.Quote()->Value() + bump, row.Quote()->Unit());
        spec.observations_[column] = NewCalibrationObservation(row.Id(), row.Instrument(), row.MeasureId(), quote, row.ErrorScale());
        return spec;
    }
    double Price(const Handle_<CalibrationInstrument_>& instrument, const CurveInstrumentCalibrationSpec_& spec, const Vector_<>& theta) {
        const auto prepared = CurveInstrumentInternal::PrepareCurvePrice(spec, instrument, "Price");
        const CandidateCurveProvider_<double> provider(prepared.geometry_, theta);
        CurveScriptInternal::EvaluationScratch_<double> scratch;
        return CurveInstrumentInternal::CashflowMeasure(prepared.plan_, provider, spec.geometry_.today_, &scratch);
    }
    Matrix_<> Shared(const CurveInstrumentCalibrationSpec_& spec, const ScriptCurveQuoteRiskSnapshot_& snapshot, int pvs) {
        Matrix_<> result(pvs, static_cast<int>(spec.observations_.size()));
        for (int i = 0; i < pvs; ++i) {
            const auto risk = ValueCalibrationInstrumentWithQuoteRisk(spec.observations_[i % result.Cols()]->Instrument(), snapshot, spec);
            REQUIRE(risk.Available(), "Script PV quote risk benchmark unavailable");
            REQUIRE(risk.Numerics().factorizationCount_ == 1, "Snapshot must retain one factorization");
            for (int j = 0; j < result.Cols(); ++j)
                result(i, j) = risk.RawSensitivity()[j];
        }
        return result;
    }
    CurveInstrumentCalibrationResult_ CalibratedRoot(const CurveInstrumentCalibrationSpec_& spec) {
        auto fit = CalibrateCurveInstruments(spec, Options());
        REQUIRE(fit.fitWithinTolerance_, "Benchmark bumped root must be accepted");
        for (const double residual : fit.rawResiduals_)
            REQUIRE(std::abs(residual) <= 1e-12, "Benchmark bumped raw residual exceeds 1e-12");
        return fit;
    }
    Matrix_<> Recalibrated(const CurveInstrumentCalibrationSpec_& spec, int pvs, int* calibrations, double step = 5e-5) {
        Matrix_<> result(pvs, static_cast<int>(spec.observations_.size()));
        for (int j = 0; j < result.Cols(); ++j) {
            const auto up = Bumped(spec, j, step), down = Bumped(spec, j, -step);
            const auto plus = CalibratedRoot(up), minus = CalibratedRoot(down);
            *calibrations += 2;
            for (int i = 0; i < pvs; ++i) {
                const auto& instrument = spec.observations_[i % result.Cols()]->Instrument();
                result(i, j) = (Price(instrument, up, plus.parameters_) - Price(instrument, down, minus.parameters_)) / (2. * step);
            }
        }
        return result;
    }
    template <class F_> double Timed(F_&& run) {
        const auto start = Clock_::now();
        const auto result = run();
        Bench::DoNotOptimize(&result);
        return std::chrono::duration<double, std::nano>(Clock_::now() - start).count();
    }
    template <class F_> std::optional<double> FullTiming(bool enabled, const F_& run) {
        return enabled ? std::optional<double>(Timed(run)) : std::nullopt;
    }
    void PrintFullTiming(const std::optional<double>& value) {
        if (value)
            std::cout << ",\"full_shared_ns\":" << *value;
    }
    void Verify(const Matrix_<>& actual, const Matrix_<>& expected) {
        for (int i = 0; i < actual.Rows(); ++i)
            for (int j = 0; j < actual.Cols(); ++j)
                REQUIRE(std::abs(actual(i, j) - expected(i, j)) <= 1e-7 + 1e-5 * std::abs(expected(i, j)), "Script benchmark FD mismatch");
    }
    void Run(const CurveInstrumentCalibrationSpec_& spec, int pvs, bool pvOnlyMode, bool dualMode, const std::string& metadata = "") {
        const int count = static_cast<int>(spec.observations_.size());
        const auto fit = CalibrateCurveInstruments(spec, Options());
        const auto start = Clock_::now();
        const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, {Vector_<>(count, 1e-12), 1e-10, 1e-12});
        const double preparation = std::chrono::duration<double, std::nano>(Clock_::now() - start).count();
        REQUIRE(snapshot.RiskAvailable(), "Script benchmark snapshot unavailable");
        std::optional<ScriptCurveQuoteRiskSnapshot_> pvOnly;
        double pvOnlyPreparation = 0.;
        if (pvOnlyMode) {
            auto accuracy = ScriptCurveQuoteRiskAccuracy_{Vector_<>(count, 1e-12), 1e-10, 1e-12};
            accuracy.pvOnly_ = true;
            const auto pvOnlyStart = Clock_::now();
            pvOnly = NewScriptCurveQuoteRiskSnapshot(fit, accuracy);
            pvOnlyPreparation = std::chrono::duration<double, std::nano>(Clock_::now() - pvOnlyStart).count();
            REQUIRE(pvOnly->RiskAvailable() && !pvOnly->MappingAvailable(), "Script PV-only benchmark snapshot unavailable");
        }
        int calibrations = 0;
        const auto expected = Recalibrated(spec, pvs, &calibrations);
        Verify(Shared(spec, snapshot, pvs), expected);
        if (pvOnly)
            Verify(Shared(spec, *pvOnly, pvs), expected);
        const auto measured = pvOnly.value_or(snapshot);
        for (int sample = -WARMUPS; sample < SAMPLES; ++sample) {
            const auto shared = [&] { return Shared(spec, measured, pvs); };
            const auto reference = [&] { return Recalibrated(spec, pvs, &calibrations); };
            double sharedNs, referenceNs;
            if (sample % 2 == 0) {
                sharedNs = Timed(shared);
                referenceNs = Timed(reference);
            } else {
                referenceNs = Timed(reference);
                sharedNs = Timed(shared);
            }
            const auto full = FullTiming(dualMode, [&] { return Shared(spec, snapshot, pvs); });
            if (sample >= 0) {
                std::cout << "{\"quotes\":" << count << ",\"pvs\":" << pvs << ",\"sample\":" << sample << ",\"shared_ns\":" << sharedNs
                          << ",\"recalibrated_ns\":" << referenceNs << ",\"prepare_ns\":" << preparation << ",\"pv_only\":" << std::boolalpha
                          << pvOnlyMode;
                if (pvOnly)
                    std::cout << ",\"pv_only_prepare_ns\":" << pvOnlyPreparation;
                PrintFullTiming(full);
                std::cout << ",\"factorizations\":" << snapshot.Numerics().factorizationCount_
                          << ",\"reference_calibrations_per_sample\":" << 2 * count << metadata << "}\n";
            }
        }
        REQUIRE(calibrations == (1 + WARMUPS + SAMPLES) * 2 * count, "Reference calibration counter mismatch");
    }
    void ProbeOis(const CompoundedOisRiskCase_& fixture) {
        const auto fit = CalibratedRoot(fixture.spec_);
        BufferCapacityBudget_ budget(std::numeric_limits<size_t>::max());
        {
            BufferCapacityScope_ scope(&budget);
            const auto count = fixture.spec_.observations_.size();
            auto accuracy = ScriptCurveQuoteRiskAccuracy_{Vector_<>(count, 1e-12), 1e-10, 1e-12};
            const auto full = NewScriptCurveQuoteRiskSnapshot(fit, accuracy);
            accuracy.pvOnly_ = true;
            const auto lean = NewScriptCurveQuoteRiskSnapshot(fit, accuracy);
            for (const auto& diagnostic : full.Diagnostics())
                std::cerr << diagnostic.code_ << ": " << diagnostic.field_.value_or("") << "; " << diagnostic.message_ << '\n';
            REQUIRE(full.MappingAvailable() && lean.RiskAvailable() && !lean.MappingAvailable(), "OIS snapshot resource probe unavailable");
            Verify(Shared(fixture.spec_, full, fixture.pvs_), Shared(fixture.spec_, lean, fixture.pvs_));
        }
        const auto tape = AAD::MeasureTape(*AAD::Tape());
        REQUIRE(budget.CapacityBytes() == 0 && tape.nodes_ == 0 && tape.liveBytes_ == 0, "OIS snapshot vector and AAD ownership released");
        std::cout << "OIS_RISK_RESOURCE quotes=" << fixture.spec_.observations_.size() << " pvs=" << fixture.pvs_ << " factors=" << fixture.factors_
                  << " vector_peak_bytes=" << budget.PeakCapacityBytes() << " after_vector_bytes=" << budget.CapacityBytes()
                  << " after_tape_nodes=" << tape.nodes_ << " after_tape_live_bytes=" << tape.liveBytes_
                  << " retained_tape_capacity=" << tape.capacityBytes_ << '\n';
    }
    void ProbeOisReference(const CompoundedOisRiskCase_& fixture) {
        int calibrations = 0;
        const auto coarse = Recalibrated(fixture.spec_, fixture.pvs_, &calibrations, 1e-4);
        const auto fine = Recalibrated(fixture.spec_, fixture.pvs_, &calibrations, 5e-5);
        Verify(coarse, fine);
        REQUIRE(calibrations == 4 * static_cast<int>(fixture.spec_.observations_.size()), "Two-step OIS reference root count");
        std::cout << "OIS_RISK_FD quotes=" << fixture.spec_.observations_.size() << " pvs=" << fixture.pvs_ << " factors=" << fixture.factors_
                  << " independent_calibrations=" << calibrations << " coarse_step=0.0001 fine_step=0.00005\n";
    }
} // namespace

int RunScriptCurveQuoteRiskBenchmarks(bool pvOnlyMode, bool scoped) {
    try {
        std::cout << std::setprecision(17);
        for (const int count : {5, 15})
            for (const int pvs : {1, 100}) {
                if (scoped && count == 5 && pvs == 100)
                    continue;
                Run(Sample(count), pvs, pvOnlyMode, scoped && pvOnlyMode);
            }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 2;
    }
}

int RunScriptCurveQuoteRiskMode(const char* option) {
    if (std::string(option) == "--compounded-ois-quote-risk") {
        try {
            RegisterAll_::Init();
            std::cout << std::setprecision(17);
            for (const auto& fixture : CompoundedOisRiskCases()) {
                ProbeOis(fixture);
                ProbeOisReference(fixture);
                const auto metadata = ",\"family\":\"OIS\",\"mode\":\"" + std::string(fixture.mode_.begin(), fixture.mode_.end()) +
                                      "\",\"factors\":" + std::to_string(fixture.factors_) +
                                      ",\"role\":\"Overnight\",\"notional\":1,\"parameterization\":\"" +
                                      fixture.spec_.geometry_.parameterization_.String() + "\"";
                Run(fixture.spec_, fixture.pvs_, true, true, metadata);
            }
            return 0;
        } catch (const std::exception& error) {
            std::cerr << error.what() << '\n';
            return 2;
        }
    }
    const std::map<std::string, std::pair<bool, bool>> modes{{"--script-curve-quote-risk", {false, false}},
                                                             {"--script-curve-quote-risk-pv-only", {true, false}},
                                                             {"--script-curve-quote-risk-scoped", {false, true}},
                                                             {"--script-curve-quote-risk-pv-only-scoped", {true, true}}};
    const auto found = modes.find(option);
    return found == modes.end() ? -1 : RunScriptCurveQuoteRiskBenchmarks(found->second.first, found->second.second);
}

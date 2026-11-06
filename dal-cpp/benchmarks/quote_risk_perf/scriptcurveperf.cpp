#include <dal/platform/platform.hpp>

#include <chrono>
#include <cmath>
#include <exception>
#include <iomanip>
#include <iostream>

#include <dal/benchmarks/bench.hpp>
#include <dal/curve/curveinstrumentevaluation_internal.hpp>
#include <dal/curve/depositscript.hpp>
#include <dal/curve/scriptcurvequoterisk.hpp>
#include <dal/time/dateutils.hpp>

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
    Matrix_<> Recalibrated(const CurveInstrumentCalibrationSpec_& spec, int pvs, int* calibrations) {
        Matrix_<> result(pvs, static_cast<int>(spec.observations_.size()));
        for (int j = 0; j < result.Cols(); ++j) {
            const auto up = Bumped(spec, j, 5e-5), down = Bumped(spec, j, -5e-5);
            const auto plus = CalibrateCurveInstruments(up, Options()), minus = CalibrateCurveInstruments(down, Options());
            *calibrations += 2;
            for (int i = 0; i < pvs; ++i) {
                const auto& instrument = spec.observations_[i % result.Cols()]->Instrument();
                result(i, j) = (Price(instrument, up, plus.parameters_) - Price(instrument, down, minus.parameters_)) / 1e-4;
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
    void Verify(const Matrix_<>& actual, const Matrix_<>& expected) {
        for (int i = 0; i < actual.Rows(); ++i)
            for (int j = 0; j < actual.Cols(); ++j)
                REQUIRE(std::abs(actual(i, j) - expected(i, j)) <= 1e-7 + 1e-5 * std::abs(expected(i, j)), "Script benchmark FD mismatch");
    }
    void Run(int count, int pvs) {
        const auto spec = Sample(count);
        const auto fit = CalibrateCurveInstruments(spec, Options());
        const auto start = Clock_::now();
        const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, {Vector_<>(count, 1e-12), 1e-10, 1e-12});
        const double preparation = std::chrono::duration<double, std::nano>(Clock_::now() - start).count();
        REQUIRE(snapshot.RiskAvailable(), "Script benchmark snapshot unavailable");
        int calibrations = 0;
        Verify(Shared(spec, snapshot, pvs), Recalibrated(spec, pvs, &calibrations));
        for (int sample = -WARMUPS; sample < SAMPLES; ++sample) {
            const auto shared = [&] { return Shared(spec, snapshot, pvs); };
            const auto reference = [&] { return Recalibrated(spec, pvs, &calibrations); };
            double sharedNs, referenceNs;
            if (sample % 2 == 0) {
                sharedNs = Timed(shared);
                referenceNs = Timed(reference);
            } else {
                referenceNs = Timed(reference);
                sharedNs = Timed(shared);
            }
            if (sample >= 0)
                std::cout << "{\"quotes\":" << count << ",\"pvs\":" << pvs << ",\"sample\":" << sample << ",\"shared_ns\":" << sharedNs
                          << ",\"recalibrated_ns\":" << referenceNs << ",\"prepare_ns\":" << preparation
                          << ",\"factorizations\":" << snapshot.Numerics().factorizationCount_
                          << ",\"reference_calibrations_per_sample\":" << 2 * count << "}\n";
        }
        REQUIRE(calibrations == (1 + WARMUPS + SAMPLES) * 2 * count, "Reference calibration counter mismatch");
    }
} // namespace

int RunScriptCurveQuoteRiskBenchmarks() {
    try {
        std::cout << std::setprecision(17);
        for (const int count : {5, 15})
            for (const int pvs : {1, 100})
                Run(count, pvs);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 2;
    }
}

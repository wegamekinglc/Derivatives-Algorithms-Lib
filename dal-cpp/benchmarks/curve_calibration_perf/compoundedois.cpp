#include <dal/platform/platform.hpp>

#include <chrono>
#include <fstream>
#include <iostream>
#include <string>

#include <dal/benchmarks/bench.hpp>
#include <dal/curve/calibration_internal.hpp>
#include <dal/curve/compoundedois.hpp>
#include <dal/curve/curveinstrumentevaluation_internal.hpp>
#include <dal/curve/curvescript.hpp>
#include <dal/indice/indexparse.hpp>
#include <dal/math/aad/recording.hpp>
#include <dal/math/aad/statistics.hpp>
#include <dal/time/dateutils.hpp>

#include "compoundedois.hpp"

using namespace Dal;

namespace {
    size_t indexParses = 0;
    size_t sourceBuilds = 0, planPreparations = 0;
    using Clock_ = std::chrono::steady_clock;

    struct Fixture_ {
        CompoundedOisScriptSpec_ contract_;
        CurveInstrumentCalibrationSpec_ context_;
        Vector_<> theta_;
        size_t historical_ = 0;
    };

    Fixture_ Fixture(size_t count) {
        Fixture_ fixture;
        auto& s = fixture.contract_;
        s.instrumentId_ = "performance-ois";
        s.notional_ = 1.;
        s.coupon_ = .025;
        s.direction_ = "PayFixed";
        s.currency_ = s.collateralCurrency_ = Ccy_("USD");
        s.collateralType_ = CollateralType_("OIS");
        s.discountRoute_ = "USD.OIS";
        s.projectionRoute_ = "USD.SOFR";
        s.fixingIndex_ = "USD-SOFR";
        s.observationBasis_ = s.fixedBasis_ = DayBasis_("ACT_360");
        s.calendar_ = HolidayCenterData_("explicit-weekends", {});
        const Date_ start(2024, 1, 1);
        auto day = start;
        for (size_t i = 0; i < count; ++i) {
            auto end = day.AddDays(1);
            while (Date::IsWeekEnd(end))
                end = end.AddDays(1);
            s.observations_.push_back({day, end, DateTime_(day), (end - day) / 360., "coupon", day, end});
            day = end;
        }
        s.fixedLeg_ = {{start, day, day.AddDays(2), (day - start) / 360.}};
        s.coupons_ = {{"coupon", start, day, day.AddDays(2)}};
        fixture.historical_ = count / 2;
        const auto today = s.observations_[fixture.historical_].start_;
        auto& c = fixture.context_;
        c.valuation_.evaluationDate_ = today;
        c.valuation_.todayFixingPolicy_ = TodayFixingPolicy_("Model");
        std::map<DateTime_, double> history;
        for (size_t i = 0; i < fixture.historical_; ++i)
            history.emplace(s.observations_[i].fixingTime_, .02);
        c.valuation_.fixings_.reset(new MarketFixingSnapshot_({{s.fixingIndex_, history}}));
        c.geometry_.today_ = today;
        c.geometry_.ccy_ = "USD";
        c.geometry_.parameterization_ = CurveParameterization_("LOG_DISCOUNT");
        c.geometry_.calibrateDiscountCurve_ = false;
        c.geometry_.targetOvernightRoute_ = s.projectionRoute_;
        for (int i = 0; i <= 8; ++i) {
            c.geometry_.knotDates_.push_back(today.AddDays(400 * i));
            if (i)
                fixture.theta_.push_back(-(.02 + .0003 * i) * (400. * i / 365.));
        }
        c.market_.routes_ = {{s.discountRoute_, false, CollateralType_("OIS"), {}, false, true},
                             {s.projectionRoute_, true, CollateralType_("OIS"), {}, true}};
        return fixture;
    }

    CurveInstrumentCalibrationSpec_ PreparedSpec(const Fixture_& fixture) {
        auto result = fixture.context_;
        result.compoundedOis_ = {PrepareCompoundedOis(fixture.contract_, result.valuation_)};
        const auto instrument = NewCompoundedOisScriptInstrument(result.compoundedOis_[0]);
        result.observations_ = {NewCalibrationObservation("pv", instrument, "Price",
                                                          NewCalibrationMarketQuote("pv", 0., CalibrationValueUnit_::Parse("Price:USD")), 1e-10),
                                NewCalibrationObservation("par", instrument, "ParRate",
                                                          NewCalibrationMarketQuote("par", .03, CalibrationValueUnit_::Parse("Rate")), 1e-10)};
        return result;
    }

    // Independent scalar reference: no builder, descriptor, provider or cashflow helper.
    double LogDiscount(Date_ date, Date_ today, const Vector_<>& theta) {
        const double position = (date - today) / 400.;
        const size_t upper = std::min(static_cast<size_t>(std::floor(position)) + 1, theta.size());
        const double left = upper == 1 ? 0. : theta[upper - 2];
        return left + (theta[upper - 1] - left) * (position - (upper - 1));
    }

    Vector_<> Scalar(const Fixture_& f, const Vector_<>& theta) {
        const auto today = f.context_.geometry_.today_;
        double product = 1.;
        for (size_t i = 0; i < f.contract_.observations_.size(); ++i) {
            const auto& row = f.contract_.observations_[i];
            const double rate = i < f.historical_ ? .02
                                                  : std::expm1(LogDiscount(row.rateStart_, today, theta) - LogDiscount(row.rateEnd_, today, theta)) /
                                                        ((row.rateEnd_ - row.rateStart_) / 360.);
            product *= 1. + row.accrual_ * rate;
        }
        const double annuity = f.contract_.fixedLeg_[0].accrual_;
        const double discount = std::exp(LogDiscount(f.contract_.coupons_[0].payment_, today, theta));
        return {(product - 1. - f.contract_.coupon_ * annuity) * discount, (product - 1.) / annuity};
    }

    struct Plan_ {
        Vector_<PreparedCalibrationInstrument_> measures_;
        std::shared_ptr<const CandidateCurveGeometry_> geometry_;
    };

    Plan_ Prepare(const Fixture_& fixture, bool counted = false) {
        auto context = fixture.context_;
        context.compoundedOis_ = {PrepareCompoundedOis(fixture.contract_, context.valuation_)};
        auto instrument = NewCompoundedOisScriptInstrument(context.compoundedOis_[0]);
        ++sourceBuilds;
        if (counted) {
            auto definition = instrument->Definition()->Spec();
            auto& text = definition.source_.rows_.back().text_;
            size_t pos = 0;
            while ((pos = text.find("IR[", pos)) != String_::npos) {
                text.replace(pos, 2, "ONPERF");
                pos += 6;
            }
            instrument = NewScriptCalibrationInstrument({instrument->Id(), NewScriptInstrumentDefinition(definition), {}, instrument->Pillar()});
            ++sourceBuilds;
        }
        Plan_ result;
        for (const auto& measure : {String_("Price"), String_("ParRate")}) {
            auto prepared = CurveInstrumentInternal::PrepareCurvePrice(context, instrument, measure);
            ++planPreparations;
            result.geometry_ = prepared.geometry_;
            result.measures_.push_back(std::move(prepared.plan_));
        }
        return result;
    }

    template <class T_> size_t ScratchBytes(const CurveScriptInternal::EvaluationScratch_<T_>& scratch) {
        return sizeof(scratch) + scratch.affine_.overflow_.capacity() * sizeof(CurveScriptInternal::Affine_<T_>) +
               scratch.independent_.overflow_.capacity() * sizeof(T_) +
               scratch.observations_.capacity() * sizeof(CurveScriptInternal::ObservationBinding_<T_>);
    }

    template <class T_>
    Vector_<T_> Values(const Plan_& plan, const Vector_<T_>& theta, Date_ today, CurveScriptInternal::EvaluationScratch_<T_>* scratch) {
        const CandidateCurveProvider_<T_> provider(plan.geometry_, theta);
        Vector_<T_> values;
        std::optional<CurveScriptInternal::Affine_<T_>> coefficients;
        if (plan.measures_.size() == 2 && CurveScriptInternal::CanShareCoefficients(plan.measures_[0], plan.measures_[1]))
            coefficients = CurveScriptInternal::Coefficients(plan.measures_[0], provider, today, scratch);
        for (const auto& measure : plan.measures_)
            values.push_back(coefficients ? CurveScriptInternal::Measure(measure, *coefficients)
                                          : CurveInstrumentInternal::CashflowMeasure(measure, provider, today, scratch));
        return values;
    }

    struct Resource_ {
        size_t scratchBytes_ = 0;
        AAD::TapeStatistics_ tape_;
    };

    Matrix_<> Partials(const Plan_& plan, const Fixture_& fixture, Resource_* resource = nullptr) {
        const auto jacobian = TapeResidualJacobian(fixture.theta_, [&](const auto& inputs) {
            CurveScriptInternal::EvaluationScratch_<AAD::Number_> scratch;
            const auto values = Values(plan, inputs, fixture.context_.geometry_.today_, &scratch);
            if (resource) {
                resource->scratchBytes_ = ScratchBytes(scratch);
                resource->tape_ = AAD::MeasureTape(*AAD::Tape());
            }
            return values;
        });
        return static_cast<const XCurveJacobian_&>(*jacobian).j_;
    }

    void Check(const Plan_& plan, const Fixture_& fixture) {
        CurveScriptInternal::EvaluationScratch_<double> scratch;
        const auto actual = Values(plan, fixture.theta_, fixture.context_.geometry_.today_, &scratch), expected = Scalar(fixture, fixture.theta_);
        const auto partials = Partials(plan, fixture);
        for (int i = 0; i < 2; ++i)
            REQUIRE(std::abs(actual[i] - expected[i]) <= 1e-10, "independent scalar Price/ParRate oracle");
        for (size_t j = 0; j < fixture.theta_.size(); ++j) {
            Vector_<> previous;
            for (const double h : {1e-4, 5e-5}) {
                auto up = fixture.theta_, down = fixture.theta_;
                up[j] += h;
                down[j] -= h;
                const auto a = Scalar(fixture, up), b = Scalar(fixture, down);
                Vector_<> reference(2);
                for (int i = 0; i < 2; ++i) {
                    reference[i] = (a[i] - b[i]) / (2. * h);
                    const double tolerance = 1e-7 + 1e-5 * std::abs(reference[i]);
                    if (!previous.empty())
                        REQUIRE(std::abs(reference[i] - previous[i]) <= tolerance, "stable independent double-step FD");
                    REQUIRE(std::abs(partials(i, j) - reference[i]) <= tolerance, "native AAD versus independent double-step FD");
                }
                previous = reference;
            }
        }
        REQUIRE(AAD::MeasureTape(*AAD::Tape()).nodes_ == 0, "AAD cleanup after oracle checks");
    }

    size_t RssKb(const std::string& key) {
#if defined(__linux__)
        std::ifstream status("/proc/self/status");
        std::string token, rest;
        while (status >> token) {
            if (token == key) {
                size_t value = 0;
                status >> value;
                return value;
            }
            std::getline(status, rest);
        }
#else
        (void)key;
#endif
        return 0;
    }

    template <class F_> void Timed(const std::string& name, F_&& action, int inner = 1) {
        Bench::Print(Bench::Run(name, std::forward<F_>(action), 2, 10, inner));
    }

    void RunCase(size_t count) {
        const auto initialBuilds = sourceBuilds, initialPlans = planPreparations;
        const auto fixture = Fixture(count);
        const auto coldStart = Clock_::now();
        const auto plan = Prepare(fixture);
        const auto coldNs = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock_::now() - coldStart).count();
        const auto initialTape = AAD::MeasureTape(*AAD::Tape());
        Resource_ resource;
        const auto aadStart = Clock_::now();
        const auto partials = Partials(plan, fixture, &resource);
        const auto aadInstrumentedNs = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock_::now() - aadStart).count();
        const auto after = AAD::MeasureTape(*AAD::Tape());
        Check(plan, fixture);
        indexParses = 0;
        const auto counted = Prepare(fixture, true);
        REQUIRE(indexParses == count, "one ON literal parse per source factor");
        Check(counted, fixture);
        REQUIRE(indexParses == count, "prepared Price/ParRate and AAD must not reparse source");
        const auto& descriptor = *std::get<CalibrationCurveFamily_>(plan.measures_[0].Descriptor());
        const auto& flow = descriptor.payments_.back();
        REQUIRE(flow.program_.size() == 2 * count + 4, "linear controlled product program");
        CurveScriptInternal::EvaluationScratch_<double> scratch;
        const auto passiveStart = Clock_::now();
        const auto passive = Values(plan, fixture.theta_, fixture.context_.geometry_.today_, &scratch);
        const auto passiveColdNs = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock_::now() - passiveStart).count();
        CurveInstrumentCalibrationOptions_ preparedOptions;
        preparedOptions.jacobianMode_ = CalibrationDerivative_::Value_::AAD;
        const PreparedCurveInstruments_ prepared(PreparedSpec(fixture), preparedOptions);
        const auto preparedResult = prepared.Evaluate(fixture.theta_);
        const auto expected = Scalar(fixture, fixture.theta_);
        for (int i = 0; i < 2; ++i) {
            REQUIRE(std::abs(preparedResult.modelMeasures_[i] - expected[i]) <= 1e-10, "prepared calibration Price/ParRate oracle");
            for (int j = 0; j < preparedResult.quoteJacobian_.Cols(); ++j)
                REQUIRE(std::abs(preparedResult.quoteJacobian_(i, j) - partials(i, j)) <= 1e-10, "prepared calibration AAD oracle");
        }
        const std::string prefix = "OIS D=" + std::to_string(count) + " ";
        const auto rssBefore = RssKb("VmRSS:"), hwmBefore = RssKb("VmHWM:");
        double sink = passive[0] + partials(0, 0);
        Timed(prefix + "prepare tables/source/two plans", [&]() { sink += Prepare(fixture).measures_.size(); });
        Timed(
            prefix + "prepared passive warm scratch", [&]() { sink += Values(plan, fixture.theta_, fixture.context_.geometry_.today_, &scratch)[0]; },
            10);
        Timed(prefix + "prepared AAD two reverse seeds", [&]() { sink += Partials(plan, fixture)(0, 0); }, 10);
        Timed(prefix + "prepared calibration residuals", [&]() { sink += prepared.Residuals(fixture.theta_)[0]; }, 10);
        Timed(prefix + "prepared calibration AAD evaluate", [&]() { sink += prepared.Evaluate(fixture.theta_).quoteJacobian_(0, 0); });
        Timed(prefix + "independent scalar product", [&]() { sink += Scalar(fixture, fixture.theta_)[0]; }, 100);
        const CandidateCurveProvider_<double> provider(plan.geometry_, fixture.theta_);
        Timed(
            prefix + "future curve queries only",
            [&]() {
                for (size_t i = fixture.historical_; i < count; ++i) {
                    const auto& row = fixture.contract_.observations_[i];
                    sink += provider.Forward(fixture.contract_.projectionRoute_, row.rateStart_, row.rateEnd_, fixture.contract_.observationBasis_);
                }
            },
            10);
        Bench::DoNotOptimize(&sink);
        const auto scratchBytes = ScratchBytes(scratch);
        std::cout << "OIS_RESOURCE D=" << count << " K=9 native_parameters=8 history=" << fixture.historical_
                  << " source_builds_per_prepare=1 additional_counted_probe_builds=1 plans=2"
                  << " total_source_builds=" << sourceBuilds - initialBuilds << " total_plan_preparations=" << planPreparations - initialPlans
                  << " counted_index_parses=" << indexParses << " prepared_reparses=" << indexParses - count
                  << " instructions=" << flow.program_.size() << " program_payload=" << flow.program_.size() * sizeof(CalibrationCurveInstruction_)
                  << " passive_scratch_bytes=" << scratchBytes << " aad_scratch_bytes=" << resource.scratchBytes_
                  << " scratch_tape_peak_payload=" << resource.scratchBytes_ + resource.tape_.liveBytes_ << " tape_nodes=" << resource.tape_.nodes_
                  << " tape_live_bytes=" << resource.tape_.liveBytes_ << " tape_reserved_bytes=" << resource.tape_.capacityBytes_
                  << " after_nodes=" << after.nodes_ << " after_live_bytes=" << after.liveBytes_ << " after_reserved_bytes=" << after.capacityBytes_
                  << " initial_tape_nodes=" << initialTape.nodes_ << " initial_tape_reserved_bytes=" << initialTape.capacityBytes_
                  << " prepare_cold_ns=" << coldNs << " passive_fresh_scratch_ns=" << passiveColdNs << " aad_instrumented_ns=" << aadInstrumentedNs
                  << " rss_before_kb=" << rssBefore << " hwm_before_kb=" << hwmBefore << " rss_after_kb=" << RssKb("VmRSS:")
                  << " process_peak_rss_kb=" << RssKb("VmHWM:") << '\n';
    }
} // namespace

void RunCompoundedOis() {
    Index::RegisterParser("ONPERF", [](const String_& text) {
        ++indexParses;
        return Index::Parse("IR" + text.substr(6));
    });
    Bench::PrintHeader();
    for (const size_t count : {30u, 365u, 1825u})
        RunCase(count);
    std::cout << "Compounded OIS contract verified: D=30,365,1825\n";
}

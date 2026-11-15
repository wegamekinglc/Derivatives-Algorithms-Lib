#include <gtest/gtest.h>

#include <dal/curve/compoundedoisscript_internal.hpp>
#include <dal/curve/curveinstrumentevaluation_internal.hpp>
#include <dal/math/aad/recording.hpp>
#include <dal/math/aad/statistics.hpp>
#include <dal/script/preparation.hpp>
#include <dal/storage/json.hpp>

#include "compoundedoisfixture.hpp"

using namespace Dal;

namespace {
    CompoundedOisScriptSpec_ Contract(const String_& mode) {
        auto s = CompoundedOisTest::Contract();
        s.ruleId_ = "dal.compounded-ois-cashflows/2";
        const bool shifted = mode == "Lookback" || mode == "ObservationShift";
        s.observationConvention_ =
            CompoundedOisObservationConvention_{CompoundedOisObservationMode_(mode), shifted ? 1 : 0, mode == "Lockout" ? 1 : 0};
        s.observations_.clear();
        s.coupons_ = {{"coupon", Date_(2024, 3, 5), Date_(2024, 3, 8), Date_(2024, 3, 8)}};
        s.fixedLeg_ = {{Date_(2024, 3, 5), Date_(2024, 3, 8), Date_(2024, 3, 8), 3. / 365.}};
        return s;
    }
    CurveScriptEvaluationRequest_ Request(const CompoundedOisScriptSpec_& s) {
        auto r = CompoundedOisTest::Request(CompoundedOisTest::Calibration("Shared"));
        r.compoundedOis_ = PrepareCompoundedOis(s, r.valuation_);
        r.derivativeMethod_ = CalibrationDerivative_::Value_::AAD;
        return r;
    }
    CalibrationPreparationContext_ Context(const Handle_<PreparedCompoundedOis_>& tables) {
        CalibrationPreparationContext_ c;
        c.valuation_ = tables->Valuation();
        c.currency_ = Ccy_("USD");
        c.discountRoute_ = "USD.OIS";
        c.curve_.reset(new GSRCurveData_("explicit", *c.valuation_.evaluationDate_, "USD", {*c.valuation_.evaluationDate_, Date_(2024, 4, 1)},
                                         {0., -.002}, {}, {}));
        c.compoundedOis_ = tables;
        return c;
    }
    CurveInstrumentCalibrationSpec_ PricingSpec(const CurveScriptEvaluationRequest_& r) {
        CurveInstrumentCalibrationSpec_ s;
        s.geometry_ = r.geometry_;
        s.market_ = r.market_;
        s.valuation_ = r.valuation_;
        s.compoundedOis_ = {r.compoundedOis_};
        return s;
    }
    template <class F_> CalibrationDiagnostic_ Failure(const F_& action) {
        try {
            action();
        } catch (const CalibrationError_& error) {
            return error.Failure().diagnostic_;
        }
        ADD_FAILURE() << "Expected owning calibration failure";
        return {};
    }
    // Hand-written dates/weights: independent of the expanded schedule and source.
    Vector_<> Oracle(const CompoundedOisScriptSpec_& s, const Vector_<>& theta) {
        const String_ mode = s.observationConvention_->mode_.String();
        Vector_<Date_> starts{Date_(2024, 3, 5), Date_(2024, 3, 6), Date_(2024, 3, 7)};
        Vector_<Date_> ends{Date_(2024, 3, 6), Date_(2024, 3, 7), Date_(2024, 3, 8)};
        Vector_<> days{1., 1., 1.};
        if (mode == "Lookback" || mode == "ObservationShift") {
            starts = {Date_(2024, 3, 1), Date_(2024, 3, 5), Date_(2024, 3, 6)};
            ends = {Date_(2024, 3, 5), Date_(2024, 3, 6), Date_(2024, 3, 7)};
            if (mode == "ObservationShift")
                days[0] = 4.;
        }
        if (mode == "Lockout") {
            starts[2] = starts[1];
            ends[2] = ends[1];
        }
        double product = 1.;
        for (int i = 0; i < 3; ++i) {
            const auto rate = std::expm1(CompoundedOisTest::LogDiscount(starts[i], theta) - CompoundedOisTest::LogDiscount(ends[i], theta)) /
                              ((ends[i] - starts[i]) / 360.);
            product *= 1. + days[i] / 360. * rate;
        }
        const double sign = s.direction_ == "PayFixed" ? 1. : -1.;
        return {sign * s.notional_ * (product - 1. - s.coupon_ * 3. / 365.) * std::exp(CompoundedOisTest::LogDiscount(Date_(2024, 3, 8), theta)),
                (product - 1.) / (3. / 365.)};
    }
    void AssertOracle(const CompoundedOisScriptSpec_& s) {
        auto r = Request(s);
        const auto instrument = NewCompoundedOisScriptInstrument(r.compoundedOis_);
        const auto expected = Oracle(s, r.parameters_);
        for (const auto backend : {CalibrationEvaluation_("AutoFast"), CalibrationEvaluation_("GenericDeterministic")}) {
            r.evaluation_ = backend;
            const auto result = EvaluateCurveScriptInstrument(instrument, r);
            ASSERT_NEAR(result.values_[0], expected[0], 1e-11 * s.notional_);
            ASSERT_NEAR(result.values_[1], expected[1], 1e-11);
            for (int j = 0; j < 2; ++j) {
                Vector_<> previous;
                for (const double h : {1e-4, 5e-5}) {
                    auto plus = r.parameters_, minus = plus;
                    plus[j] += h;
                    minus[j] -= h;
                    const auto up = Oracle(s, plus), down = Oracle(s, minus);
                    Vector_<> fd(2);
                    for (int i = 0; i < 2; ++i) {
                        fd[i] = (up[i] - down[i]) / (2. * h);
                        const double scale = i == 0 ? s.notional_ : 1.;
                        const double tolerance = scale * 1e-7 + 1e-5 * std::abs(fd[i]);
                        ASSERT_NEAR(result.nativePartials_(i, j), fd[i], tolerance);
                        if (!previous.empty())
                            ASSERT_NEAR(fd[i], previous[i], tolerance);
                    }
                    previous = fd;
                }
            }
        }
    }
} // namespace

TEST(CompoundedOisV2EvaluationTest, TestFourModesAgainstIndependentAmountsAndTwoStepNativeFd) {
    for (const auto& mode : CompoundedOisObservationModeListAll()) {
        SCOPED_TRACE(mode.String());
        ASSERT_NO_FATAL_FAILURE(AssertOracle(Contract(mode.String())));
    }
}

TEST(CompoundedOisV2EvaluationTest, TestUnfinishedCalibrationIsRejectedAfterDirectPricingAdmission) {
    const auto s = Contract("Lookback");
    const auto r = Request(s);
    auto fit = CompoundedOisTest::Calibration("Shared");
    fit.compoundedOis_ = {r.compoundedOis_};
    const auto instrument = NewCompoundedOisScriptInstrument(r.compoundedOis_);
    fit.observations_ = {NewCalibrationObservation("rate", instrument, "ParRate",
                                                   NewCalibrationMarketQuote("rate", .02, CalibrationValueUnit_::Parse("Rate")), 1e-12)};
    ASSERT_EQ(Failure([&] { PreparedCurveInstruments_ prepared(fit); }).code_, "UnsupportedCompoundedOisCalibration");
}

TEST(CompoundedOisV2EvaluationTest, TestNotionalDirectionAndNativeFiniteDifference) {
    for (const auto& mode : CompoundedOisObservationModeListAll()) {
        auto s = Contract(mode.String());
        const auto original = EvaluateCurveScriptInstrument(NewCompoundedOisScriptInstrument(Request(s).compoundedOis_), Request(s));
        s.notional_ *= 2.;
        s.direction_ = "ReceiveFixed";
        auto r = Request(s);
        const auto instrument = NewCompoundedOisScriptInstrument(r.compoundedOis_);
        const auto scaled = EvaluateCurveScriptInstrument(instrument, r);
        ASSERT_NEAR(scaled.values_[0], -2. * original.values_[0], 1e-12);
        ASSERT_NEAR(scaled.values_[1], original.values_[1], 1e-12);
        r.derivativeMethod_ = CalibrationDerivative_::Value_::FiniteDifference;
        const auto difference = EvaluateCurveScriptInstrument(instrument, r);
        for (int j = 0; j < 2; ++j) {
            ASSERT_NEAR(scaled.nativePartials_(0, j), -2. * original.nativePartials_(0, j), 1e-10);
            ASSERT_NEAR(difference.nativePartials_(0, j), scaled.nativePartials_(0, j), 1e-5);
        }
    }
}

TEST(CompoundedOisV2EvaluationTest, TestLockoutKeepsUnequalFactorsAndAccumulatesSharedRateAdjoints) {
    auto s = Contract("Lockout");
    s.observationConvention_->lockoutBusinessDays_ = 2;
    s.coupons_[0].end_ = s.coupons_[0].payment_ = Date_(2024, 3, 11);
    s.fixedLeg_[0].end_ = s.fixedLeg_[0].payment_ = Date_(2024, 3, 11);
    s.fixedLeg_[0].accrual_ = 6. / 365.;
    auto v = CompoundedOisTest::Valuation();
    v.evaluationDate_ = Date_(2024, 3, 7);
    v.fixings_.reset(new MarketFixingSnapshot_({{s.fixingIndex_, {{DateTime_(Date_(2024, 3, 5)), .01}, {DateTime_(Date_(2024, 3, 6)), .2}}}}));
    const auto tables = PrepareCompoundedOis(s, v);
    const auto plan = PrepareCalibrationInstrument(NewCompoundedOisScriptInstrument(tables), "Price", Context(tables));
    const auto& descriptor = *std::get<CalibrationCurveFamily_>(plan.Descriptor());
    ASSERT_EQ(descriptor.observations_.size(), 4u);
    const auto& flow = descriptor.payments_.back();
    const double a = 1. + .01 / 360., b = 1. + .2 / 360., c = 1. + .2 * 3. / 360.;
    const double product = a * b * b * c;
    ASSERT_GT(std::abs((product - 1.) - (.01 / 360. + .2 * 5. / 360.)), 1e-7);
    {
        AAD::RecordingScope_ recording;
        AAD::Number_ first, shared;
        recording.RegisterInput(first, .01);
        recording.RegisterInput(shared, .2);
        recording.StartRecording();
        const auto value = CurveScriptInternal::EvaluateIndependent<AAD::Number_>(flow, [&](size_t i) { return i == 0 ? first : shared; });
        recording.FinishRecording();
        recording.ClearAdjoints();
        AAD::Adjoint(value) = 1.;
        recording.Reverse();
        ASSERT_NEAR(static_cast<double>(value), s.notional_ * (product - 1.), 1e-12);
        ASSERT_NEAR(AAD::AdjointValue(shared), s.notional_ * a * (2. * b * c / 360. + b * b * 3. / 360.), 1e-12);
    }
    ASSERT_EQ(AAD::MeasureTape(*AAD::Tape()).nodes_, 0u);
}

TEST(CompoundedOisV2EvaluationTest, TestMappedTodayHistoryAndMissingFixingOwnsScheduleLocation) {
    auto s = Contract("Lookback");
    auto r = Request(s);
    const auto instrument = NewCompoundedOisScriptInstrument(r.compoundedOis_);
    const auto model = EvaluateCurveScriptInstrument(instrument, r);
    ASSERT_EQ(model.observations_[0].source_, "Projection");
    r.valuation_.todayFixingPolicy_ = TodayFixingPolicy_("RequireHistorical");
    r.compoundedOis_ = PrepareCompoundedOis(s, r.valuation_);
    const auto historical = EvaluateCurveScriptInstrument(instrument, r);
    ASSERT_DOUBLE_EQ(*historical.observations_[0].rate_, .9);
    ASSERT_EQ(historical.observations_[0].source_, "Historical");
    ASSERT_EQ(historical.observations_[1].source_, "Projection");
    r.valuation_.fixings_.reset(new MarketFixingSnapshot_());
    r.compoundedOis_ = PrepareCompoundedOis(s, r.valuation_);
    const auto d = Failure([&] { EvaluateCurveScriptInstrument(instrument, r); });
    ASSERT_EQ(d.code_, "MissingFixing");
    ASSERT_EQ(d.field_, "/observation_schedule/0");
    ASSERT_EQ(d.actual_, "2024-03-01");
    ASSERT_EQ(d.sourceRow_, 4);
    ASSERT_EQ(d.observationId_, "coupon");
}

TEST(CompoundedOisV2EvaluationTest, TestAllHistoricalOnRiskIsZeroWhileDiscountRiskRemains) {
    const auto s = Contract("Lookback");
    auto r = Request(s);
    r.geometry_.today_ = Date_(2024, 3, 7);
    r.geometry_.knotDates_ = {Date_(2024, 3, 7), Date_(2024, 3, 8), Date_(2024, 3, 20)};
    r.valuation_.evaluationDate_ = r.geometry_.today_;
    r.valuation_.todayFixingPolicy_ = TodayFixingPolicy_("RequireHistorical");
    r.valuation_.fixings_.reset(new MarketFixingSnapshot_(
        {{s.fixingIndex_, {{DateTime_(Date_(2024, 3, 1)), -.01}, {DateTime_(Date_(2024, 3, 5)), .02}, {DateTime_(Date_(2024, 3, 6)), .03}}}}));
    r.compoundedOis_ = PrepareCompoundedOis(s, r.valuation_);
    const auto instrument = NewCompoundedOisScriptInstrument(r.compoundedOis_);
    r.market_.routes_[0].useTargetCurve_ = false;
    r.market_.discountCurves_[CollateralType_("OIS")].reset(
        new DiscountLogDF_("passive", "USD", r.geometry_.knotDates_, {0., -.0001, -.002}, DayBasis_("ACT_365F"), LogDfScheme_("LOG_LINEAR")));
    const auto onTarget = EvaluateCurveScriptInstrument(instrument, r);
    ASSERT_DOUBLE_EQ(onTarget.nativePartials_(0, 0), 0.);
    ASSERT_DOUBLE_EQ(onTarget.nativePartials_(0, 1), 0.);
    r.market_.routes_[0].useTargetCurve_ = true;
    const auto shared = EvaluateCurveScriptInstrument(instrument, r);
    ASSERT_GT(std::abs(shared.nativePartials_(0, 0)), 1e-6);
    ASSERT_DOUBLE_EQ(shared.nativePartials_(1, 0), 0.);
}

TEST(CompoundedOisV2EvaluationTest, TestSourceCloneCapabilityAndUnmatchedWeightsIncludingSettledRows) {
    auto r = Request(Contract("ObservationShift"));
    const auto exported = CompoundedOisInternal::BuildSource(r.compoundedOis_);
    const auto loaded = LoadCalibrationInstrumentJson(DumpCalibrationInstrumentJson(exported.instrument_));
    ASSERT_EQ(loaded->Clone()->InstrumentIdentity(), exported.instrument_->InstrumentIdentity());
    const auto prepared = CurveInstrumentInternal::PrepareCurvePrice(PricingSpec(r), loaded, "Price");
    ASSERT_EQ(prepared.plan_.Capability().ruleId_, "dal.compounded-ois-cashflows/2");
    const auto legacy = CompoundedOisTest::Calibration("Shared");
    const auto legacyPlan = CurveInstrumentInternal::PrepareCurvePrice(legacy, legacy.observations_[0]->Instrument(), "Price");
    ASSERT_NE(prepared.plan_.CompilerIdentity(), legacyPlan.plan_.CompilerIdentity());
    ASSERT_EQ(exported.locations_.back().table_, "observation_schedule");
    for (const auto today : {Date_(2024, 3, 1), Date_(2024, 3, 9)}) {
        auto v = r.valuation_;
        v.evaluationDate_ = today;
        v.fixings_.reset(new MarketFixingSnapshot_());
        const auto tables = PrepareCompoundedOis(Contract("ObservationShift"), v);
        const auto c = Context(tables);
        auto definition = loaded->Definition()->Spec();
        auto& text = definition.source_.rows_.back().text_;
        const auto weight = CalibrationInternal::NumberText(4. / 360.);
        const auto position = text.find(weight);
        ASSERT_NE(position, String_::npos);
        text.replace(position, weight.size(), "0.5");
        const auto changed = NewScriptCalibrationInstrument({loaded->Id(), NewScriptInstrumentDefinition(definition), {}, loaded->Pillar()});
        ASSERT_EQ(Failure([&] { PrepareCalibrationInstrument(changed, "Price", c); }).code_, "CompoundedOisSourceMismatch");
        if (today == Date_(2024, 3, 9)) {
            const auto settled = PrepareCalibrationInstrument(loaded, "Price", c);
            ASSERT_TRUE(settled.Cashflows().empty());
            ASSERT_DOUBLE_EQ(settled.Price({}), 0.);
        }
    }
}

TEST(CompoundedOisV2EvaluationTest, TestOn2CannotBypassGeneralScriptOrMonteCarloAdmission) {
    const auto r = Request(Contract("Lookback"));
    const auto instrument = NewCompoundedOisScriptInstrument(r.compoundedOis_);
    ASSERT_TRUE(CompoundedOisInternal::ContainsOvernight(*instrument));
    ASSERT_THROW(Script::Detail::PrepareBoundScript(instrument->Product(), nullptr, r.valuation_, {}, {}), ScriptError_);
    auto c = Context(r.compoundedOis_);
    c.compoundedOis_.reset();
    ASSERT_EQ(Failure([&] { PrepareCalibrationInstrument(instrument, "Price", c); }).code_, "UnsupportedCompoundedOisContext");
    c.compoundedOis_ = r.compoundedOis_;
    CalibrationEvaluationOptions_ options;
    options.evaluation_ = CalibrationEvaluation_("ScriptMC");
    ASSERT_EQ(Failure([&] { PrepareCalibrationInstrument(instrument, "Price", c, options); }).code_, "UnsupportedCompoundedOisExecution");
}

TEST(CompoundedOisV2EvaluationTest, TestEvaluationScheduleRetainsIndependentWeightsAndSettledUnavailableRates) {
    const auto s = Contract("ObservationShift");
    auto r = Request(s);
    const auto instrument = NewCompoundedOisScriptInstrument(r.compoundedOis_);
    const auto value = EvaluateCurveScriptInstrument(instrument, r);
    ASSERT_EQ(value.observationSchedule_.size(), 3u);
    const auto& row = value.observationSchedule_[0];
    ASSERT_EQ(row.sourceKey_, "observation_schedule/0");
    ASSERT_DOUBLE_EQ(row.accrual_, 1. / 360.);
    ASSERT_DOUBLE_EQ(row.compoundingAccrual_, 4. / 360.);
    ASSERT_EQ(row.weightStart_, Date_(2024, 3, 1));
    ASSERT_EQ(row.source_, "Projection");
    ASSERT_TRUE(row.rate_);
    r.geometry_.today_ = Date_(2024, 3, 9);
    r.geometry_.knotDates_ = {Date_(2024, 3, 9), Date_(2024, 3, 12), Date_(2024, 3, 20)};
    r.valuation_.evaluationDate_ = r.geometry_.today_;
    r.valuation_.fixings_.reset(new MarketFixingSnapshot_());
    r.compoundedOis_ = PrepareCompoundedOis(s, r.valuation_);
    r.measureIds_ = {"Price"};
    const auto settled = EvaluateCurveScriptInstrument(instrument, r);
    ASSERT_DOUBLE_EQ(settled.values_[0], 0.);
    for (const auto& observation : settled.observationSchedule_) {
        ASSERT_TRUE(observation.settled_);
        ASSERT_FALSE(observation.included_);
        ASSERT_EQ(observation.source_, "Settled");
        ASSERT_FALSE(observation.rate_);
    }
    for (const auto& payment : settled.cashflows_) {
        ASSERT_FALSE(payment.amount_);
        ASSERT_FALSE(payment.discount_);
    }
}

TEST(CompoundedOisV2EvaluationTest, TestPaymentTodayAndStableTinyNegativeOrInvalidHistoricalFactors) {
    auto s = Contract("ObservationShift");
    s.coupon_ = 0.;
    for (const double rate : {1e-16, -.02, -360., 1e200}) {
        auto v = CompoundedOisTest::Valuation();
        v.evaluationDate_ = Date_(2024, 3, 8);
        v.fixings_.reset(new MarketFixingSnapshot_(
            {{s.fixingIndex_, {{DateTime_(Date_(2024, 3, 1)), rate}, {DateTime_(Date_(2024, 3, 5)), rate}, {DateTime_(Date_(2024, 3, 6)), rate}}}}));
        const auto tables = PrepareCompoundedOis(s, v);
        ASSERT_TRUE(tables->CouponPayments()[0].included_);
        const auto price = [&] {
            const auto plan = PrepareCalibrationInstrument(NewCompoundedOisScriptInstrument(tables), "Price", Context(tables));
            return plan.Price({1., 1.});
        };
        if (rate < -1. || rate > 1.) {
            ASSERT_EQ(Failure(price).code_, "CalibrationNumericalFailure");
        } else {
            const double growth = std::expm1(std::log1p(rate * 4. / 360.) + 2. * std::log1p(rate / 360.));
            const auto value = price();
            ASSERT_NEAR(value, s.notional_ * growth, std::abs(s.notional_ * growth) * 1e-12);
            ASSERT_NE(value, 0.);
        }
    }
}

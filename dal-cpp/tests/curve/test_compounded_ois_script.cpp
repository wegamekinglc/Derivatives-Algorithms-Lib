#include <gtest/gtest.h>

#include <chrono>
#include <iostream>
#include <limits>

#include <dal/curve/compoundedoisscript_internal.hpp>
#include <dal/curve/curvescript_internal.hpp>
#include <dal/curve/depositscript.hpp>
#include <dal/curve/scriptcurvequoterisk.hpp>
#include <dal/curve/xccyscript.hpp>
#include <dal/indice/indexparse.hpp>
#include <dal/math/aad/recording.hpp>
#include <dal/math/aad/statistics.hpp>
#include <dal/script/instrument_internal.hpp>
#include <dal/script/preparation.hpp>
#include <dal/time/dateutils.hpp>

using namespace Dal;

namespace {
    CompoundedOisScriptSpec_ Spec() {
        CompoundedOisScriptSpec_ s;
        s.instrumentId_ = "ois";
        s.notional_ = 100.;
        s.coupon_ = .03;
        s.direction_ = "PayFixed";
        s.currency_ = s.collateralCurrency_ = Ccy_("USD");
        s.collateralType_ = CollateralType_("OIS");
        s.discountRoute_ = "USD.OIS";
        s.projectionRoute_ = "USD.SOFR";
        s.fixingIndex_ = "USD-SOFR";
        s.observationBasis_ = s.fixedBasis_ = DayBasis_("ACT_360");
        s.calendar_ = HolidayCenterData_("weekends", {});
        const Date_ start(2024, 3, 1), monday(2024, 3, 4), end(2024, 3, 5), payment(2024, 3, 6);
        s.fixedLeg_ = {{start, end, payment, 4. / 360.}};
        s.coupons_ = {{"coupon", start, end, payment}};
        s.observations_ = {{start, monday, DateTime_(start), 3. / 360., "coupon", start, monday},
                           {monday, end, DateTime_(monday), 1. / 360., "coupon", monday, end}};
        return s;
    }
    Script::ScriptValuationSettings_ Valuation() {
        Script::ScriptValuationSettings_ v;
        v.evaluationDate_ = Date_(2024, 3, 5);
        v.fixings_.reset(new MarketFixingSnapshot_({{"USD-SOFR", {{DateTime_(Date_(2024, 3, 1)), -.012}, {DateTime_(Date_(2024, 3, 4)), .041}}}}));
        return v;
    }
    CalibrationPreparationContext_ Context(const Handle_<PreparedCompoundedOis_>& tables) {
        CalibrationPreparationContext_ c;
        c.valuation_ = tables->Valuation();
        c.currency_ = Ccy_("USD");
        c.discountRoute_ = "USD.OIS";
        c.curve_.reset(
            new GSRCurveData_("curve", *c.valuation_.evaluationDate_, "USD", {*c.valuation_.evaluationDate_, Date_(2034, 3, 6)}, {0., -.2}, {}, {}));
        c.compoundedOis_ = tables;
        return c;
    }
    Handle_<CalibrationInstrument_> EditSource(const Handle_<CalibrationInstrument_>& instrument, size_t row, const String_& text) {
        auto d = instrument->Definition()->Spec();
        d.source_.rows_[row].text_ = text;
        return NewScriptCalibrationInstrument({instrument->Id(), NewScriptInstrumentDefinition(d), {}, instrument->Pillar()});
    }
    template <class F_> CalibrationFailure_ Failure(const F_& action) {
        try {
            action();
        } catch (const CalibrationError_& e) {
            return e.Failure();
        }
        ADD_FAILURE() << "Expected owning calibration failure";
        return {};
    }
    double Price(const CompoundedOisScriptSpec_& s, const Script::ScriptValuationSettings_& v, CalibrationEvaluation_ backend) {
        const auto tables = PrepareCompoundedOis(s, v);
        CalibrationEvaluationOptions_ options;
        options.evaluation_ = backend;
        const auto plan = PrepareCalibrationInstrument(CompoundedOisInternal::BuildSource(tables).instrument_, "Price", Context(tables), options);
        return plan.Price(Vector_<>(plan.Cashflows().size(), 1.));
    }
} // namespace

TEST(CompoundedOisScriptTest, TestControlledProductMatchesIndependentScalar) {
    CalibrationCurvePayment_ flow;
    flow.program_ = {{'c', 0, 0, 1.}, {'g', 0, 0, 3. / 360.}, {'g', 1, 0, 1. / 360.}, {'p', 1, 2}, {'-', 3, 0}};
    const Vector_<> rates{-.012, .041};
    const auto fixing = [&](size_t i) { return rates[i]; };
    double product = 1.;
    for (size_t i = 0; i < rates.size(); ++i)
        product *= 1. + (i == 0 ? 3. : 1.) / 360. * rates[i];
    const auto affine = CurveScriptInternal::Evaluate<double>(flow, fixing);
    ASSERT_NEAR(affine.b_, product - 1., 1e-14);
    ASSERT_DOUBLE_EQ(affine.a_, 0.);
    ASSERT_NEAR(CurveScriptInternal::EvaluateIndependent<double>(flow, fixing), product - 1., 1e-14);
}

TEST(CompoundedOisScriptTest, TestHistoricalBoundSourceAndBothBackendsAgainstScalarOracle) {
    const auto tables = PrepareCompoundedOis(Spec(), Valuation());
    const auto exported = CompoundedOisInternal::BuildSource(tables);
    ASSERT_EQ(exported.locations_.size(), 4);
    const double floating = 100. * ((1. + 3. / 360. * -.012) * (1. + 1. / 360. * .041) - 1.);
    for (const auto backend : {CalibrationEvaluation_::Value_::RequireFast, CalibrationEvaluation_::Value_::GenericDeterministic}) {
        CalibrationEvaluationOptions_ options;
        options.evaluation_ = backend;
        const auto plan = PrepareCalibrationInstrument(exported.instrument_, "Price", Context(tables), options);
        ASSERT_EQ(plan.Capability().ruleId_, "dal.compounded-ois-cashflows/1");
        ASSERT_EQ(plan.Cashflows().size(), 2);
        ASSERT_NEAR(plan.Price({.999, .999}), (floating - 100. * .03 * 4. / 360.) * .999, 1e-12);
        const auto par = PrepareCalibrationInstrument(exported.instrument_, "ParRate", Context(tables), options);
        ASSERT_NEAR(par.AffineMeasure(par.Coefficients({.999, .999})).value_, floating / (100. * 4. / 360.), 1e-12);
    }
}

TEST(CompoundedOisScriptTest, TestTinyAccruedInterestSurvivesProductCancellation) {
    auto s = Spec();
    s.coupon_ = 0.;
    auto v = Valuation();
    v.fixings_.reset(new MarketFixingSnapshot_({{"USD-SOFR", {{DateTime_(Date_(2024, 3, 1)), 1e-15}, {DateTime_(Date_(2024, 3, 4)), 2e-15}}}}));
    const double expected = s.notional_ * std::expm1(std::log1p(3. / 360. * 1e-15) + std::log1p(1. / 360. * 2e-15));
    for (const auto backend : {CalibrationEvaluation_::Value_::RequireFast, CalibrationEvaluation_::Value_::GenericDeterministic})
        ASSERT_NEAR(Price(s, v, backend), expected, expected * 1e-12);
}

TEST(CompoundedOisScriptTest, TestHistoricalDiscountOnlyPriceRetainsAnnuityFloor) {
    auto spec = Spec();
    spec.minAbsAnnuity_ = .1;
    const auto tables = PrepareCompoundedOis(spec, Valuation());
    const auto instrument = CompoundedOisInternal::BuildSource(tables).instrument_;
    const auto plan = PrepareCalibrationInstrument(instrument, "Price", Context(tables));
    ASSERT_THROW(static_cast<void>(plan.Price({.01, .01})), CalibrationError_);
}

TEST(CompoundedOisScriptTest, TestSourceJsonCloneAndTableRowLocations) {
    const auto tables = PrepareCompoundedOis(Spec(), Valuation());
    const auto source = CompoundedOisInternal::BuildSource(tables);
    const auto json = DumpCalibrationInstrumentJson(source.instrument_);
    const auto loaded = LoadCalibrationInstrumentJson(json);
    ASSERT_EQ(DumpCalibrationInstrumentJson(loaded), json);
    ASSERT_EQ(loaded->InstrumentIdentity(), source.instrument_->InstrumentIdentity());
    ASSERT_EQ(source.instrument_->Clone()->InstrumentIdentity(), loaded->InstrumentIdentity());
    for (const auto& location : source.locations_) {
        const auto& row = loaded->Definition()->Spec().source_.rows_[location.sourceRow_ - 1];
        ASSERT_EQ(row.key_, "2024-03-06");
        if (location.table_ == "observations") {
            ASSERT_EQ(location.sourceRow_, 4);
            ASSERT_EQ(location.statement_, 1);
            ASSERT_EQ(row.text_.substr(location.offset_, 3), "(1+");
        }
    }
    ASSERT_NO_THROW(PrepareCalibrationInstrument(loaded, "Price", Context(tables)));
}

TEST(CompoundedOisScriptTest, TestMissingHistoryOwnsTableAndSourceDiagnosticsAndDoesNotCacheFailure) {
    auto valuation = Valuation();
    valuation.fixings_.reset(new MarketFixingSnapshot_());
    const auto empty = PrepareCompoundedOis(Spec(), valuation);
    const auto source = CompoundedOisInternal::BuildSource(empty);
    auto cache = std::make_shared<CalibrationPreparationCache_>();
    const auto missing = [&] { PrepareCalibrationInstrument(source.instrument_, "Price", Context(empty), {}, cache); };
    for (int i = 0; i < 2; ++i) {
        const auto failure = Failure(missing);
        ASSERT_EQ(failure.diagnostic_.code_, "MissingFixing");
        ASSERT_EQ(failure.diagnostic_.stage_, CalibrationStage_::Value_::Prepare);
        ASSERT_EQ(failure.diagnostic_.field_, "/observations/0");
        ASSERT_EQ(failure.diagnostic_.sourceRow_, 4);
        ASSERT_EQ(failure.diagnostic_.observationId_, "coupon");
        ASSERT_NE(failure.diagnostic_.expected_->find("USD-SOFR"), String_::npos);
        ASSERT_EQ(failure.diagnostic_.actual_, "2024-03-01");
        ASSERT_FALSE(failure.report_->Errors().empty());
    }
    const auto valid = PrepareCompoundedOis(Spec(), Valuation());
    const auto plan = PrepareCalibrationInstrument(source.instrument_, "Price", Context(valid), {}, cache);
    ASSERT_TRUE(std::isfinite(plan.Price({1., 1.})));
    ASSERT_EQ(Failure(missing).diagnostic_.code_, "MissingFixing");
}

TEST(CompoundedOisScriptTest, TestControlledSourceRejectsUnmatchedFactorsAndOperationsInBothBackends) {
    const auto tables = PrepareCompoundedOis(Spec(), Valuation());
    const auto source = CompoundedOisInternal::BuildSource(tables);
    const String_ fix = "FIX(" + CompoundedOisInternal::Index(Spec(), 0).Name() + ",2024-03-01)";
    const Vector_<String_> invalid = {"PAY PAYS N*((1+K*" + fix + ")-1)",
                                      "PAY PAYS N*(EXP(" + fix + ")-1)",
                                      "PAY PAYS N*(MAX(" + fix + ",0))",
                                      "IF K>0 THEN " + source.instrument_->Definition()->Spec().source_.rows_[3].text_ + " END",
                                      "PAY PAYS N*((1+0.5*" + fix + ")-1)",
                                      "PAY PAYS N*(N*(1+0.01*" + fix + "))",
                                      "PAY PAYS N*((1+0.01*FIX(IR[USD,DF,2024-03-04,2024-03-01],2024-03-01))-1)"};
    for (const auto backend : {CalibrationEvaluation_::Value_::RequireFast, CalibrationEvaluation_::Value_::GenericDeterministic}) {
        CalibrationEvaluationOptions_ options;
        options.evaluation_ = backend;
        for (const auto& text : invalid) {
            const auto changed = EditSource(source.instrument_, 3, text);
            const auto failure = Failure([&] { PrepareCalibrationInstrument(changed, "Price", Context(tables), options); });
            ASSERT_EQ(failure.diagnostic_.stage_, CalibrationStage_::Value_::Prepare);
            ASSERT_EQ(failure.diagnostic_.sourceRow_, 4);
        }
    }
}

TEST(CompoundedOisScriptTest, TestLegalRateWeightAndDateChangesStaySameSource) {
    for (const auto backend : {CalibrationEvaluation_::Value_::RequireFast, CalibrationEvaluation_::Value_::GenericDeterministic}) {
        const auto base = Price(Spec(), Valuation(), backend);
        auto s = Spec();
        s.observationBasis_ = DayBasis_("ACT_365F");
        s.observations_[0].accrual_ = 3. / 365.;
        s.observations_[1].accrual_ = 1. / 365.;
        const auto expected = 100. * ((1. + 3. / 365. * -.012) * (1. + 1. / 365. * .041) - 1. - .03 * 4. / 360.);
        ASSERT_NEAR(Price(s, Valuation(), backend), expected, 1e-12);
        ASSERT_NE(Price(s, Valuation(), backend), base);
        s = Spec();
        s.coupons_[0].start_ = s.observations_[0].start_ = Date_(2024, 3, 2);
        s.observations_[0].accrual_ = 2. / 360.;
        ASSERT_NEAR(Price(s, Valuation(), backend), 100. * ((1. + 2. / 360. * -.012) * (1. + 1. / 360. * .041) - 1. - .03 * 4. / 360.), 1e-12);
        auto v = Valuation();
        v.fixings_.reset(new MarketFixingSnapshot_({{"USD-SOFR", {{DateTime_(Date_(2024, 3, 1)), .02}, {DateTime_(Date_(2024, 3, 4)), .041}}}}));
        ASSERT_NEAR(Price(Spec(), v, backend), 100. * ((1. + 3. / 360. * .02) * (1. + 1. / 360. * .041) - 1. - .03 * 4. / 360.), 1e-12);
        s = Spec();
        s.direction_ = "ReceiveFixed";
        ASSERT_NEAR(Price(s, Valuation(), backend), -base, 1e-12);
    }
}

TEST(CompoundedOisScriptTest, TestFutureAndModelTodayExplicitlyUnsupported) {
    auto v = Valuation();
    v.evaluationDate_ = Date_(2024, 3, 4);
    const auto tables = PrepareCompoundedOis(Spec(), v);
    const auto source = CompoundedOisInternal::BuildSource(tables);
    const auto failure = Failure([&] { PrepareCalibrationInstrument(source.instrument_, "Price", Context(tables)); });
    ASSERT_EQ(failure.diagnostic_.code_, "UnsupportedCompoundedOisProjection");
    v.todayFixingPolicy_ = TodayFixingPolicy_::Value_::REQUIREHISTORICAL;
    ASSERT_NEAR(Price(Spec(), v, CalibrationEvaluation_::Value_::RequireFast),
                Price(Spec(), Valuation(), CalibrationEvaluation_::Value_::RequireFast), 1e-12);
    v.evaluationDate_ = Date_(2024, 2, 29);
    const auto future = PrepareCompoundedOis(Spec(), v);
    ASSERT_EQ(Failure([&] { PrepareCalibrationInstrument(source.instrument_, "Price", Context(future)); }).diagnostic_.code_,
              "UnsupportedCompoundedOisProjection");
}

TEST(CompoundedOisScriptTest, TestUnknownRouteDateAndFixingKeyFailAgainstTheTables) {
    const auto tables = PrepareCompoundedOis(Spec(), Valuation());
    const auto source = CompoundedOisInternal::BuildSource(tables);
    for (const auto& replacement :
         {std::make_pair("USD.SOFR", "other.route"), std::make_pair("USD-SOFR", "other.fixing"), std::make_pair("2024-03-01", "2024-03-02")}) {
        auto text = CalibrationInternal::Bytes(source.instrument_->Definition()->Spec().source_.rows_[3].text_);
        text.replace(text.find(replacement.first), std::string(replacement.first).size(), replacement.second);
        const auto changed = EditSource(source.instrument_, 3, String_(text));
        const auto failure = Failure([&] { PrepareCalibrationInstrument(changed, "Price", Context(tables)); });
        ASSERT_EQ(failure.diagnostic_.code_, "CompoundedOisSourceMismatch");
        ASSERT_EQ(failure.diagnostic_.field_, "/observations/0");
        ASSERT_EQ(failure.diagnostic_.sourceRow_, 4);
    }
}

TEST(CompoundedOisScriptTest, TestPaymentTodayAndSeparateCouponsRetainOwnFixings) {
    auto s = Spec();
    s.fixedLeg_[0].payment_ = Date_(2024, 3, 5);
    s.coupons_ = {{"first", Date_(2024, 3, 1), Date_(2024, 3, 4), Date_(2024, 3, 5)},
                  {"second", Date_(2024, 3, 4), Date_(2024, 3, 5), Date_(2024, 3, 6)}};
    s.observations_[0].couponId_ = "first";
    s.observations_[1].couponId_ = "second";
    const auto tables = PrepareCompoundedOis(s, Valuation());
    const auto plan = PrepareCalibrationInstrument(CompoundedOisInternal::BuildSource(tables).instrument_, "Price", Context(tables));
    ASSERT_EQ(plan.Cashflows().size(), 3);
    ASSERT_NEAR(*plan.Cashflows()[1].amount_, 100. * 3. / 360. * -.012, 1e-12);
    ASSERT_NEAR(*plan.Cashflows()[2].amount_, 100. / 360. * .041, 1e-12);
    ASSERT_NEAR(plan.Price({1., 1., .99}), -100. * 4. / 360. * .03 + 100. * 3. / 360. * -.012 + .99 * 100. / 360. * .041, 1e-12);
}

TEST(CompoundedOisScriptTest, TestContextIdentityOwnsTablesAndRejectsStaleValuation) {
    auto s = Spec();
    const auto tables = PrepareCompoundedOis(s, Valuation());
    const auto source = CompoundedOisInternal::BuildSource(tables);
    const auto context = Context(tables);
    const auto plan = PrepareCalibrationInstrument(source.instrument_, "Price", context);
    ASSERT_NO_THROW(plan.RequireCompatible(source.instrument_, context));
    s.calendar_->center_ = "different-frozen-calendar";
    const auto other = Context(PrepareCompoundedOis(s, Valuation()));
    const auto changed = PrepareCalibrationInstrument(source.instrument_, "Price", other);
    ASSERT_NE(plan.ContextIdentity(), changed.ContextIdentity());
    ASSERT_NE(plan.PlanIdentity(), changed.PlanIdentity());
    ASSERT_THROW(plan.RequireCompatible(source.instrument_, other), CalibrationError_);
    auto stale = context;
    stale.valuation_.fixings_.reset(new MarketFixingSnapshot_());
    ASSERT_EQ(Failure([&] { PrepareCalibrationInstrument(source.instrument_, "Price", stale); }).diagnostic_.code_, "CalibrationContextMismatch");
}

TEST(CompoundedOisScriptTest, TestEmptyFixingSeriesCannotBypassCompoundedContext) {
    for (const auto backend : {CalibrationEvaluation_::Value_::RequireFast, CalibrationEvaluation_::Value_::GenericDeterministic}) {
        for (const bool withEmpty : {false, true}) {
            Vector_<CalibrationFailure_> failures;
            {
                auto valuation = Valuation();
                auto values = valuation.fixings_->Values();
                if (withEmpty)
                    values.emplace("unused-empty", MarketFixingSnapshot_::history_t{});
                valuation.fixings_.reset(new MarketFixingSnapshot_(values));
                const auto tables = PrepareCompoundedOis(Spec(), valuation);
                const auto source = CompoundedOisInternal::BuildSource(tables);
                const auto context = Context(tables);
                const auto cache = std::make_shared<CalibrationPreparationCache_>();
                CalibrationEvaluationOptions_ options;
                options.evaluation_ = backend;
                const auto plan = PrepareCalibrationInstrument(source.instrument_, "Price", context, options, cache);
                auto same = context;
                same.valuation_.fixings_.reset(new MarketFixingSnapshot_(values));
                ASSERT_NO_THROW(plan.RequireCompatible(source.instrument_, same));
                ASSERT_EQ(PrepareCalibrationInstrument(source.instrument_, "Price", same, options).PlanIdentity(), plan.PlanIdentity());
                ASSERT_EQ(PrepareCalibrationInstrument(source.instrument_, "Price", same, options, cache).PlanIdentity(), plan.PlanIdentity());
                if (withEmpty)
                    values.erase("unused-empty");
                else
                    values.emplace("unused-empty", MarketFixingSnapshot_::history_t{});
                auto changed = context;
                changed.valuation_.fixings_.reset(new MarketFixingSnapshot_(values));
                failures = {Failure([&] { PrepareCalibrationInstrument(source.instrument_, "Price", changed, options); }),
                            Failure([&] { PrepareCalibrationInstrument(source.instrument_, "Price", changed, options, cache); }),
                            Failure([&] { plan.RequireCompatible(source.instrument_, changed); })};
                const auto rebuilt = Context(PrepareCompoundedOis(Spec(), changed.valuation_));
                const auto fresh = PrepareCalibrationInstrument(source.instrument_, "Price", rebuilt, options, cache);
                ASSERT_NE(fresh.ContextIdentity(), plan.ContextIdentity());
                ASSERT_DOUBLE_EQ(fresh.Price({1., 1.}), plan.Price({1., 1.}));
                ASSERT_EQ(PrepareCalibrationInstrument(source.instrument_, "Price", context, options, cache).PlanIdentity(), plan.PlanIdentity());
            }
            for (const auto& failure : failures) {
                ASSERT_EQ(failure.diagnostic_.code_, "CalibrationContextMismatch");
                ASSERT_TRUE(failure.report_);
                ASSERT_EQ(failure.report_->Errors().size(), 1u);
                ASSERT_EQ(failure.report_->Errors()[0].field_, failure.diagnostic_.field_);
            }
        }
    }
}

TEST(CompoundedOisScriptTest, TestMixedXccyContextIsRejectedBeforeOisValuationFingerprint) {
    const auto tables = PrepareCompoundedOis(Spec(), Valuation());
    const auto source = CompoundedOisInternal::BuildSource(tables);
    auto xccy = std::make_shared<XccyScriptContext_>();
    xccy->valuationTime_ = DateTime_(Date_(2024, 3, 5));
    xccy->pair_ = {Ccy_("USD"), Ccy_("EUR")};
    xccy->collateralCurrency_ = Ccy_("USD");
    xccy->discountRoute_ = "USD.OIS";
    CalibrationPreparationContext_ context;
    context.compoundedOis_ = tables;
    context.xccy_ = xccy;
    ASSERT_EQ(Failure([&] { PrepareCalibrationInstrument(source.instrument_, "Price", context); }).diagnostic_.code_,
              "UnsupportedCompoundedOisExecution");
}

TEST(CompoundedOisScriptTest, TestSettledSourceStillValidatedWithoutHistoricalDependency) {
    auto v = Valuation();
    v.evaluationDate_ = Date_(2024, 3, 7);
    v.fixings_.reset(new MarketFixingSnapshot_());
    const auto tables = PrepareCompoundedOis(Spec(), v);
    const auto source = CompoundedOisInternal::BuildSource(tables);
    const auto plan = PrepareCalibrationInstrument(source.instrument_, "Price", Context(tables));
    ASSERT_TRUE(plan.Cashflows().empty());
    ASSERT_DOUBLE_EQ(plan.Price({}), 0.);
    const auto par = PrepareCalibrationInstrument(source.instrument_, "ParRate", Context(tables));
    ASSERT_EQ(Failure([&] { static_cast<void>(par.AffineMeasure(par.Coefficients({}))); }).diagnostic_.code_, "InvalidCalibrationAnnuity");
    const auto changed = EditSource(source.instrument_, 3, "PAY PAYS N*(K*K)");
    ASSERT_EQ(Failure([&] { PrepareCalibrationInstrument(changed, "Price", Context(tables)); }).diagnostic_.code_, "CompoundedOisSourceMismatch");
}

TEST(CompoundedOisScriptTest, TestFactorAndCumulativeNumericalFailuresAreNeverClipped) {
    CalibrationCurvePayment_ factor;
    factor.program_ = {{'g', 0, 0, 1.}};
    for (const double rate : {-1., -2., std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        const auto fixing = [&](size_t) { return rate; };
        ASSERT_THROW(CurveScriptInternal::Evaluate<double>(factor, fixing), CalibrationError_);
        ASSERT_THROW(CurveScriptInternal::EvaluateIndependent<double>(factor, fixing), CalibrationError_);
    }
    for (const double value : {1e200, 1e-200}) {
        CalibrationCurvePayment_ flow;
        flow.program_ = {{'c', 0, 0, value}, {'p', 0, 0}};
        ASSERT_THROW(CurveScriptInternal::Evaluate<double>(flow, [](size_t) { return 0.; }), CalibrationError_);
        ASSERT_THROW(CurveScriptInternal::EvaluateIndependent<double>(flow, [](size_t) { return 0.; }), CalibrationError_);
    }
}

TEST(CompoundedOisScriptTest, TestNonFiniteBoundAmountFailsBeforePublishingPlan) {
    auto s = Spec();
    s.notional_ = 1e200;
    s.coupon_ = 1e200;
    const auto tables = PrepareCompoundedOis(s, Valuation());
    const auto source = CompoundedOisInternal::BuildSource(tables);
    const auto failure = Failure([&] { PrepareCalibrationInstrument(source.instrument_, "Price", Context(tables)); });
    ASSERT_EQ(failure.diagnostic_.code_, "CalibrationNumericalFailure");
    ASSERT_EQ(failure.diagnostic_.stage_, CalibrationStage_::Value_::Prepare);
}

TEST(CompoundedOisScriptTest, TestNoLegacyGenericOrMonteCarloAdmission) {
    const auto tables = PrepareCompoundedOis(Spec(), Valuation());
    const auto source = CompoundedOisInternal::BuildSource(tables);
    for (const auto mode : {CalibrationEvaluation_::Value_::AutoFast, CalibrationEvaluation_::Value_::RequireFast,
                            CalibrationEvaluation_::Value_::GenericDeterministic, CalibrationEvaluation_::Value_::ScriptMC}) {
        CalibrationEvaluationOptions_ options;
        options.evaluation_ = mode;
        auto c = Context(tables);
        c.compoundedOis_.reset();
        ASSERT_EQ(Failure([&] { PrepareCalibrationInstrument(source.instrument_, "Price", c, options); }).diagnostic_.code_,
                  "UnsupportedCompoundedOisContext");
    }
    CalibrationEvaluationOptions_ mc;
    mc.evaluation_ = CalibrationEvaluation_::Value_::ScriptMC;
    mc.allowMonteCarloCalibration_ = true;
    mc.sampling_ = CalibrationSamplingSpec_{16, 42, "mrg32"};
    ASSERT_EQ(Failure([&] { PrepareCalibrationInstrument(source.instrument_, "Price", Context(tables), mc); }).diagnostic_.code_,
              "UnsupportedCompoundedOisExecution");
    ASSERT_THROW(Script::Detail::PrepareBoundScript(source.instrument_->Product(), nullptr, Valuation(), {}, {}), ScriptError_);
}

namespace {
    size_t overnightParses = 0;
    CompoundedOisScriptSpec_ LongSpec(size_t count) {
        auto s = Spec();
        s.observations_.clear();
        const Date_ start(2010, 1, 4);
        auto day = start;
        for (size_t i = 0; i < count; ++i) {
            auto end = day.AddDays(1);
            while (Date::IsWeekEnd(end))
                end = end.AddDays(1);
            s.observations_.push_back({day, end, DateTime_(day), (end - day) / 360., "coupon", day, end});
            day = end;
        }
        const auto payment = day.AddDays(2);
        s.fixedLeg_ = {{start, day, payment, (day - start) / 360.}};
        s.coupons_ = {{"coupon", start, day, payment}};
        return s;
    }
    Script::ScriptValuationSettings_ LongValuation(const CompoundedOisScriptSpec_& s) {
        auto v = Valuation();
        v.evaluationDate_ = s.coupons_[0].end_;
        std::map<DateTime_, double> history;
        for (const auto& row : s.observations_)
            history.emplace(row.fixingTime_, .02);
        v.fixings_.reset(new MarketFixingSnapshot_({{s.fixingIndex_, history}}));
        return v;
    }
    double Milliseconds(std::chrono::steady_clock::time_point start) {
        return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    }
    size_t TreeDepth(const Script::Node_& node) {
        size_t result = 0;
        for (const auto& child : node.arguments_)
            result = std::max(result, TreeDepth(*child));
        return result + 1;
    }
    Handle_<CalibrationInstrument_> CountedSource(const Handle_<CalibrationInstrument_>& instrument) {
        auto text = CalibrationInternal::Bytes(instrument->Definition()->Spec().source_.rows_[3].text_);
        size_t pos = 0;
        while ((pos = text.find("IR[", pos)) != std::string::npos) {
            text.replace(pos, 2, "ONCOUNT");
            pos += 7;
        }
        return EditSource(instrument, 3, String_(text));
    }
    CurveInstrumentCalibrationSpec_ LegacySpec() {
        CurveInstrumentCalibrationSpec_ s;
        const Date_ today(2024, 3, 5), end(2025, 3, 5);
        s.geometry_.today_ = today;
        s.geometry_.ccy_ = "USD";
        s.geometry_.parameterization_ = CurveParameterization_::Value_::LOG_DISCOUNT;
        s.geometry_.knotDates_ = {today, end};
        s.valuation_ = Valuation();
        s.market_.routes_ = {{"USD.OIS"}};
        const auto instrument = NewDepositScriptInstrument(Deposit_(today, end, .025, DayBasis_("ACT_360")), {"deposit", Ccy_("USD"), "USD.OIS", 1.});
        s.observations_ = {NewCalibrationObservation("observation", instrument, "ParRate",
                                                     NewCalibrationMarketQuote("quote", .03, CalibrationValueUnit_::Parse("Rate")), 1e-13)};
        return s;
    }
} // namespace

TEST(CompoundedOisScriptTest, TestBalanced1825FactorSourceParsesOnceAndLowersLinearly) {
    Index::RegisterParser("ONCOUNT", [](const String_& text) {
        ++overnightParses;
        return Index::Parse("IR" + text.substr(7));
    });
    for (const size_t count : {30u, 365u, 1825u}) {
        const auto spec = LongSpec(count);
        const auto start = std::chrono::steady_clock::now();
        const auto tables = PrepareCompoundedOis(spec, LongValuation(spec));
        const auto source = CompoundedOisInternal::BuildSource(tables);
        overnightParses = 0;
        const auto counted = CountedSource(source.instrument_);
        ASSERT_EQ(overnightParses, count);
        const auto product = CalibrationInternal::PreparationProduct(*counted);
        ASSERT_LE(TreeDepth(*product->Events()[0][1]), 20);
        const auto plan = PrepareCalibrationInstrument(counted, "Price", Context(tables));
        const auto prepareMs = Milliseconds(start);
        CalibrationEvaluationOptions_ options;
        options.evaluation_ = CalibrationEvaluation_::Value_::GenericDeterministic;
        const auto generic = PrepareCalibrationInstrument(counted, "Price", Context(tables), options);
        ASSERT_EQ(overnightParses, count);
        ASSERT_EQ(CalibrationInternal::PreparationProduct(*counted).get(), product.get());
        const auto& descriptor = *std::get<CalibrationCurveFamily_>(plan.Descriptor());
        const auto& flow = descriptor.payments_[1];
        ASSERT_EQ(flow.program_.size(), 2 * count + 4);
        ASSERT_EQ(flow.locations_.size(), count);
        double reference = 1.;
        for (const auto& row : spec.observations_)
            reference *= 1. + row.accrual_ * .02;
        const auto expected = spec.notional_ * (reference - 1. - spec.coupon_ * spec.fixedLeg_[0].accrual_);
        ASSERT_NEAR(plan.Price({1., 1.}), expected, 1e-10);
        ASSERT_NEAR(generic.Price({1., 1.}), expected, 1e-10);
        const auto evalStart = std::chrono::steady_clock::now();
        double checksum = 0.;
        for (int i = 0; i < 100; ++i)
            checksum += CurveScriptInternal::EvaluateIndependent<double>(flow, [](size_t) { return .02; });
        ASSERT_NEAR(checksum, 100. * spec.notional_ * (reference - 1.), 1e-8);
        std::cout << "OIS D=" << count << " prepare_ms=" << prepareMs << " kernel100_ms=" << Milliseconds(evalStart)
                  << " instructions=" << flow.program_.size() << " payload=" << flow.program_.size() * sizeof(CalibrationCurveInstruction_)
                  << " parses=" << overnightParses << '\n';
        ASSERT_EQ(DumpCalibrationInstrumentJson(LoadCalibrationInstrumentJson(DumpCalibrationInstrumentJson(source.instrument_))),
                  DumpCalibrationInstrumentJson(source.instrument_));
    }
}

TEST(CompoundedOisScriptTest, TestControlledActiveKernelAgainstAnalyticProductAndTapeCleanup) {
    const auto tables = PrepareCompoundedOis(Spec(), Valuation());
    const auto plan = PrepareCalibrationInstrument(CompoundedOisInternal::BuildSource(tables).instrument_, "Price", Context(tables));
    const auto& flow = std::get<CalibrationCurveFamily_>(plan.Descriptor())->payments_[1];
    {
        AAD::RecordingScope_ recording;
        AAD::Number_ first, second;
        recording.RegisterInput(first, -.012);
        recording.RegisterInput(second, .041);
        recording.StartRecording();
        const auto value = CurveScriptInternal::EvaluateIndependent<AAD::Number_>(flow, [&](size_t i) { return i == 0 ? first : second; });
        recording.FinishRecording();
        recording.ClearAdjoints();
        AAD::Adjoint(value) = 1.;
        recording.Reverse();
        ASSERT_NEAR(AAD::AdjointValue(first), 100. * 3. / 360. * (1. + .041 / 360.), 1e-12);
        ASSERT_NEAR(AAD::AdjointValue(second), 100. / 360. * (1. - .012 * 3. / 360.), 1e-12);
    }
    ASSERT_EQ(AAD::MeasureTape(*AAD::Tape()).nodes_, 0);
}

TEST(CompoundedOisScriptTest, TestExtraParameterIsRejectedBeforeCurvePlanConstruction) {
    const auto tables = PrepareCompoundedOis(Spec(), Valuation());
    const auto source = CompoundedOisInternal::BuildSource(tables);
    auto definition = source.instrument_->Definition()->Spec();
    definition.source_.rows_ = {definition.source_.rows_[0],
                                definition.source_.rows_[1],
                                {ScriptSourceRowKind_::Value_::Definition, "ALPHA", "1"},
                                definition.source_.rows_[2],
                                definition.source_.rows_[3]};
    definition.parameters_.push_back(
        {"ALPHA", ScriptParameterType_::Value_::Real, CalibrationValueUnit_::Parse("YearFraction"), ScriptParameterRole_::Value_::Accrual, false});
    definition.measures_ = {definition.measures_[0]};
    const auto instrument = NewScriptCalibrationInstrument({"ois", NewScriptInstrumentDefinition(definition), {}, source.instrument_->Pillar()});
    ASSERT_EQ(Failure([&] { PrepareCalibrationInstrument(instrument, "Price", Context(tables)); }).diagnostic_.code_, "CompoundedOisSourceMismatch");
}

TEST(CompoundedOisScriptTest, TestP1SnapshotAndPerPvRejectNewCapabilityAndPreparationHasZeroSolveCalls) {
    const auto source = CompoundedOisInternal::BuildSource(PrepareCompoundedOis(Spec(), Valuation()));
    const auto legacy = LegacySpec();
    const auto fit = CalibrateCurveInstruments(legacy);
    const ScriptCurveQuoteRiskAccuracy_ accuracy{{1e-12}, 1e-10, 1e-12};
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, accuracy);
    ASSERT_TRUE(snapshot.MappingAvailable());
    const auto risk = ValueCalibrationInstrumentWithQuoteRisk(source.instrument_, snapshot, legacy);
    ASSERT_FALSE(risk.Available());
    ASSERT_EQ(risk.Diagnostics()[0].code_, "ScriptCurveQuoteRiskUnsupportedCapability");
    auto unsupported = legacy;
    unsupported.observations_ = {NewCalibrationObservation("observation", source.instrument_, "ParRate",
                                                           NewCalibrationMarketQuote("quote", .03, CalibrationValueUnit_::Parse("Rate")), 1e-13)};
    auto fabricated = fit;
    fabricated.resolvedSpec_ = std::make_shared<const CurveInstrumentCalibrationSpec_>(unsupported);
    const auto rejected = NewScriptCurveQuoteRiskSnapshot(fabricated, accuracy);
    ASSERT_FALSE(rejected.MappingAvailable());
    ASSERT_EQ(rejected.Diagnostics()[0].code_, "ScriptCurveQuoteRiskUnsupportedCapability");
    const auto failure = Failure([&] { CalibrateCurveInstruments(unsupported); });
    ASSERT_EQ(failure.diagnostic_.stage_, CalibrationStage_::Value_::Prepare);
    ASSERT_EQ(failure.report_->Data().summary_.evaluations_, 0);
    ASSERT_EQ(failure.report_->Data().summary_.jacobianEvaluations_, 0);
}

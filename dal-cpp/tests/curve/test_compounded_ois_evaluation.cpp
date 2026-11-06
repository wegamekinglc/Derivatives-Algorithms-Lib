#include <gtest/gtest.h>

#include <dal/curve/curveinstrumentcalibration.hpp>
#include <dal/curve/curvescript.hpp>
#include <dal/curve/curvescriptevaluation.hpp>
#include <dal/curve/scriptcurvequoterisk.hpp>
#include <dal/curve/ycconst.hpp>
#include <dal/curve/yclogdf.hpp>
#include <dal/curve/ycpwlf.hpp>
#include <dal/math/aad/recording.hpp>

using namespace Dal;

namespace {
    CompoundedOisScriptSpec_ OisSpec() {
        CompoundedOisScriptSpec_ s;
        s.instrumentId_ = "two-coupon-ois";
        s.notional_ = 1.;
        s.coupon_ = .023;
        s.direction_ = "PayFixed";
        s.currency_ = s.collateralCurrency_ = Ccy_("USD");
        s.collateralType_ = CollateralType_("OIS");
        s.discountRoute_ = "USD.OIS";
        s.projectionRoute_ = "USD.SOFR";
        s.fixingIndex_ = "USD-SOFR";
        s.observationBasis_ = DayBasis_("ACT_360");
        s.fixedBasis_ = DayBasis_("ACT_365F");
        s.calendar_ = HolidayCenterData_("explicit-holiday", {Date_(2024, 3, 4)});
        const Date_ a(2024, 2, 28), b(2024, 2, 29), c(2024, 3, 1), d(2024, 3, 3), e(2024, 3, 5), f(2024, 3, 6), p(2024, 3, 8);
        s.fixedLeg_ = {{a, d, e, 4. / 365.}, {d, f, p, 3. / 365.}};
        s.coupons_ = {{"first", a, d, e}, {"second", d, f, p}};
        s.observations_ = {{a, b, DateTime_(a), 1. / 360., "first", a, b},
                           {b, c, DateTime_(b), 1. / 360., "first", b, c},
                           {c, d, DateTime_(c), 2. / 360., "first", c, e},
                           {d, e, DateTime_(c), 2. / 360., "second", c, e},
                           {e, f, DateTime_(e), 1. / 360., "second", e, f}};
        return s;
    }
    Script::ScriptValuationSettings_ OisValuation(Date_ today = Date_(2024, 2, 27)) {
        Script::ScriptValuationSettings_ v;
        v.evaluationDate_ = today;
        v.fixings_.reset(new MarketFixingSnapshot_({{"USD-SOFR",
                                                     {{DateTime_(Date_(2024, 2, 28)), -.012},
                                                      {DateTime_(Date_(2024, 2, 29)), .018},
                                                      {DateTime_(Date_(2024, 3, 1)), .021},
                                                      {DateTime_(Date_(2024, 3, 5)), -.004}}}}));
        return v;
    }
    CurveInstrumentCalibrationSpec_ OisCalibration(const CompoundedOisScriptSpec_& contract, const Script::ScriptValuationSettings_& valuation) {
        CurveInstrumentCalibrationSpec_ spec;
        const auto today = *valuation.evaluationDate_;
        spec.geometry_.today_ = today;
        spec.geometry_.ccy_ = "USD";
        spec.geometry_.parameterization_ = CurveParameterization_::Value_::LOG_DISCOUNT;
        spec.geometry_.knotDates_ = {today, today.AddDays(4), today.AddDays(20)};
        spec.geometry_.calibrateDiscountCurve_ = false;
        spec.geometry_.targetOvernightRoute_ = "USD.SOFR";
        spec.market_.routes_ = {{"USD.OIS", false, CollateralType_("OIS"), {}, false, true}, {"USD.SOFR", true, CollateralType_("OIS"), {}, true}};
        spec.valuation_ = valuation;
        spec.compoundedOis_ = {PrepareCompoundedOis(contract, valuation)};
        const auto instrument = NewCompoundedOisScriptInstrument(spec.compoundedOis_[0]);
        spec.observations_ = {NewCalibrationObservation("pv", instrument, "Price",
                                                        NewCalibrationMarketQuote("pv", 0., CalibrationValueUnit_::Parse("Price:USD")), 1e-10),
                              NewCalibrationObservation("par", instrument, "ParRate",
                                                        NewCalibrationMarketQuote("par", .03, CalibrationValueUnit_::Parse("Rate")), 1e-10)};
        return spec;
    }
    double OracleLogDf(Date_ date, Date_ today, const Vector_<>& theta) {
        const double days = date - today;
        return days <= 4. ? theta[0] * days / 4. : theta[0] + (theta[1] - theta[0]) * (days - 4.) / 16.;
    }
    CurveScriptEvaluationRequest_ Request(const CurveInstrumentCalibrationSpec_& spec) {
        CurveScriptEvaluationRequest_ result;
        result.geometry_ = spec.geometry_;
        result.market_ = spec.market_;
        result.valuation_ = spec.valuation_;
        result.compoundedOis_ = spec.compoundedOis_[0];
        result.parameters_ = {-.0003, -.0016};
        return result;
    }
    Vector_<> FutureOracle(const Vector_<>& theta) {
        const Date_ today(2024, 2, 27), a(2024, 2, 28), b(2024, 2, 29), c(2024, 3, 1), e(2024, 3, 5), f(2024, 3, 6), p(2024, 3, 8);
        const auto rate = [&](Date_ start, Date_ end) {
            return std::expm1(OracleLogDf(start, today, theta) - OracleLogDf(end, today, theta)) / ((end - start) / 360.);
        };
        const double first = (1. + rate(a, b) / 360.) * (1. + rate(b, c) / 360.) * (1. + 2. * rate(c, e) / 360.) - 1.;
        const double second = (1. + 2. * rate(c, e) / 360.) * (1. + rate(e, f) / 360.) - 1.;
        const double df1 = std::exp(OracleLogDf(e, today, theta)), df2 = std::exp(OracleLogDf(p, today, theta));
        const double floating = first * df1 + second * df2, annuity = 4. / 365. * df1 + 3. / 365. * df2;
        return {floating - .023 * annuity, floating / annuity};
    }
    Vector_<> BoundaryOracle(Date_ today, bool historicalToday, const Vector_<>& projection, const Vector_<>& discount) {
        const Date_ a(2024, 2, 28), b(2024, 2, 29), c(2024, 3, 1), e(2024, 3, 5), f(2024, 3, 6), p(2024, 3, 8);
        const auto rate = [&](Date_ start, Date_ end, double historical) {
            if (start < today || (start == today && historicalToday))
                return historical;
            return std::expm1(OracleLogDf(start, today, projection) - OracleLogDf(end, today, projection)) / ((end - start) / 360.);
        };
        double floating = 0., annuity = 0.;
        if (today <= e) {
            const double df = std::exp(OracleLogDf(e, today, discount));
            floating += ((1. + rate(a, b, -.012) / 360.) * (1. + rate(b, c, .018) / 360.) * (1. + 2. * rate(c, e, .021) / 360.) - 1.) * df;
            annuity += 4. / 365. * df;
        }
        if (today <= p) {
            const double df = std::exp(OracleLogDf(p, today, discount));
            floating += ((1. + 2. * rate(c, e, .021) / 360.) * (1. + rate(e, f, -.004) / 360.) - 1.) * df;
            annuity += 3. / 365. * df;
        }
        return {floating - .023 * annuity, floating / annuity};
    }
    template <class F_> void AssertNativePartials(const Matrix_<>& partials, const Vector_<>& parameters, const F_& oracle) {
        for (int j = 0; j < 2; ++j) {
            Vector_<> previous;
            for (const double h : {1e-4, 5e-5}) {
                auto up = parameters, down = parameters;
                up[j] += h;
                down[j] -= h;
                const auto plus = oracle(up), minus = oracle(down);
                Vector_<> fd(2);
                for (int i = 0; i < 2; ++i) {
                    fd[i] = (plus[i] - minus[i]) / (2. * h);
                    if (!previous.empty())
                        ASSERT_NEAR(fd[i], previous[i], 1e-7 + 1e-5 * std::abs(fd[i]));
                    ASSERT_NEAR(partials(i, j), fd[i], 1e-7 + 1e-5 * std::abs(fd[i]));
                }
                previous = fd;
            }
        }
    }
    void AssertZeroPartials(const Matrix_<>& partials) {
        for (int i = 0; i < 2; ++i)
            for (int j = 0; j < 2; ++j)
                ASSERT_DOUBLE_EQ(partials(i, j), 0.);
    }
    void AssertBoundaryBackends(const CurveInstrumentCalibrationSpec_& spec, bool historicalToday, bool shared, const Vector_<>& background) {
        const auto today = spec.geometry_.today_;
        auto request = Request(spec);
        const auto oracle = [&](const Vector_<>& theta) { return BoundaryOracle(today, historicalToday, theta, shared ? theta : background); };
        const auto expected = oracle(request.parameters_);
        for (const auto backend : {CalibrationEvaluation_("AutoFast"), CalibrationEvaluation_("GenericDeterministic")}) {
            request.evaluation_ = backend;
            request.derivativeMethod_ = CalibrationDerivative_::Value_::AAD;
            const auto result = EvaluateCurveScriptInstrument(spec.observations_[0]->Instrument(), request);
            for (int i = 0; i < 2; ++i)
                ASSERT_NEAR(result.values_[i], expected[i], 1e-10);
            ASSERT_NO_FATAL_FAILURE(AssertNativePartials(result.nativePartials_, request.parameters_, oracle));
            if (!shared && today > Date_(2024, 3, 5))
                ASSERT_NO_FATAL_FAILURE(AssertZeroPartials(result.nativePartials_));
        }
    }
} // namespace

TEST(CompoundedOisEvaluationTest, TestExplicitOvernightTargetHasNoTenorFallback) {
    CurveInstrumentCalibrationSpec_ spec;
    spec.geometry_.today_ = Date_(2024, 3, 1);
    spec.geometry_.ccy_ = "USD";
    spec.geometry_.parameterization_ = CurveParameterization_::Value_::LOG_DISCOUNT;
    spec.geometry_.knotDates_ = {Date_(2024, 3, 1), Date_(2024, 3, 11)};
    spec.geometry_.calibrateDiscountCurve_ = false;
    spec.geometry_.targetOvernightRoute_ = "USD.SOFR";
    spec.market_.routes_ = {{"USD.SOFR", true, CollateralType_("OIS"), {}, true}};
    spec.valuation_.evaluationDate_ = spec.geometry_.today_;
    spec.valuation_.fixings_.reset(new MarketFixingSnapshot_());
    const auto geometry = std::make_shared<const CandidateCurveGeometry_>(spec);
    const CandidateCurveProvider_<double> provider(geometry, {-.001});
    ASSERT_NEAR(provider.Forward("USD.SOFR", Date_(2024, 3, 1), Date_(2024, 3, 4), DayBasis_("ACT_360")), std::expm1(.0003) / (3. / 360.), 1e-12);
    ASSERT_THROW(static_cast<void>(provider.Forward("USD.sofr", Date_(2024, 3, 1), Date_(2024, 3, 4), DayBasis_("ACT_360"))), CalibrationError_);
}

TEST(CompoundedOisEvaluationTest, TestFutureCashflowsSharePriceParAndNativeAad) {
    const auto spec = OisCalibration(OisSpec(), OisValuation());
    CurveInstrumentCalibrationOptions_ options;
    options.jacobianMode_ = CalibrationDerivative_::Value_::AAD;
    const Vector_<> theta{-.0003, -.0016};
    const auto expected = FutureOracle(theta);
    const auto result = PreparedCurveInstruments_(spec, options).Evaluate(theta);
    ASSERT_NEAR(result.modelMeasures_[0], expected[0], 1e-10);
    ASSERT_NEAR(result.modelMeasures_[1], expected[1], 1e-10);
    ASSERT_NO_FATAL_FAILURE(AssertNativePartials(result.quoteJacobian_, theta, FutureOracle));
}

TEST(CompoundedOisEvaluationTest, TestDirectEvaluationOwnsMeasuresFlowsAndSourcesWithoutQuotes) {
    const auto spec = OisCalibration(OisSpec(), OisValuation());
    CurveScriptEvaluationRequest_ request;
    request.geometry_ = spec.geometry_;
    request.market_ = spec.market_;
    request.valuation_ = spec.valuation_;
    request.compoundedOis_ = spec.compoundedOis_[0];
    request.parameters_ = {-.0003, -.0016};
    request.derivativeMethod_ = CalibrationDerivative_::Value_::AAD;
    const auto result = EvaluateCurveScriptInstrument(spec.observations_[0]->Instrument(), request);
    const auto expected = FutureOracle(request.parameters_);
    ASSERT_EQ(result.measureIds_, (Vector_<String_>{"Price", "ParRate"}));
    ASSERT_NEAR(result.values_[0], expected[0], 1e-10);
    ASSERT_NEAR(result.values_[1], expected[1], 1e-10);
    ASSERT_EQ(result.nativePartials_.Rows(), 2);
    ASSERT_EQ(result.nativePartials_.Cols(), 2);
    ASSERT_EQ(result.cashflows_.size(), 4u);
    ASSERT_EQ(result.observations_.size(), 5u);
    double pv = 0.;
    for (const auto& row : result.cashflows_) {
        ASSERT_TRUE(row.included_);
        ASSERT_TRUE(row.amount_);
        ASSERT_TRUE(row.discount_);
        ASSERT_NEAR(row.pv_, *row.amount_ * *row.discount_, 1e-14);
        pv += row.pv_;
    }
    ASSERT_NEAR(pv, result.values_[0], 1e-14);
    for (const auto& row : result.observations_) {
        ASSERT_EQ(row.source_, "Projection");
        ASSERT_TRUE(row.rate_);
        ASSERT_GT(row.sourceRow_, 0u);
    }
    ASSERT_TRUE(result.report_->Axes().quotes_.empty());
    for (const auto& row : result.report_->Data().observations_)
        ASSERT_FALSE(row.quoteId_);
    request.derivativeMethod_ = CalibrationDerivative_::Value_::None;
    const auto passive = EvaluateCurveScriptInstrument(spec.observations_[0]->Instrument(), request);
    ASSERT_EQ(passive.nativePartials_.Rows(), 0);
    ASSERT_EQ(passive.nativePartials_.Cols(), 0);
}

TEST(CompoundedOisEvaluationTest, TestPreparedProviderFreezesWritableBackgroundCurveAliases) {
    auto spec = OisCalibration(OisSpec(), OisValuation());
    spec.market_.routes_[0].useTargetCurve_ = false;
    auto background = std::make_shared<DiscountLogDF_>("background", "USD", spec.geometry_.knotDates_, Vector_<>{0., -.0004, -.002},
                                                       DayBasis_("ACT_365F"), LogDfScheme_("LOG_LINEAR"));
    spec.market_.discountCurves_[CollateralType_("OIS")] = Handle_<DiscountCurve_>(background);
    const auto geometry = std::make_shared<const CandidateCurveGeometry_>(spec);
    const CandidateCurveProvider_<double> provider(geometry, {-.0003, -.0016});
    const auto before = provider.Discount("USD.OIS", spec.geometry_.today_, Date_(2024, 3, 8));
    const Vector_<> change{.1, .1};
    background->ApplyDX(change.begin(), 1.);
    ASSERT_DOUBLE_EQ(provider.Discount("USD.OIS", spec.geometry_.today_, Date_(2024, 3, 8)), before);
}

TEST(CompoundedOisEvaluationTest, TestBackgroundHorizonRejectsExtrapolatedOisPayments) {
    auto spec = OisCalibration(OisSpec(), OisValuation());
    spec.market_.routes_[0].useTargetCurve_ = false;
    const auto today = spec.geometry_.today_;
    spec.market_.discountCurves_[CollateralType_("OIS")].reset(
        new DiscountLogDF_("short", "USD", {today, today.AddDays(2)}, {0., -.0002}, DayBasis_("ACT_365F"), LogDfScheme_("LOG_LINEAR")));
    ASSERT_THROW(static_cast<void>(PreparedCurveInstruments_(spec).Evaluate({-.0003, -.0016})), CalibrationError_);
}

TEST(CompoundedOisEvaluationTest, TestOvernightTargetRejectsAmbiguousGeometryAndCollateral) {
    const auto original = OisCalibration(OisSpec(), OisValuation());
    {
        auto spec = original;
        spec.geometry_.calibrateDiscountCurve_ = true;
        ASSERT_THROW(CandidateCurveGeometry_{spec}, CalibrationError_);
    }
    {
        auto spec = original;
        spec.geometry_.targetTenor_ = PeriodLength_("3M");
        ASSERT_THROW(CandidateCurveGeometry_{spec}, CalibrationError_);
    }
    {
        auto spec = original;
        spec.market_.routes_[0].collateral_ = CollateralType_("GC");
        ASSERT_THROW(CandidateCurveGeometry_{spec}, CalibrationError_);
    }
}

TEST(CompoundedOisEvaluationTest, TestPreparedSpecAndDirectResultIsolateWritableInputs) {
    auto spec = OisCalibration(OisSpec(), OisValuation());
    spec.market_.routes_[0].useTargetCurve_ = false;
    auto background = std::make_shared<DiscountLogDF_>("background", "USD", spec.geometry_.knotDates_, Vector_<>{0., -.0004, -.002},
                                                       DayBasis_("ACT_365F"), LogDfScheme_("LOG_LINEAR"));
    spec.market_.discountCurves_[CollateralType_("OIS")] = Handle_<DiscountCurve_>(background);
    auto alias = std::shared_ptr<CalibrationInstrument_>(spec.observations_[0]->Instrument()->Clone().release());
    spec.observations_[0] = NewCalibrationObservation("pv", Handle_<CalibrationInstrument_>(alias), "Price", spec.observations_[0]->Quote(), 1e-10);
    const PreparedCurveInstruments_ prepared(spec);
    CurveScriptEvaluationRequest_ request;
    request.geometry_ = spec.geometry_;
    request.market_ = spec.market_;
    request.valuation_ = spec.valuation_;
    request.compoundedOis_ = spec.compoundedOis_[0];
    request.parameters_ = {-.0003, -.0016};
    const auto direct = EvaluateCurveScriptInstrument(Handle_<CalibrationInstrument_>(alias), request);
    const auto before = direct.instrument_->InstrumentIdentity();
    const auto copied = prepared.Describe().resolvedSpec_;
    const auto beforeDf = (*copied->market_.discountCurves_.at(CollateralType_("OIS")))(spec.geometry_.today_, Date_(2024, 3, 8));
    const Vector_<> change{.1, .1};
    background->ApplyDX(change.begin(), 1.);
    spec.observations_.clear();
    alias.reset();
    ASSERT_DOUBLE_EQ((*copied->market_.discountCurves_.at(CollateralType_("OIS")))(spec.geometry_.today_, Date_(2024, 3, 8)), beforeDf);
    ASSERT_EQ(direct.instrument_->InstrumentIdentity(), before);
    ASSERT_EQ(prepared.Describe().axes_.observations_[0]->Instrument()->InstrumentIdentity(), before);
}

TEST(CompoundedOisEvaluationTest, TestPriceEnforcesLiveAnnuityFloorAndSettledPriceIsZero) {
    auto contract = OisSpec();
    contract.minAbsAnnuity_ = .1;
    const auto spec = OisCalibration(contract, OisValuation());
    auto request = Request(spec);
    request.measureIds_ = {"Price"};
    ASSERT_THROW(EvaluateCurveScriptInstrument(spec.observations_[0]->Instrument(), request), CalibrationError_);
    auto valuation = OisValuation(Date_(2024, 3, 9));
    valuation.fixings_.reset(new MarketFixingSnapshot_());
    const auto settled = OisCalibration(contract, valuation);
    request = Request(settled);
    request.measureIds_ = {"Price"};
    const auto result = EvaluateCurveScriptInstrument(settled.observations_[0]->Instrument(), request);
    ASSERT_DOUBLE_EQ(result.values_[0], 0.);
    for (const auto& flow : result.cashflows_) {
        ASSERT_TRUE(flow.settled_);
        ASSERT_FALSE(flow.amount_);
        ASSERT_DOUBLE_EQ(flow.pv_, 0.);
    }
    request.measureIds_ = {"ParRate"};
    ASSERT_THROW(EvaluateCurveScriptInstrument(settled.observations_[0]->Instrument(), request), CalibrationError_);
}

TEST(CompoundedOisEvaluationTest, TestIndependentOracleAcrossFixingPaymentAndNativeGradientBoundaries) {
    for (const auto today : {Date_(2024, 2, 27), Date_(2024, 2, 28), Date_(2024, 3, 1), Date_(2024, 3, 5), Date_(2024, 3, 7), Date_(2024, 3, 8)})
        for (const bool historicalToday : {false, true})
            for (const bool shared : {false, true}) {
                SCOPED_TRACE(Date::ToString(today));
                auto valuation = OisValuation(today);
                valuation.todayFixingPolicy_ = TodayFixingPolicy_(historicalToday ? "RequireHistorical" : "Model");
                auto spec = OisCalibration(OisSpec(), valuation);
                const Vector_<> background{-.0002, -.0014};
                if (!shared) {
                    spec.market_.routes_[0].useTargetCurve_ = false;
                    spec.market_.discountCurves_[CollateralType_("OIS")].reset(new DiscountLogDF_("passive", "USD", spec.geometry_.knotDates_,
                                                                                                  {0., background[0], background[1]},
                                                                                                  DayBasis_("ACT_365F"), LogDfScheme_("LOG_LINEAR")));
                }
                ASSERT_NO_FATAL_FAILURE(AssertBoundaryBackends(spec, historicalToday, shared, background));
            }
}

TEST(CompoundedOisEvaluationTest, TestFutureSnapshotValuesAreIgnoredAndHistoricalFixingsAffectOnlyDependentCoupons) {
    for (const auto today : {Date_(2024, 2, 27), Date_(2024, 3, 1)}) {
        const auto original = OisCalibration(OisSpec(), OisValuation(today));
        const auto reference = EvaluateCurveScriptInstrument(original.observations_[0]->Instrument(), Request(original));
        auto valuation = OisValuation(today);
        auto values = valuation.fixings_->Values();
        values["USD-SOFR"][DateTime_(Date_(2024, 2, 28))] += .1;
        valuation.fixings_.reset(new MarketFixingSnapshot_(values));
        const auto changed = OisCalibration(OisSpec(), valuation);
        const auto result = EvaluateCurveScriptInstrument(changed.observations_[0]->Instrument(), Request(changed));
        ASSERT_NE(result.contextIdentity_, reference.contextIdentity_);
        if (today == Date_(2024, 2, 27)) {
            ASSERT_EQ(result.values_, reference.values_);
        } else {
            ASSERT_NE(result.cashflows_[2].amount_, reference.cashflows_[2].amount_);
            ASSERT_EQ(result.cashflows_[3].amount_, reference.cashflows_[3].amount_);
            ASSERT_EQ(result.cashflows_[0].amount_, reference.cashflows_[0].amount_);
        }
    }
}

TEST(CompoundedOisEvaluationTest, TestDirectionNotionalAndFiniteDifferenceRemainInNativeUnits) {
    const auto original = OisCalibration(OisSpec(), OisValuation());
    auto request = Request(original);
    request.derivativeMethod_ = CalibrationDerivative_::Value_::AAD;
    const auto unit = EvaluateCurveScriptInstrument(original.observations_[0]->Instrument(), request);
    for (const auto direction : {String_("PayFixed"), String_("ReceiveFixed")}) {
        auto contract = OisSpec();
        contract.direction_ = direction;
        contract.notional_ = 1e6;
        const auto spec = OisCalibration(contract, OisValuation());
        request = Request(spec);
        request.derivativeMethod_ = CalibrationDerivative_::Value_::FiniteDifference;
        request.differenceStep_ = 5e-5;
        const auto result = EvaluateCurveScriptInstrument(spec.observations_[0]->Instrument(), request);
        const double sign = direction == "PayFixed" ? 1. : -1.;
        ASSERT_NEAR(result.values_[0], sign * 1e6 * unit.values_[0], 1e-4);
        ASSERT_NEAR(result.values_[1], unit.values_[1], 1e-10);
        for (int j = 0; j < 2; ++j) {
            ASSERT_NEAR(result.nativePartials_(0, j) / (sign * 1e6), unit.nativePartials_(0, j), 1e-7 + 1e-5 * std::abs(unit.nativePartials_(0, j)));
            ASSERT_NEAR(result.nativePartials_(1, j), unit.nativePartials_(1, j), 1e-7 + 1e-5 * std::abs(unit.nativePartials_(1, j)));
        }
    }
}

TEST(CompoundedOisEvaluationTest, TestDiscountTargetUsesExplicitPassiveOvernightRouteAndContextIdentity) {
    auto spec = OisCalibration(OisSpec(), OisValuation());
    spec.geometry_.targetOvernightRoute_.clear();
    spec.geometry_.calibrateDiscountCurve_ = true;
    spec.market_.overnightCurves_["USD.SOFR"].reset(
        new DiscountLogDF_("passive-on", "USD", spec.geometry_.knotDates_, {0., -.0002, -.0014}, DayBasis_("ACT_365F"), LogDfScheme_("LOG_LINEAR")));
    auto request = Request(spec);
    request.derivativeMethod_ = CalibrationDerivative_::Value_::AAD;
    const auto result = EvaluateCurveScriptInstrument(spec.observations_[0]->Instrument(), request);
    const auto expected = BoundaryOracle(spec.geometry_.today_, false, {-.0002, -.0014}, request.parameters_);
    for (int i = 0; i < 2; ++i)
        ASSERT_NEAR(result.values_[i], expected[i], 1e-10);
    request.market_.overnightCurves_["USD.SOFR"].reset(
        new DiscountLogDF_("changed-on", "USD", spec.geometry_.knotDates_, {0., -.0001, -.0012}, DayBasis_("ACT_365F"), LogDfScheme_("LOG_LINEAR")));
    const auto changed = EvaluateCurveScriptInstrument(spec.observations_[0]->Instrument(), request);
    ASSERT_NE(changed.contextIdentity_, result.contextIdentity_);
    ASSERT_NE(changed.values_[0], result.values_[0]);
    request.market_.overnightCurves_.clear();
    ASSERT_THROW(EvaluateCurveScriptInstrument(spec.observations_[0]->Instrument(), request), CalibrationError_);
}

TEST(CompoundedOisEvaluationTest, TestMissingHistoryAndFailureReportsSurviveInputsAndRejectBeforeSolve) {
    Handle_<CalibrationReport_> report;
    {
        auto valuation = OisValuation(Date_(2024, 3, 1));
        valuation.fixings_.reset(new MarketFixingSnapshot_());
        const auto spec = OisCalibration(OisSpec(), valuation);
        try {
            CalibrateCurveInstruments(spec);
            FAIL() << "Missing history was accepted";
        } catch (const CalibrationError_& error) {
            ASSERT_EQ(error.Failure().diagnostic_.stage_, CalibrationStage_::Value_::Prepare);
            report = error.Failure().report_;
        }
    }
    ASSERT_TRUE(report);
    ASSERT_EQ(report->Data().summary_.evaluations_, 0);
    ASSERT_EQ(report->Data().summary_.jacobianEvaluations_, 0);
    ASSERT_FALSE(report->Errors().empty());
}

TEST(CompoundedOisEvaluationTest, TestAadNestedRejectionModeRestorationAndExceptionReentry) {
    const auto spec = OisCalibration(OisSpec(), OisValuation());
    auto request = Request(spec);
    request.derivativeMethod_ = CalibrationDerivative_::Value_::AAD;
    for (const bool multi : {false, true}) {
        const auto mode = AAD::SetNumResultsForAAD(multi, multi ? 2 : 1);
        {
            AAD::RecordingScope_ recording;
            AAD::Number_ input;
            recording.RegisterInput(input, 2.);
            recording.StartRecording();
            ASSERT_THROW(EvaluateCurveScriptInstrument(spec.observations_[0]->Instrument(), request), CalibrationError_);
            const AAD::Number_ output = input * input;
            ASSERT_DOUBLE_EQ(static_cast<double>(output), 4.);
            recording.FinishRecording();
            recording.Close();
        }
        const auto result = EvaluateCurveScriptInstrument(spec.observations_[0]->Instrument(), request);
        ASSERT_EQ(AAD::Tape()->multi_, multi);
        ASSERT_EQ(AAD::Tape()->numAdj_, multi ? 2u : 1u);
        ASSERT_EQ(result.nativePartials_.Cols(), 2);
        auto invalid = request;
        invalid.parameters_ = {1000., -1000.};
        ASSERT_THROW(EvaluateCurveScriptInstrument(spec.observations_[0]->Instrument(), invalid), CalibrationError_);
        ASSERT_EQ(EvaluateCurveScriptInstrument(spec.observations_[0]->Instrument(), request).values_, result.values_);
    }
}

TEST(CompoundedOisEvaluationTest, TestTwoMaturityCalibrationAndP1FitSnapshotRejection) {
    const Vector_<> theta{-.0003, -.0016};
    auto first = OisSpec();
    first.instrumentId_ = "first-coupon";
    first.fixedLeg_ = {first.fixedLeg_[0]};
    first.coupons_ = {first.coupons_[0]};
    first.observations_ = {first.observations_[0], first.observations_[1], first.observations_[2]};
    auto spec = OisCalibration(first, OisValuation());
    const auto longer = OisCalibration(OisSpec(), OisValuation());
    spec.compoundedOis_.push_back(longer.compoundedOis_[0]);
    const Date_ today(2024, 2, 27), a(2024, 2, 28), b(2024, 2, 29), c(2024, 3, 1), e(2024, 3, 5);
    const auto rate = [&](Date_ start, Date_ end) {
        return std::expm1(OracleLogDf(start, today, theta) - OracleLogDf(end, today, theta)) / ((end - start) / 360.);
    };
    const double shortPar = ((1. + rate(a, b) / 360.) * (1. + rate(b, c) / 360.) * (1. + 2. * rate(c, e) / 360.) - 1.) / (4. / 365.);
    spec.observations_ = {NewCalibrationObservation("short", spec.observations_[0]->Instrument(), "ParRate",
                                                    NewCalibrationMarketQuote("short", shortPar, CalibrationValueUnit_::Parse("Rate")), 1e-10),
                          NewCalibrationObservation("long", longer.observations_[0]->Instrument(), "ParRate",
                                                    NewCalibrationMarketQuote("long", FutureOracle(theta)[1], CalibrationValueUnit_::Parse("Rate")),
                                                    1e-10)};
    spec.solver_.initialGuessPerNode_ = {-.0002, -.001};
    for (const auto derivative : {CalibrationDerivative_("AAD"), CalibrationDerivative_("FiniteDifference")}) {
        CurveInstrumentCalibrationOptions_ options;
        options.jacobianMode_ = derivative;
        const auto fit = CalibrateCurveInstruments(spec, options);
        ASSERT_TRUE(fit.fitWithinTolerance_);
        ASSERT_GT(fit.evaluations_, 0);
        for (int j = 0; j < 2; ++j)
            ASSERT_NEAR(fit.parameters_[j], theta[j], 1e-9);
        const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, {{1e-10, 1e-10}, 1e-10, 1e-12});
        ASSERT_FALSE(snapshot.MappingAvailable());
        ASSERT_EQ(snapshot.Diagnostics()[0].code_, "ScriptCurveQuoteRiskUnsupportedCapability");
    }
}

TEST(CompoundedOisEvaluationTest, TestPassiveAndHistoricalOvernightRoutesRequireContractCollateral) {
    for (const auto today : {Date_(2024, 2, 27), Date_(2024, 3, 7)}) {
        auto spec = OisCalibration(OisSpec(), OisValuation(today));
        spec.geometry_.targetOvernightRoute_.clear();
        spec.geometry_.calibrateDiscountCurve_ = true;
        spec.market_.routes_[1].collateral_ = CollateralType_("GC");
        spec.market_.overnightCurves_["USD.SOFR"].reset(new DiscountLogDF_("passive-on", "USD", spec.geometry_.knotDates_, {0., -.0002, -.0014},
                                                                           DayBasis_("ACT_365F"), LogDfScheme_("LOG_LINEAR")));
        ASSERT_THROW(EvaluateCurveScriptInstrument(spec.observations_[0]->Instrument(), Request(spec)), CalibrationError_);
    }
}

TEST(CompoundedOisEvaluationTest, TestDirectEvaluationRejectsUnrelatedFrozenTables) {
    const auto spec = OisCalibration(OisSpec(), OisValuation());
    const auto instrument = NewDepositScriptInstrument(Deposit_(spec.geometry_.today_, Date_(2024, 3, 8), .025, DayBasis_("ACT_360")),
                                                       {"deposit", Ccy_("USD"), "USD.OIS", 1.});
    ASSERT_THROW(EvaluateCurveScriptInstrument(instrument, Request(spec)), CalibrationError_);
}

TEST(CompoundedOisEvaluationTest, TestLegacyMultiCurveWrapperDoesNotTreatOvernightAsTenor) {
    const auto spec = OisCalibration(OisSpec(), OisValuation());
    try {
        CalibrateCurveInstruments(MultiCurveInstrumentCalibrationSpec_{{spec}});
        FAIL() << "Legacy tenor staging admitted ON";
    } catch (const CalibrationError_& error) {
        ASSERT_EQ(error.Failure().diagnostic_.code_, "UnsupportedCompoundedOisExecution");
    }
}

TEST(CompoundedOisEvaluationTest, TestBackgroundForwardKnotsMayPrecedeValuation) {
    for (const bool linear : {false, true}) {
        auto spec = OisCalibration(OisSpec(), OisValuation());
        spec.market_.routes_[0].useTargetCurve_ = false;
        const auto today = spec.geometry_.today_;
        const Vector_<Date_> knots{today.AddDays(-2), today.AddDays(20)};
        const Handle_<DiscountCurve_> background =
            linear ? Handle_<DiscountCurve_>(new Tape::DiscountPWLF_<double>("old", "USD", knots, {.02, .02}, {.02, .02}))
                   : Handle_<DiscountCurve_>(new Tape::DiscountPWC_<double>("old", "USD", knots, {.02, .02}));
        spec.market_.discountCurves_[CollateralType_("OIS")] = background;
        const auto result = EvaluateCurveScriptInstrument(spec.observations_[0]->Instrument(), Request(spec));
        for (const auto& flow : result.cashflows_)
            ASSERT_NEAR(*flow.discount_, (*background)(today, flow.payment_), 1e-14);
    }
}

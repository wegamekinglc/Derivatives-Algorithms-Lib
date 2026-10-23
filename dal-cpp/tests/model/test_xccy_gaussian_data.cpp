#include <gtest/gtest.h>

#include <dal/calibration/protocol.hpp>
#include <dal/curve/tapeguard.hpp>
#include <dal/curve/ycconst.hpp>
#include <dal/curve/yclogdf.hpp>
#include <dal/curve/ycpwlf.hpp>
#include <dal/model/xccygaussian.hpp>
#include <dal/model/xccygaussiandata.hpp>
#include <dal/platform/initall.hpp>
#include <dal/storage/json.hpp>

#include "xccygaussiankerneloracle.hpp"

using namespace Dal;

namespace {
    Handle_<DiscountCurve_> Curve(const String_& currency, double rate) {
        const Date_ start(2026, 10, 21);
        return Handle_<DiscountCurve_>(NewDiscountLogDF("curve", currency, {start, start.AddDays(1095)}, {0., -3. * rate}, DayBasis_("ACT365F"),
                                                        LogDfScheme_::Value_::LOG_LINEAR));
    }

    XccyGaussianMarketSpec_ MarketSpec() {
        const Handle_<DiscountCurve_> domestic = Curve("USD", .04), foreign = Curve("EUR", .02);
        XccyGaussianMarketSpec_ result;
        result.valuationTime_ = DateTime_(Date_(2026, 10, 21));
        result.pair_ = CurrencyPair_(Ccy_("USD"), Ccy_("EUR"));
        result.collateralCurrency_ = Ccy_("USD");
        result.fxSpot_ = 1.1;
        result.discountRoute_ = "USD.OIS/EUR.OIS/BASIS";
        result.domesticBlock_ = Handle_<CurveBlock_>(
            new CurveBlock_("domestic", "USD", {{CollateralType_("OIS"), domestic}}, {{PeriodLength_("3M"), Curve("USD", .045)}}));
        result.foreignBlock_ =
            Handle_<CurveBlock_>(new CurveBlock_("foreign", "EUR", {{CollateralType_("OIS"), foreign}}, {{PeriodLength_("6M"), Curve("EUR", .026)}}));
        result.basis_ = Curve("USD", .003);
        return result;
    }

    XccyGaussianModelSpec_ ModelSpec() {
        XccyGaussianModelSpec_ result;
        result.market_ = MarketSpec();
        auto& dynamics = result.dynamics_;
        dynamics.domesticA_ = .03;
        dynamics.foreignA_ = .07;
        dynamics.bucketEndDates_ = {Date_(2026, 10, 21).AddDays(1095)};
        dynamics.domesticSigma_ = {.01};
        dynamics.foreignSigma_ = {.009};
        dynamics.fxSigma_ = {.12};
        dynamics.factorNames_ = {"rate:USD", "rate:EUR", "fx:EUR/USD"};
        dynamics.correlation_ = XccyGaussianTestSupport::Correlation();
        return result;
    }

    XccyGaussianModelSpec_ BucketSpec() {
        auto spec = ModelSpec();
        const Date_ valuation = spec.market_.valuationTime_.Date();
        spec.dynamics_.bucketEndDates_ = {valuation.AddDays(180), valuation.AddDays(365), valuation.AddDays(1095)};
        spec.dynamics_.domesticSigma_ = {.008, .012, .010};
        spec.dynamics_.foreignSigma_ = {.006, .011, .009};
        spec.dynamics_.fxSigma_ = {.10, .14, .12};
        return spec;
    }

    Handle_<DiscountCurve_> NonflatCurve(const String_& currency, double rate, double quadratic) {
        const Date_ valuation(2026, 10, 21);
        Vector_<Date_> dates;
        Vector_<> values;
        for (const int days : {0, 180, 365, 730, 1095}) {
            const double t = days / 365.;
            dates.push_back(valuation.AddDays(days));
            values.push_back(-rate * t - quadratic * t * t);
        }
        return Handle_<DiscountCurve_>(NewDiscountLogDF("nonflat", currency, dates, values, DayBasis_("ACT365F"), LogDfScheme_::Value_::LOG_LINEAR));
    }

    XccyGaussianModelSpec_ NonflatSpec() {
        auto spec = BucketSpec();
        spec.market_.domesticBlock_ =
            Handle_<CurveBlock_>(new CurveBlock_("domestic", "USD", {{CollateralType_("OIS"), NonflatCurve("USD", .04, .002)}},
                                                 {{PeriodLength_("3M"), NonflatCurve("USD", .045, .0015)}}));
        spec.market_.foreignBlock_ = Handle_<CurveBlock_>(new CurveBlock_(
            "foreign", "EUR", {{CollateralType_("OIS"), NonflatCurve("EUR", .02, .001)}}, {{PeriodLength_("6M"), NonflatCurve("EUR", .026, .0008)}}));
        spec.market_.basis_ = NonflatCurve("USD", .003, -.0002);
        return spec;
    }

    template <class F_> void AssertError(const F_& action, const String_& code, const String_& fieldPrefix) {
        try {
            action();
            FAIL() << "expected " << code;
        } catch (const CalibrationError_& error) {
            const auto& failure = error.Failure();
            ASSERT_EQ(failure.diagnostic_.code_, code) << error.what();
            ASSERT_TRUE(failure.diagnostic_.field_);
            ASSERT_EQ(failure.diagnostic_.field_->find(fieldPrefix), 0U);
            ASSERT_TRUE(failure.report_);
            ASSERT_EQ(failure.report_->Errors().size(), 1);
        }
    }

    void AssertMartingales(const XccyGaussianModelSpec_& spec, int days) {
        using namespace XccyGaussianTestSupport;
        const XccyGaussianModelData_ data("martingale", spec);
        const auto model = NewXccyGaussianModel(data);
        const auto moments = OracleMoments(spec, 0.L, days / 365.L);
        const Date_ date = data.market_->valuationTime_.Date().AddDays(days);
        auto meanD = model->InitialState(), meanF = meanD;
        meanD.date_ = meanF.date_ = date;
        for (int i = 0; i < 5; ++i) {
            meanD.values_[i] = static_cast<double>(moments.meanD_[i]);
            meanF.values_[i] = static_cast<double>(moments.meanF_[i]);
        }
        const auto& c = moments.covariance_;
        const double pd = std::exp(data.market_->domestic_->LogDF(date));
        const double pf = std::exp(data.market_->foreign_->LogDF(date) - data.market_->basis_->LogDF(date));
        const long double domesticUnit = std::exp(.5L * c[2][2]) / model->DomesticNumeraire(meanD);
        const long double foreignUnit = model->Spot(meanD) / model->DomesticNumeraire(meanD) * std::exp(.5L * (c[3][3] + c[4][4] - 2.L * c[3][4]));
        const long double density =
            model->Spot(meanD) * model->ForeignNumeraire(meanD) / (data.market_->fxSpot_ * model->DomesticNumeraire(meanD)) * std::exp(.5L * c[4][4]);
        ASSERT_NEAR(static_cast<double>(domesticUnit), pd, 1e-13);
        ASSERT_NEAR(static_cast<double>(foreignUnit), data.market_->fxSpot_ * pf, 1e-13);
        ASSERT_NEAR(static_cast<double>(density), 1., 1e-13);
        // Qf uses zero-mean foreign OU, positive domestic quanto and +.5 FX variance drift.
        const long double spotF = model->Spot(meanF) * std::exp(c[4][4]);
        const long double qfDomesticUnit =
            data.market_->fxSpot_ / (spotF * model->ForeignNumeraire(meanF)) * std::exp(.5L * (c[2][2] + c[4][4] + 2.L * c[2][4]));
        const long double qfForeignUnit = data.market_->fxSpot_ / model->ForeignNumeraire(meanF) * std::exp(.5L * c[3][3]);
        ASSERT_NEAR(static_cast<double>(qfDomesticUnit), pd, 1e-13);
        ASSERT_NEAR(static_cast<double>(qfForeignUnit), data.market_->fxSpot_ * pf, 1e-13);
    }
} // namespace

TEST(XccyGaussianDataTest, TestOwnedNormalizedSnapshotAndRoutes) {
    RegisterAll_::Init();
    auto spec = MarketSpec();
    const XccyGaussianMarket_ market("market", spec);
    const Date_ maturity = spec.valuationTime_.Date().AddDays(365);
    ASSERT_NEAR(market.domestic_->LogDF(maturity), -.04, 1e-15);
    ASSERT_NEAR(market.foreign_->LogDF(maturity) - market.basis_->LogDF(maturity), -.017, 1e-15);
    ASSERT_NEAR(market.Projection("EUR", "6M").LogDF(maturity), -.026, 1e-15);
    spec.basis_.reset();
    spec.domesticBlock_.reset();
    spec.foreignBlock_.reset();
    ASSERT_NEAR(market.domestic_->LogDF(maturity), -.04, 1e-15);
    ASSERT_THROW((void)market.Projection("EUR", "3M"), CalibrationError_);
}

TEST(XccyGaussianModelTest, TestInitialCurveFitAndExactFiveSlotPath) {
    const XccyGaussianModelData_ data("model", ModelSpec());
    Dal::AAD::XccyGaussian_<> model(data);
    const auto initial = model.InitialState();
    const Date_ maturity = initial.date_.AddDays(365);
    ASSERT_NEAR(model.Bond("USD", initial, maturity), std::exp(-.04), 1e-15);
    ASSERT_NEAR(model.Bond("EUR", initial, maturity), std::exp(-.017), 1e-15);
    ASSERT_NEAR(model.ForeignOwnBond(initial, maturity), std::exp(-.02), 1e-15);
    ASSERT_NEAR(model.ProjectionBond("EUR", "6M", initial, maturity), std::exp(-.026), 1e-15);
    ASSERT_NEAR(model.FxForward(initial, maturity), 1.1 * std::exp(.023), 1e-15);
    model.Prepare({maturity});
    ASSERT_EQ(model.NumFactors(), 3);
    ASSERT_EQ(model.SimDim(), 5);
    ASSERT_FALSE(model.SupportsBrownianBridge());
    const Vector_<> normals{.3, -.5, .7, -.2, .1};
    const auto path = model.GenerateStates(normals);
    ASSERT_EQ(path.size(), 2);
    const auto transition = model.Transition(initial.date_, maturity);
    const auto lower = XccyGaussianDetail::ScaledFactor(transition.covariance_);
    for (int i = 0; i < 5; ++i) {
        double expected = transition.shift_[i];
        for (int j = 0; j <= i; ++j)
            expected += lower(i, j) * normals[j];
        ASSERT_NEAR(path.back().values_[i], expected, 1e-15);
    }
    ASSERT_GT(model.DomesticNumeraire(path.back()), 0.);
    ASSERT_GT(model.Spot(path.back()), 0.);
}

TEST(XccyGaussianModelTest, TestGridIncludesAllDynamicsBoundaries) {
    const auto spec = BucketSpec();
    const Date_ valuation = spec.market_.valuationTime_.Date();
    const XccyGaussianModelData_ data("bucketed", spec);
    auto model = NewXccyGaussianModel(data);
    model->Prepare({valuation.AddDays(730)});
    ASSERT_EQ(model->Grid(), (Vector_<Date_>{valuation, valuation.AddDays(180), valuation.AddDays(365), valuation.AddDays(730)}));
    ASSERT_EQ(model->SimDim(), 15);
}

TEST(XccyGaussianDataTest, TestPairMustBeExplicit) {
    auto spec = MarketSpec();
    spec.pair_ = {};
    ASSERT_THROW(XccyGaussianMarket_("missing-pair", spec), CalibrationError_);
}

TEST(XccyGaussianDataTest, TestRejectInconsistentNativeCoordinateMap) {
    const Date_ valuation(2026, 10, 21);
    Matrix_<> jacobian(2, 1, 0.);
    jacobian(1, 0) = 1.;
    ASSERT_THROW(XccyGaussianCurveData_("bad-map", "USD", "OIS", {valuation, valuation.AddDays(365)}, {0., -.04}, {"logdf:USD:OIS:2027-10-21"},
                                        {-.03}, jacobian),
                 CalibrationError_);
}

TEST(XccyGaussianDataTest, TestAnchorRiskAndChangedNativeMapCannotMasqueradeAsNumericRefresh) {
    const Date_ valuation(2026, 10, 21);
    {
        Matrix_<> jacobian(2, 2, 0.);
        jacobian(0, 1) = 1.;
        jacobian(1, 0) = 1.;
        ASSERT_THROW(XccyGaussianCurveData_("anchor-risk", "USD", "OIS", {valuation, valuation.AddDays(365)}, {0., -.04}, {"one", "two"}, {-.04, 0.},
                                            jacobian),
                     CalibrationError_);
    }
    const XccyGaussianModelData_ original("original", ModelSpec());
    const auto& market = *original.market_;
    Matrix_<> changedMap = market.domestic_->snapshotJacobian_;
    changedMap(1, 0) *= 2.;
    Vector_<> changedValues = market.domestic_->nativeValues_;
    changedValues[0] /= 2.;
    const Handle_<XccyGaussianCurveData_> changedCurve(new XccyGaussianCurveData_(
        "changed", "USD", "OIS", market.domestic_->nodeDates_, market.domestic_->logDF_, market.domestic_->nativeLabels_, changedValues, changedMap));
    const Handle_<XccyGaussianMarket_> changedMarket(new XccyGaussianMarket_("changed", valuation, "USD", "EUR", "USD", market.fxSpot_,
                                                                             market.discountRoute_, changedCurve, market.foreign_, market.basis_,
                                                                             market.projections_));
    const XccyGaussianModelData_ changed("changed", changedMarket, original.dynamics_);
    AssertError([&] { original.RequireSameGeometry(changed); }, "CalibrationContextMismatch", "/model/market/refresh/curves");
}

TEST(XccyGaussianModelTest, TestNumericRefreshRequiredAfterParameterMutation) {
    const XccyGaussianModelData_ data("model", ModelSpec());
    auto model = NewXccyGaussianModel(data);
    const Date_ maturity = data.market_->valuationTime_.Date().AddDays(365);
    model->Prepare({maturity});
    const Vector_<> normals{.3, -.5, .7, -.2, .1};
    const auto original = model->GenerateStates(normals);
    const auto& labels = model->ParameterLabels();
    const size_t sigma = static_cast<size_t>(std::find(labels.begin(), labels.end(), "sigma:USD:2029-10-20") - labels.begin());
    ASSERT_LT(sigma, model->NumParams());
    *model->Parameters()[sigma] *= 1.1;
    ASSERT_THROW((void)model->GenerateStates(normals), CalibrationError_);
    model->RefreshNumeric();
    const auto refreshed = model->GenerateStates(normals);
    ASSERT_NE(refreshed.back().values_[0], original.back().values_[0]);
    ASSERT_NE(refreshed.back().values_[2], original.back().values_[2]);
}

TEST(XccyGaussianModelTest, TestFailedPreparationPreservesThePreviousValidGrid) {
    auto spec = ModelSpec();
    const Date_ valuation = spec.market_.valuationTime_.Date();
    const Handle_<DiscountCurve_> shortCurve(
        NewDiscountLogDF("short", "USD", {valuation, valuation.AddDays(365)}, {0., -.04}, DayBasis_("ACT365F"), LogDfScheme_::Value_::LOG_LINEAR));
    spec.market_.domesticBlock_ = Handle_<CurveBlock_>(new CurveBlock_(shortCurve));
    const XccyGaussianModelData_ data("short-coverage", spec);
    auto model = NewXccyGaussianModel(data);
    model->Prepare({valuation.AddDays(365)});
    const auto grid = model->Grid();
    ASSERT_THROW(model->Prepare({valuation.AddDays(730)}), CalibrationError_);
    ASSERT_EQ(model->Grid(), grid);
    ASSERT_EQ(model->GenerateStates(Vector_<>(5, 0.)).size(), 2);
}

TEST(XccyGaussianModelTest, TestExponentialOverflowIsNumericalFailure) {
    const XccyGaussianModelData_ data("model", ModelSpec());
    const auto model = NewXccyGaussianModel(data);
    auto state = model->InitialState();
    state.values_[4] = 1e4;
    ASSERT_THROW((void)model->Spot(state), CalibrationError_);
    state.values_[2] = 1e4;
    ASSERT_THROW((void)model->DomesticNumeraire(state), CalibrationError_);
    state.values_[0] = -1e6;
    ASSERT_THROW((void)model->Bond("USD", state, state.date_.AddDays(365)), CalibrationError_);
}

TEST(XccyGaussianModelTest, TestNumericalFailureRetainsStageAndEventDate) {
    const XccyGaussianModelData_ data("model", ModelSpec());
    const auto model = NewXccyGaussianModel(data);
    auto state = model->InitialState();
    state.values_[4] = 1e4;
    try {
        (void)model->Spot(state);
        FAIL() << "expected overflow";
    } catch (const CalibrationError_& error) {
        ASSERT_EQ(error.Failure().diagnostic_.stage_, CalibrationStage_::Value_::Evaluate);
        ASSERT_EQ(error.Failure().diagnostic_.eventDate_, state.date_);
        ASSERT_EQ(error.Failure().report_->Stage(), CalibrationStage_::Value_::Evaluate);
    }
}

TEST(XccyGaussianModelTest, TestNonfiniteComposedMomentsFailBeforeReturningAValue) {
    auto spec = ModelSpec();
    const Date_ valuation = spec.market_.valuationTime_.Date();
    spec.dynamics_.bucketEndDates_ = {valuation.AddDays(365), valuation.AddDays(730)};
    spec.dynamics_.domesticSigma_ = {1e154, 1e154};
    spec.dynamics_.foreignSigma_ = {.009, .009};
    spec.dynamics_.fxSigma_ = {.12, .12};
    const XccyGaussianModelData_ data("overflow", spec);
    const auto model = NewXccyGaussianModel(data);
    AssertError([&] { (void)model->Transition(valuation, valuation.AddDays(730)); }, "CalibrationNumericalFailure", "/model/transition/");
}

TEST(XccyGaussianModelTest, TestBucketedJointMomentsAgainstIndependentKernels) {
    using namespace XccyGaussianTestSupport;
    const auto spec = BucketSpec();
    const XccyGaussianModelData_ data("model", spec);
    const auto model = NewXccyGaussianModel(data);
    const Date_ valuation = data.market_->valuationTime_.Date();
    const auto transition = model->Transition(valuation.AddDays(90), valuation.AddDays(730));
    const auto oracle = OracleMoments(spec, 90.L / 365.L, 2.L, 48);
    const auto refined = OracleMoments(spec, 90.L / 365.L, 2.L, 64);
    const auto lower = XccyGaussianDetail::ScaledFactor(transition.covariance_);
    for (int i = 0; i < 5; ++i) {
        ASSERT_NEAR(transition.shift_[i], static_cast<double>(oracle.meanD_[i]), 1e-15);
        for (int j = 0; j < 5; ++j) {
            const long double scale = std::sqrt(oracle.covariance_[i][i] * oracle.covariance_[j][j]);
            ASSERT_NEAR(static_cast<double>((oracle.covariance_[i][j] - refined.covariance_[i][j]) / scale), 0., 1e-13);
            ASSERT_NEAR(static_cast<double>((transition.covariance_(i, j) - oracle.covariance_[i][j]) / scale), 0., 1e-12);
            long double reconstruction = 0.L;
            for (int k = 0; k < 5; ++k)
                reconstruction += static_cast<long double>(lower(i, k)) * lower(j, k);
            ASSERT_NEAR(static_cast<double>((reconstruction - oracle.covariance_[i][j]) / scale), 0., 1e-12);
        }
    }
}

TEST(XccyGaussianModelTest, TestDomesticForeignAndMeasureDensityMartingales) {
    for (const bool nonflat : {false, true})
        for (int flip = -1; flip < 3; ++flip) {
            auto spec = nonflat ? NonflatSpec() : ModelSpec();
            if (flip >= 0) {
                const int row[3] = {0, 0, 1}, col[3] = {1, 2, 2};
                spec.dynamics_.correlation_(row[flip], col[flip]) *= -1.;
                spec.dynamics_.correlation_(col[flip], row[flip]) *= -1.;
            }
            AssertMartingales(spec, 365);
            AssertMartingales(spec, 730);
        }
    auto spec = ModelSpec();
    spec.dynamics_.domesticSigma_ = spec.dynamics_.foreignSigma_ = {0.};
    AssertMartingales(spec, 730);
    spec.dynamics_.fxSigma_ = {0.};
    AssertMartingales(spec, 730);
}

TEST(XccyGaussianModelTest, TestQuantoAndBasisSignNegativeControls) {
    using namespace XccyGaussianTestSupport;
    const auto spec = ModelSpec();
    const XccyGaussianModelData_ data("model", spec);
    const auto model = NewXccyGaussianModel(data);
    const auto moments = OracleMoments(spec, 0.L, 2.L);
    auto missingQuanto = model->InitialState();
    missingQuanto.date_ = missingQuanto.date_.AddDays(730);
    const auto& c = moments.covariance_;
    const long double wrong =
        model->Spot(missingQuanto) / model->DomesticNumeraire(missingQuanto) * std::exp(.5L * (c[3][3] + c[4][4] - 2.L * c[3][4]));
    const double correct = 1.1 * std::exp(-.017 * 2.);
    ASSERT_GT(std::abs(static_cast<double>(wrong) - correct), 1e-5);
    ASSERT_LE(std::abs(moments.meanD_[3] + c[3][4]), 1e-17L);
    const double wrongBasis = 1.1 * std::exp((-.02 - .003) * 2.);
    ASSERT_GT(std::abs(wrongBasis - correct), 1e-3);
    AssertMartingales(spec, 730);
}

TEST(XccyGaussianModelTest, TestConditionalBondsAndProjectionAgainstKernelDiscountExpectation) {
    using namespace XccyGaussianTestSupport;
    const auto spec = NonflatSpec();
    const XccyGaussianModelData_ data("nonflat", spec);
    const auto model = NewXccyGaussianModel(data);
    auto state = model->InitialState();
    state.date_ = state.date_.AddDays(180);
    state.values_[0] = .02;
    state.values_[1] = -.01;
    const Date_ maturity = data.market_->valuationTime_.Date().AddDays(730);
    const long double t = 180.L / 365.L, finalTime = 2.L;
    const auto atT = OracleMoments(spec, 0.L, finalTime), atCurrent = OracleMoments(spec, 0.L, t), future = OracleMoments(spec, t, finalTime);
    for (const bool foreign : {false, true}) {
        const int slot = foreign ? 3 : 2, factor = foreign ? 1 : 0;
        const auto& curve = foreign ? data.market_->foreign_ : data.market_->domestic_;
        const double a = foreign ? spec.dynamics_.foreignA_ : spec.dynamics_.domesticA_;
        const long double integratedStateMean =
            Integrate([&](long double s) { return state.values_[factor] * std::exp(-a * (s - t)); }, t, finalTime, 48);
        long double logRatio = curve->LogDF(maturity) - curve->LogDF(state.date_);
        if (foreign)
            logRatio -= data.market_->basis_->LogDF(maturity) - data.market_->basis_->LogDF(state.date_);
        const long double expected = std::exp(logRatio - .5L * (atT.covariance_[slot][slot] - atCurrent.covariance_[slot][slot]) -
                                              integratedStateMean + .5L * future.covariance_[slot][slot]);
        const String_ currency = foreign ? "EUR" : "USD";
        ASSERT_NEAR(model->Bond(currency, state, maturity), static_cast<double>(expected), 1e-13);
        ASSERT_DOUBLE_EQ(model->Bond(currency, state, state.date_), 1.);
    }
    const auto initial = model->InitialState();
    const Date_ start = initial.date_.AddDays(365);
    const double nativeRate =
        (std::exp(data.market_->Projection("EUR", "6M").LogDF(start) - data.market_->Projection("EUR", "6M").LogDF(maturity)) - 1.) / (365. / 360.);
    ASSERT_NEAR(model->ForwardRate("EUR", "6M", initial, start, maturity, 365. / 360.), nativeRate, 1e-14);
    ASSERT_NE(model->ForwardRate("EUR", "6M", state, start, maturity, 365. / 360.), nativeRate);
    ASSERT_NEAR(model->FxForward(state, maturity), model->Spot(state) * model->Bond("EUR", state, maturity) / model->Bond("USD", state, maturity),
                1e-15);
}

TEST(XccyGaussianDataTest, TestNativeNormalizationBaseUnionAndLifetime) {
    const Date_ anchor(2026, 10, 21), valuation = anchor.AddDays(90);
    auto* base = new Tape::DiscountPWC_<double>("base", "USD", {anchor, anchor.AddDays(365), anchor.AddDays(730), anchor.AddDays(1095)},
                                                {.002, .003, .004, .004});
    const Handle_<DiscountCurve_> ownedBase(base);
    auto* original = new DiscountLogDF_("native", "USD", {anchor, anchor.AddDays(180), anchor.AddDays(1095)}, {0., -.02, -.20}, DayBasis_("ACT360"),
                                        LogDfScheme_::Value_::LOG_LINEAR, ownedBase);
    Handle_<DiscountCurve_> owned(original);
    const auto snapshot = SnapshotXccyGaussianCurve(*original, valuation, "USD", "OIS");
    ASSERT_EQ(snapshot->nodeDates_, (Vector_<Date_>{valuation, anchor.AddDays(180), anchor.AddDays(365), anchor.AddDays(730), anchor.AddDays(1095)}));
    ASSERT_EQ(snapshot->nativeValues_.size(), 6);
    for (int days = 90; days <= 1095; days += 37)
        ASSERT_NEAR(snapshot->LogDF(anchor.AddDays(days)), std::log((*original)(valuation, anchor.AddDays(days))), 1e-15);
    for (int col = 0; col < 6; ++col) {
        auto* native = col < 2 ? static_cast<FittableCurve_*>(original) : static_cast<FittableCurve_*>(base);
        Vector_<> dx(col < 2 ? 2 : 4, 0.);
        dx[col < 2 ? col : col - 2] = 1e-6;
        native->ApplyDX(dx.begin(), 1.);
        const auto up = SnapshotXccyGaussianCurve(*original, valuation, "USD", "OIS");
        native->ApplyDX(dx.begin(), -2.);
        const auto down = SnapshotXccyGaussianCurve(*original, valuation, "USD", "OIS");
        native->ApplyDX(dx.begin(), 1.);
        for (int row = 0; row < snapshot->snapshotJacobian_.Rows(); ++row)
            ASSERT_NEAR((up->logDF_[row] - down->logDF_[row]) / 2e-6, snapshot->snapshotJacobian_(row, col), 1e-10);
    }
    const double frozen = snapshot->logDF_.back();
    Vector_<> bump{.01, .02};
    original->ApplyDX(bump.begin(), 1.);
    ASSERT_DOUBLE_EQ(snapshot->logDF_.back(), frozen);
    owned.reset();
    ASSERT_DOUBLE_EQ(snapshot->LogDF(snapshot->nodeDates_.back()), frozen);
}

TEST(XccyGaussianDataTest, TestNewTypeArchiveRoundTripAndNumericGeometry) {
    const XccyGaussianModelData_ original("roundtrip", NonflatSpec());
    const auto restored = handle_cast<XccyGaussianModelData_>(JSON::ReadString(JSON::WriteString(original), false));
    ASSERT_TRUE(restored);
    ASSERT_EQ(original.Identity(), restored->Identity());
    original.RequireSameGeometry(*restored);
    auto candidateSpec = NonflatSpec();
    candidateSpec.market_.fxSpot_ = 1.2;
    const XccyGaussianModelData_ candidate("roundtrip", candidateSpec);
    original.RequireSameGeometry(candidate);
    ASSERT_NE(original.Identity(), candidate.Identity());
    candidateSpec.dynamics_.bucketEndDates_[0] = candidateSpec.dynamics_.bucketEndDates_[0].AddDays(1);
    const XccyGaussianModelData_ changed("roundtrip", candidateSpec);
    AssertError([&] { original.RequireSameGeometry(changed); }, "CalibrationContextMismatch", "/model/market/refresh/dynamics");
}

TEST(XccyGaussianDataTest, TestArchiveRoundTripWithoutProjectionRoutes) {
    auto spec = ModelSpec();
    spec.market_.domesticBlock_ = Handle_<CurveBlock_>(new CurveBlock_("domestic", "USD", {{CollateralType_("OIS"), Curve("USD", .04)}}));
    spec.market_.foreignBlock_ = Handle_<CurveBlock_>(new CurveBlock_("foreign", "EUR", {{CollateralType_("OIS"), Curve("EUR", .02)}}));
    const XccyGaussianModelData_ original("no-projections", spec);
    ASSERT_TRUE(original.market_->projections_.empty());
    const auto restored = handle_cast<XccyGaussianModelData_>(JSON::ReadString(JSON::WriteString(original), false));
    ASSERT_TRUE(restored);
    ASSERT_TRUE(restored->market_->projections_.empty());
    ASSERT_EQ(original.Identity(), restored->Identity());
    original.RequireSameGeometry(*restored);
    const auto model = NewXccyGaussianModel(*restored);
    const auto initial = model->InitialState();
    const Date_ maturity = initial.date_.AddDays(365);
    model->Prepare({maturity});
    ASSERT_EQ(model->SimDim(), 5);
    ASSERT_EQ(model->GenerateStates(Vector_<>(5, 0.)).size(), 2);
    ASSERT_NEAR(model->Bond("EUR", initial, maturity), std::exp(-.017), 1e-15);
    ASSERT_THROW((void)model->ProjectionBond("EUR", "6M", initial, maturity), CalibrationError_);
}

TEST(XccyGaussianDataTest, TestMarketCurrencyRouteTimeAndCoverageErrorsOwnReports) {
    {
        auto spec = MarketSpec();
        spec.basis_ = Curve("EUR", .003);
        AssertError([&] { XccyGaussianMarket_ market("wrong-basis", spec); }, "CalibrationContextMismatch", "/model/market/");
    }
    {
        auto spec = MarketSpec();
        spec.basis_.reset();
        AssertError([&] { XccyGaussianMarket_ market("missing-basis", spec); }, "CalibrationContextMismatch", "/model/market/basis");
    }
    {
        auto spec = MarketSpec();
        spec.collateralCurrency_ = Ccy_("EUR");
        AssertError([&] { XccyGaussianMarket_ market("wrong-collateral", spec); }, "CalibrationContextMismatch", "/model/market/collateralCurrency");
    }
    {
        auto spec = MarketSpec();
        spec.valuationTime_ = DateTime_(spec.valuationTime_.Date(), 10);
        AssertError([&] { XccyGaussianMarket_ market("intraday", spec); }, "UnsupportedCalibrationFixingTime", "/model/market/valuationTime");
    }
    {
        auto spec = MarketSpec();
        spec.domesticBlock_ = Handle_<CurveBlock_>(new CurveBlock_("missing-ois", "USD", {{CollateralType_("GC"), Curve("USD", .04)}}));
        AssertError([&] { XccyGaussianMarket_ market("missing-ois", spec); }, "CalibrationContextMismatch", "/model/market/block/OIS");
    }
    {
        const XccyGaussianModelData_ data("model", ModelSpec());
        const auto model = NewXccyGaussianModel(data);
        const Date_ valuation = data.market_->valuationTime_.Date();
        AssertError([&] { model->Prepare({valuation.AddDays(1096)}); }, "InvalidCalibrationPlan", "/model/dynamics/coverage");
        AssertError([&] { (void)model->Transition(valuation.AddDays(1), valuation); }, "InvalidCalibrationPlan", "/model/transition/interval");
        AssertError([&] { (void)model->Bond("GBP", model->InitialState(), valuation.AddDays(365)); }, "CalibrationContextMismatch",
                    "/model/market/currency");
    }
}

TEST(XccyGaussianDataTest, TestLosslessCurveAdmissionAndExplicitProjection) {
    const Date_ valuation(2026, 10, 21);
    const Vector_<Date_> dates{valuation, valuation.AddDays(365), valuation.AddDays(1095)};
    const DiscountLogDF_ nonzeroAnchor("nonzero-anchor", "USD", dates, {5e-16, -.04, -.12}, DayBasis_("ACT365F"), LogDfScheme_::Value_::LOG_LINEAR);
    AssertError([&] { (void)SnapshotXccyGaussianCurve(nonzeroAnchor, valuation, "USD", "OIS"); }, "MCCrossCurrencyUnsupported",
                "/model/market/USD:OIS");
    const DiscountLogDF_ cubic("cubic", "USD", dates, {0., -.04, -.12}, DayBasis_("ACT365F"), LogDfScheme_::Value_::LOG_CUBIC_NATURAL);
    AssertError([&] { (void)SnapshotXccyGaussianCurve(cubic, valuation, "USD", "OIS"); }, "MCCrossCurrencyUnsupported", "/model/market/USD:OIS");
    const Tape::DiscountPWLF_<double> linear("linear", "USD", dates, {.01, .03, .04}, {.02, .035, .04});
    AssertError([&] { (void)SnapshotXccyGaussianCurve(linear, valuation, "USD", "OIS"); }, "MCCrossCurrencyUnsupported", "/model/market/USD:OIS");
    const Tape::DiscountPWLF_<double> constant("constant", "USD", dates, {.01, .02, .03}, {.02, .03, .04});
    const auto snapshot = SnapshotXccyGaussianCurve(constant, valuation, "USD", "OIS");
    for (const int days : {0, 90, 365, 540, 1095})
        ASSERT_NEAR(snapshot->LogDF(valuation.AddDays(days)), std::log(constant(valuation, valuation.AddDays(days))), 1e-15);
    auto spec = MarketSpec();
    spec.foreignBlock_ = Handle_<CurveBlock_>(new CurveBlock_(Curve("EUR", .02)));
    const XccyGaussianMarket_ noProjection("no-unused-projection", spec);
    AssertError([&] { (void)noProjection.Projection("EUR", "6M"); }, "CalibrationContextMismatch", "/model/market/projection/EUR/6M");
}

TEST(XccyGaussianDataTest, TestExplicitDynamicsAndStrictNamedPositiveDefiniteCorrelation) {
    const auto original = ModelSpec();
    for (const double a : {-1., std::numeric_limits<double>::quiet_NaN()}) {
        auto spec = original;
        spec.dynamics_.domesticA_ = a;
        AssertError([&] { XccyGaussianModelData_ data("invalid-a", spec); }, "InvalidCalibrationPlan", "/model/dynamics/meanReversion");
    }
    {
        auto spec = original;
        spec.dynamics_.fxSigma_.clear();
        AssertError([&] { XccyGaussianModelData_ data("missing-sigma", spec); }, "InvalidCalibrationPlan", "/model/dynamics/buckets");
    }
    for (const double sigma : {-1., std::numeric_limits<double>::infinity()}) {
        auto spec = original;
        spec.dynamics_.domesticSigma_ = {sigma};
        AssertError([&] { XccyGaussianModelData_ data("invalid-sigma", spec); }, "InvalidCalibrationPlan", "/model/dynamics/sigma");
    }
    {
        auto spec = original;
        spec.dynamics_.factorNames_ = {"rate:EUR", "rate:USD", "fx:EUR/USD"};
        AssertError([&] { XccyGaussianModelData_ data("wrong-names", spec); }, "CalibrationContextMismatch", "/model/market/correlation/names");
    }
    for (const double rho : {1., 1. - 1e-11, 1. - 1e-9}) {
        auto spec = original;
        spec.dynamics_.correlation_ = Matrix_<>(3, 3, 0.);
        for (int i = 0; i < 3; ++i)
            spec.dynamics_.correlation_(i, i) = 1.;
        spec.dynamics_.correlation_(0, 1) = spec.dynamics_.correlation_(1, 0) = rho;
        if (rho > 1. - 1e-10)
            AssertError([&] { XccyGaussianModelData_ data("singular", spec); }, "InvalidCalibrationPlan", "/model/dynamics/correlation");
        else {
            const XccyGaussianModelData_ data("strictly-pd", spec);
            auto model = NewXccyGaussianModel(data);
            ASSERT_NO_THROW(model->Prepare({spec.market_.valuationTime_.Date().AddDays(365)}));
        }
    }
}

TEST(XccyGaussianModelTest, TestZeroVolatilityValueAndDerivativeBoundaryAreSeparate) {
    auto spec = ModelSpec();
    spec.dynamics_.domesticA_ = spec.dynamics_.foreignA_ = 0.;
    spec.dynamics_.domesticSigma_ = spec.dynamics_.foreignSigma_ = spec.dynamics_.fxSigma_ = {0.};
    const XccyGaussianModelData_ data("zero", spec);
    auto scalar = NewXccyGaussianModel(data);
    const Date_ date = data.market_->valuationTime_.Date().AddDays(365);
    scalar->Prepare({date});
    ASSERT_EQ(scalar->SimDim(), 5);
    const auto path = scalar->GenerateStates({1., -2., 3., -4., 5.});
    for (const double value : path.back().values_)
        ASSERT_DOUBLE_EQ(value, 0.);
    ASSERT_NEAR(scalar->DomesticNumeraire(path.back()), std::exp(.04), 1e-15);
    ASSERT_NEAR(scalar->Spot(path.back()), 1.1 * std::exp(.023), 1e-15);
    auto aad = NewXccyGaussianModel<AAD::Number_>(data);
    AssertError([&] { aad->Prepare({date}); }, "DerivativeUnavailable", "/model/dynamics/sigma");
    AssertError([&] { scalar->Allocate({}, {}); }, "MCCrossCurrencyUnsupported", "/model/observations");
    ASSERT_EQ(scalar->FactorNames().size(), 3);
    ASSERT_EQ(scalar->InnovationSlots(), (Vector_<String_>{"xd", "xf", "Id", "If", "Z"}));
    ASSERT_EQ(scalar->LayoutVersion(), "dal.xccy-gaussian-exact/1");
}

namespace {
    template <class T_> T_ PathValue(AAD::XccyGaussian_<T_>* model, const Date_& sample, const Vector_<>& normals) {
        model->Prepare({sample});
        const auto path = model->GenerateStates(normals);
        const auto& state = path.back();
        const T_ discount = 1. / model->DomesticNumeraire(state);
        return (model->Spot(state) + .2) * discount + .3 * model->Bond("EUR", state, sample.AddDays(365)) +
               .1 * model->ProjectionBond("EUR", "6M", state, sample.AddDays(365));
    }

    double CentralDifference(AAD::XccyGaussian_<>* model, size_t parameter, double h, const Date_& sample, const Vector_<>& normals) {
        const double original = *model->Parameters()[parameter];
        *model->Parameters()[parameter] = original + h;
        const double up = PathValue(model, sample, normals);
        *model->Parameters()[parameter] = original - h;
        const double down = PathValue(model, sample, normals);
        *model->Parameters()[parameter] = original;
        return (up - down) / (2. * h);
    }
} // namespace

TEST(XccyGaussianModelTest, TestInteriorAadAndSmallPositiveSigmaMatchRefreshedCrnDifferences) {
    for (const double domesticSigma : {.01, 1e-8}) {
        auto spec = ModelSpec();
        spec.dynamics_.domesticSigma_ = {domesticSigma};
        const XccyGaussianModelData_ data("aad", spec);
        const Date_ sample = data.market_->valuationTime_.Date().AddDays(365);
        const Vector_<> normals{.3, -.5, .7, -.2, .1};
        const TapeGuard_ guard(AAD::Tape());
        auto aad = NewXccyGaussianModel<AAD::Number_>(data);
        AAD::Rewind(*AAD::Tape());
        for (auto* parameter : aad->Parameters())
            AAD::PutOnTape(*parameter);
        AAD::NewRecording(*AAD::Tape());
        AAD::Number_ value = PathValue(aad.get(), sample, normals);
        AAD::Adjoint(value) = 1.;
        AAD::PropagateToStart(*AAD::Tape());
        auto scalar = NewXccyGaussianModel(data);
        ASSERT_NEAR(AAD::Value(value), PathValue(scalar.get(), sample, normals), 1e-13);
        for (size_t i = 0; i < scalar->NumParams(); ++i) {
            const bool small = scalar->ParameterLabels()[i].find("sigma:USD:") == 0 && domesticSigma < 1e-6;
            const double h = small ? 1e-11 : 1e-6;
            const double differences[2] = {CentralDifference(scalar.get(), i, h, sample, normals),
                                           CentralDifference(scalar.get(), i, h / 2., sample, normals)};
            ASSERT_NEAR(differences[0], differences[1], small ? 5e-5 : 1e-8);
            ASSERT_NEAR(AAD::Adjoint(*aad->Parameters()[i]), differences[1], small ? 5e-5 : 1e-8) << scalar->ParameterLabels()[i];
        }
        ASSERT_EQ(std::find(aad->ParameterLabels().begin(), aad->ParameterLabels().end(), "a:USD"), aad->ParameterLabels().end());
    }
}

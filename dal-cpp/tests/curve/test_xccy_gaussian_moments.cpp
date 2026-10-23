#include <gtest/gtest.h>

#include <dal/curve/calibration_internal.hpp>
#include <dal/curve/xccygaussianmoments.hpp>
#include <dal/model/xccygaussian.hpp>

#include "xccygaussianmomentfixture.hpp"

using namespace Dal;
using namespace Dal::TestXccyMoments;

TEST(XccyGaussianMomentsTest, TestZeroDynamicsIndependentCashflows) {
    const auto spec = ModelSpec(TestXccy::TODAY, true);
    const XccyGaussianModelData_ data("moments", spec);
    const auto prepared = Prepare(Instrument(), spec);
    const auto result = EvaluateXccyGaussianMoments<double>(prepared, data);
    const auto time = [](const Date_& date) { return (date - TestXccy::TODAY) / 365.; };
    const Date_ start(2027, 5, 4), middle(2027, 8, 4), end(2027, 11, 4), delayed(2027, 11, 8);
    const double first = (middle - start) / 360., second = (end - middle) / 360., foreign = (end - start) / 360.;
    const double a = -110. * (first * std::exp(-.04 * time(middle)) + second * std::exp(-.04 * time(end)));
    const double coupons = 100. * (std::exp(.026 * (end - start) / 365.) - 1.) * 1.1 * std::exp(-.017 * time(delayed)) -
                           110. * ((std::exp(.045 * (middle - start) / 365.) - 1.) * std::exp(-.04 * time(middle)) +
                                   (std::exp(.045 * (end - middle) / 365.) - 1.) * std::exp(-.04 * time(end)));
    const double principal = 110. * (std::exp(-.04 * time(start)) - std::exp(-.04 * time(end))) +
                             100. * 1.1 * (std::exp(-.017 * time(end)) - std::exp(-.017 * time(start)));
    ASSERT_GT(foreign, 0.);
    ASSERT_NEAR(result.a_, a, 1e-10);
    ASSERT_NEAR(result.b_, coupons + principal, 1e-10);
    ASSERT_NEAR(result.price_, coupons + principal + .001 * a, 1e-10);
}

TEST(XccyGaussianMomentsTest, TestCanonicalTenorsComeFromSource) {
    auto config = TestXccy::Config();
    config.convention_.domesticLeg_.paymentFrequency_ = config.convention_.domesticIndex_.forecastTenor_ = PeriodLength_("6M");
    config.convention_.foreignLeg_.paymentFrequency_ = config.convention_.foreignIndex_.forecastTenor_ = PeriodLength_("3M");
    config.domesticRateFixing_.indexName_ = "IR:USD,LIBOR_6M_LCH";
    config.foreignRateFixing_.indexName_ = "IR:EUR,LIBOR_3M_LCH";
    const CrossCurrencySwap_ native(TestXccy::TODAY, TestXccy::START, TestXccy::END, .001, config);
    const auto instrument = NewXccyScriptInstrument(native, {"tenors", "USD.OIS"});
    auto spec = ModelSpec(TestXccy::TODAY, true);
    const auto usd = spec.market_.domesticBlock_, eur = spec.market_.foreignBlock_;
    spec.market_.domesticBlock_.reset(new CurveBlock_("domestic", "USD", usd->DiscountCurves(), {{PeriodLength_("6M"), Curve("USD", .045)}}));
    spec.market_.foreignBlock_.reset(new CurveBlock_("foreign", "EUR", eur->DiscountCurves(), {{PeriodLength_("3M"), Curve("EUR", .026)}}));
    const auto prepared = Prepare(instrument, spec);
    const XccyGaussianModelData_ model("tenors", spec);
    const auto result = EvaluateXccyGaussianMoments<double>(prepared, model);
    Tape::JointCurveBlock_<double> domestic, foreign;
    domestic.discountCurves[CollateralType_("OIS")] = &spec.market_.domesticBlock_->Discount(CollateralType_("OIS"));
    foreign.discountCurves[CollateralType_("OIS")] = &spec.market_.foreignBlock_->Discount(CollateralType_("OIS"));
    domestic.forwardCurves[PeriodLength_("6M")] = &spec.market_.domesticBlock_->Forward(PeriodLength_("6M"), CollateralType_("OIS"));
    foreign.forwardCurves[PeriodLength_("3M")] = &spec.market_.foreignBlock_->Forward(PeriodLength_("3M"), CollateralType_("OIS"));
    const XccyMarketView_<double> market{spec.market_.valuationTime_, *spec.market_.pair_, spec.market_.collateralCurrency_, 1.1, &domestic, &foreign,
                                         spec.market_.basis_.get()};
    ASSERT_NEAR(result.price_, EvaluateXccyScript(prepared, market), 1e-10);
}

TEST(XccyGaussianMomentsTest, TestOneLegSettledAndExpiredUseOutstandingOracle) {
    for (bool foreign : {false, true}) {
        const Date_ valuation(2027, 11, 5);
        auto spec = ModelSpec(valuation);
        spec.market_.domesticBlock_.reset(new CurveBlock_("USD", "USD", spec.market_.domesticBlock_->DiscountCurves(), {}));
        spec.market_.foreignBlock_.reset(new CurveBlock_("EUR", "EUR", spec.market_.foreignBlock_->DiscountCurves(), {}));
        const XccyGaussianModelData_ model("settled", spec);
        const auto instrument = Instrument(XccyNotionalMode_::Value_::MARK_TO_MARKET, foreign);
        MarketFixingSnapshot_::values_t history;
        history["IR:EUR,LIBOR_6M_LCH"][DateTime_(Date_(2027, 4, 30))] = .031;
        const Handle_<MarketFixingSnapshot_> snapshot(new MarketFixingSnapshot_(history));
        const auto price = Prepare(instrument, spec, "Price", snapshot), par = Prepare(instrument, spec, "BasisSpread", snapshot);
        const auto result = EvaluateXccyGaussianMoments<double>(price, model);
        const double alpha = (Date_(2027, 11, 4) - Date_(2027, 5, 4)) / 360.;
        const double signedAnnuity = (foreign ? -1. : 1.) * 100. * alpha * 1.1 * std::exp(-.017 * 3. / 365.);
        ASSERT_NEAR(result.b_, .031 * signedAnnuity, 1e-10);
        ASSERT_NEAR(result.a_, foreign ? signedAnnuity : 0., 1e-10);
        ASSERT_NEAR(result.price_, (.031 + (foreign ? .001 : 0.)) * signedAnnuity, 1e-10);
        if (foreign)
            ASSERT_NEAR(EvaluateXccyGaussianMoments<double>(par, model).Value(par), -.031, 1e-12);
        else
            ASSERT_THROW(static_cast<void>(EvaluateXccyGaussianMoments<double>(par, model).Value(par)), CalibrationError_);
        Tape::JointCurveBlock_<double> domestic, eur;
        domestic.discountCurves[CollateralType_("OIS")] = &spec.market_.domesticBlock_->Discount(CollateralType_("OIS"));
        eur.discountCurves[CollateralType_("OIS")] = &spec.market_.foreignBlock_->Discount(CollateralType_("OIS"));
        const XccyMarketView_<double> native{spec.market_.valuationTime_, *spec.market_.pair_, spec.market_.collateralCurrency_, 1.1, &domestic, &eur,
                                             spec.market_.basis_.get()};
        ASSERT_THROW(EvaluateXccyScript(price, native), CalibrationError_);
    }
    const auto expired = ModelSpec(Date_(2027, 11, 9));
    const XccyGaussianModelData_ model("expired", expired);
    const auto instrument = Instrument(XccyNotionalMode_::Value_::MARK_TO_MARKET);
    const auto price = Prepare(instrument, expired), par = Prepare(instrument, expired, "BasisSpread");
    const auto result = EvaluateXccyGaussianMoments<double>(price, model);
    ASSERT_EQ(result.a_, 0.);
    ASSERT_EQ(result.b_, 0.);
    ASSERT_EQ(result.price_, 0.);
    ASSERT_THROW(static_cast<void>(result.Value(par)), CalibrationError_);
}

TEST(XccyGaussianMomentsTest, TestParFailureOwnsInstrumentAndMeasure) {
    const auto expired = ModelSpec(Date_(2027, 11, 9));
    const XccyGaussianModelData_ model("expired", expired);
    const auto par = Prepare(Instrument(), expired, "BasisSpread");
    const auto result = EvaluateXccyGaussianMoments<double>(par, model);
    try {
        static_cast<void>(result.Value(par));
        FAIL();
    } catch (const CalibrationError_& error) {
        ASSERT_EQ(error.Failure().diagnostic_.code_, "InvalidCalibrationAnnuity");
        ASSERT_EQ(error.Failure().diagnostic_.instrumentId_, par.Instrument()->Id());
        ASSERT_EQ(error.Failure().diagnostic_.measureId_, "BasisSpread");
        ASSERT_EQ(error.Failure().diagnostic_.stage_, CalibrationStage_::Value_::Evaluate);
        ASSERT_TRUE(error.Failure().report_);
    }
}

TEST(XccyGaussianMomentsTest, TestNativeCommonSupportAndConvexity) {
    for (const auto mode : {XccyNotionalMode_::Value_::FIXED, XccyNotionalMode_::Value_::RESETTABLE, XccyNotionalMode_::Value_::MARK_TO_MARKET}) {
        const auto spec = ModelSpec(TestXccy::TODAY, true);
        const XccyGaussianModelData_ model("zero", spec);
        const auto price = Prepare(Instrument(mode), spec), par = Prepare(Instrument(mode), spec, "BasisSpread");
        ASSERT_NEAR(EvaluateXccyGaussianMoments<double>(price, model).price_, Native(price, spec), 1e-10);
        ASSERT_NEAR(EvaluateXccyGaussianMoments<double>(par, model).Value(par), Native(par, spec), 1e-12);
    }
    const auto spec = ModelSpec();
    const XccyGaussianModelData_ model("stochastic", spec);
    auto config = TestXccy::Config();
    config.convention_.foreignLeg_.paymentLag_ = 0;
    const CrossCurrencySwap_ native(TestXccy::TODAY, TestXccy::START, TestXccy::END, .001, config);
    const auto noLag = Prepare(NewXccyScriptInstrument(native, {"no-lag", "USD.OIS"}), spec);
    ASSERT_NEAR(EvaluateXccyGaussianMoments<double>(noLag, model).price_, Native(noLag, spec), 1e-10);
    const auto lag = Prepare(Instrument(), spec);
    ASSERT_GT(std::abs(EvaluateXccyGaussianMoments<double>(lag, model).price_ - Native(lag, spec)), 1e-6);
}

TEST(XccyGaussianMomentsTest, TestImmutableRebindingAndFutureSnapshot) {
    const auto spec = ModelSpec();
    const XccyGaussianModelData_ model("binding", spec);
    const auto instrument = Instrument(XccyNotionalMode_::Value_::MARK_TO_MARKET);
    const auto program = LowerXccyGaussianMoments(Prepare(instrument, spec));
    const auto before = EvaluateXccyGaussianMoments<double>(program, model);
    CalibrationParameterBindings_ bindings;
    bindings.values_ = {{"K", .002, CalibrationValueUnit_::Parse("BasisSpread")}, {"N", 220., CalibrationValueUnit_::Parse("Price:USD")}};
    const auto rebound = Prepare(BindCalibrationInstrumentParameters(instrument, bindings), spec);
    const auto after = EvaluateXccyGaussianMoments<double>(rebound, model);
    ASSERT_NEAR(after.price_, 2. * (before.b_ + .002 * before.a_), 1e-10);
    ASSERT_DOUBLE_EQ(EvaluateXccyGaussianMoments<double>(program, model).price_, before.price_);
    MarketFixingSnapshot_::values_t future;
    future["IR:USD,LIBOR_3M_LCH"][DateTime_(Date_(2027, 4, 30))] = .9;
    future["FX[EUR/USD]"][DateTime_(Date_(2027, 8, 4))] = 9.;
    const auto supplied = Prepare(instrument, spec, "Price", Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_(future)));
    ASSERT_DOUBLE_EQ(EvaluateXccyGaussianMoments<double>(supplied, model).price_, before.price_);
    ASSERT_NE(supplied.ContextIdentity(), program.Prepared().ContextIdentity());
}

TEST(XccyGaussianMomentsTest, TestSnapshotNativeMapAndSmallSigma) {
    const auto spec = NonflatSpec();
    const XccyGaussianModelData_ model("native-map", spec);
    const auto inputs = XccyGaussianMomentInputs(model);
    const auto runtime = NewXccyGaussianModel(model);
    ASSERT_EQ(inputs.labels_, runtime->ParameterLabels());
    const auto prepared = Prepare(Instrument(XccyNotionalMode_::Value_::RESETTABLE), spec);
    Vector_<String_> labels;
    const auto map = XccyGaussianMomentNativeMap(model, &labels);
    ASSERT_EQ(map.Rows(), map.Cols());
    for (int i = 0; i < map.Rows(); ++i)
        for (int j = 0; j < map.Cols(); ++j)
            ASSERT_EQ(map(i, j), i == j ? 1. : 0.);
    auto small = ModelSpec();
    small.dynamics_.domesticSigma_ = {1e-8};
    const XccyGaussianModelData_ smallModel("small", small);
    const auto active = XccyGaussianMomentInputs(smallModel);
    const auto jacobian = TapeResidualJacobian(active.values_, [&](const Vector_<AAD::Number_>& p) {
        return Vector_<AAD::Number_>{EvaluateXccyGaussianMoments<AAD::Number_>(Prepare(Instrument(), small), smallModel, p).price_};
    });
    const auto& matrix = static_cast<const XCurveJacobian_&>(*jacobian).j_;
    ASSERT_TRUE(std::isfinite(matrix(0, static_cast<int>(active.sigmaOffsets_[0]))));
    const auto zero = ModelSpec(TestXccy::TODAY, true);
    const XccyGaussianModelData_ zeroModel("zero", zero);
    ASSERT_THROW(static_cast<void>(EvaluateXccyGaussianMoments<AAD::Number_>(Prepare(Instrument(), zero), zeroModel)), CalibrationError_);
}

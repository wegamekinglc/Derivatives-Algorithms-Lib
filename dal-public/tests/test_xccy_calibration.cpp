//
// Created by wegamekinglc on 2026/6/20.
//

#include <gtest/gtest.h>

#include <dal-public/src/curvedata.hpp>
#include <dal-public/src/curveinstrument.hpp>
#include <dal-public/src/curveprotocol.hpp>
#include <dal-public/src/xccycalibration.hpp>

using Dal::CalibrateXccyMarket;
using Dal::CollateralType_OIS;
using Dal::CrossCurrencyCalibrationSpecBuilder_;
using Dal::CrossCurrencySwapNew;
using Dal::CurrencyPair_New;
using Dal::CurveBlockNew;
using Dal::CurveSolveMode_;
using Dal::Date_;
using Dal::DateTime_;
using Dal::DayBasis_New;
using Dal::DiscountPWLFNew;
using Dal::JointCurveDeclaration_;
using Dal::JointXccyCalibrationOptions_;
using Dal::JointXccyCalibrationResult_;
using Dal::JointXccyCalibrationSpecBuilder_;
using Dal::MarketFixingSnapshotNew;
using Dal::PeriodLength_New;
using Dal::RateIndexConvention_;
using Dal::RateIndexConvention_New;
using Dal::RateLegConvention_;
using Dal::RateLegConvention_New;
using Dal::String_;
using Dal::Vector_;

namespace {

    Date_ Today() { return Date_(2025, 6, 20); }
    Date_ Spot() { return Today().AddDays(2); }

    Dal::RateLegConvention_ Fixed6M365F() { return RateLegConvention_New(PeriodLength_New("6M"), DayBasis_New("ACT_365F")); }

    Dal::RateLegConvention_ Fixed6M360() { return RateLegConvention_New(PeriodLength_New("6M"), DayBasis_New("ACT_360")); }

    Dal::RateIndexConvention_ Libor3M() { return RateIndexConvention_New(PeriodLength_New("3M"), DayBasis_New("ACT_360"), CollateralType_OIS()); }

    Dal::RateIndexConvention_ Euribor6M() { return RateIndexConvention_New(PeriodLength_New("6M"), DayBasis_New("ACT_360"), CollateralType_OIS()); }

    Dal::RateLegConvention_ FloatLeg(const char* tenor, const char* basis) {
        return RateLegConvention_New(PeriodLength_New(tenor), DayBasis_New(basis));
    }

} // namespace

// CrossCurrencyCalibrationSpecBuilder_ defaults

TEST(XccyCalibrationTest, TestBuilderDefaults) {
    CrossCurrencyCalibrationSpecBuilder_ builder;
    ASSERT_NEAR(builder.tolerance_, 1.0e-10, 1e-15);
    ASSERT_NEAR(builder.fitTolerance_, 1.0e-6, 1e-15);
    ASSERT_EQ(builder.maxEvaluations_, 200);
    ASSERT_EQ(builder.maxRestarts_, 20);
    ASSERT_NEAR(builder.initialGuess_, 0.0, 1e-15);
    ASSERT_NEAR(builder.smoothingWeight_, 1.0, 1e-15);
    ASSERT_NEAR(builder.fxSpot_, 0.0, 1e-15);
    ASSERT_EQ(builder.solveMode_.Switch(), CurveSolveMode_::Value_::EXACT);
}

TEST(XccyCalibrationTest, TestJointBuilderDefaultsMatchCoreJointCalibration) {
    JointXccyCalibrationSpecBuilder_ builder;

    ASSERT_NEAR(builder.solverOptions_.tolerance_, 1.0e-8, 1.0e-15);
    ASSERT_NEAR(builder.solverOptions_.fitTolerance_, 1.0e-6, 1.0e-15);
    ASSERT_NEAR(builder.solverOptions_.initialGuess_, 0.0, 1.0e-15);
    ASSERT_EQ(builder.solverOptions_.maxEvaluations_, 200);
    ASSERT_EQ(builder.solverOptions_.maxRestarts_, 20);
    ASSERT_EQ(builder.solverOptions_.solveMode_.Switch(), CurveSolveMode_::Value_::EXACT);
}

// Build baseline curves for XCCY calibration

namespace {

    struct BaselineCurves_ {
        Dal::Handle_<Dal::CurveBlock_> domesticBlock_;
        Dal::Handle_<Dal::CurveBlock_> foreignBlock_;
    };

    BaselineCurves_ MakeBaselineCurves() {
        Vector_<Date_> knotDates;
        knotDates.push_back(Spot());
        knotDates.push_back(Spot().AddDays(3650)); // 10 years
        Vector_<> oisRates;
        oisRates.push_back(0.04);
        oisRates.push_back(0.04);

        auto usdOis = DiscountPWLFNew(String_("usd_ois"), String_("USD"), knotDates, oisRates);
        auto eurOis = DiscountPWLFNew(String_("eur_ois"), String_("EUR"), knotDates, oisRates);

        auto usdBlock = CurveBlockNew(usdOis, DayBasis_New("ACT_365F"));
        auto eurBlock = CurveBlockNew(eurOis, DayBasis_New("ACT_360"));

        BaselineCurves_ result;
        result.domesticBlock_ = usdBlock;
        result.foreignBlock_ = eurBlock;
        return result;
    }

} // namespace

// Cross-currency calibration

TEST(XccyCalibrationTest, TestCalibrateXccyMarket) {
    auto curves = MakeBaselineCurves();

    CrossCurrencyCalibrationSpecBuilder_ builder;
    builder.today_ = Today();
    builder.basisPair_ = CurrencyPair_New("USD", "EUR");
    builder.domesticCurveBlock_ = curves.domesticBlock_;
    builder.foreignCurveBlock_ = curves.foreignBlock_;
    builder.fxSpot_ = 1.10;
    builder.tolerance_ = 1.0e-8;
    builder.solveMode_ = CurveSolveMode_::Value_::APPROXIMATE;
    builder.initialGuess_ = 0.01;

    // Cross-currency swaps: USD fixed vs EUR float + spread
    auto usdLeg = Fixed6M365F();
    auto usdIndex = Libor3M();
    auto eurLeg = FloatLeg("6M", "ACT_360");
    auto eurIndex = Euribor6M();
    auto currencies = CurrencyPair_New("USD", "EUR");

    Vector_<Date_> knotDates;
    for (int y : {2, 5, 10}) {
        Date_ maturity = Spot().AddDays(y * 365);
        knotDates.push_back(maturity);
        builder.instruments_.push_back(
            CrossCurrencySwapNew(Today(), Spot(), maturity, 0.01, currencies, 100.0, 100.0 / 1.10, usdLeg, usdIndex, eurLeg, eurIndex));
    }
    builder.knotDates_ = knotDates;

    auto spec = builder.Build();
    auto result = CalibrateXccyMarket(spec);

    ASSERT_GT(result.diagnostics_.marketRates_.size(), static_cast<size_t>(0));
    ASSERT_EQ(result.diagnostics_.marketRates_.size(), result.diagnostics_.modelRates_.size());
    ASSERT_LT(result.diagnostics_.maxAbsResidual_, 1.0e-4);
    ASSERT_LT(result.diagnostics_.rmsResidual_, 1.0e-4);

    // Verify FX forward curve is populated
    ASSERT_GT(result.fxForwardCurve_.dates_.size(), static_cast<size_t>(0));
    ASSERT_EQ(result.fxForwardCurve_.dates_.size(), result.fxForwardCurve_.forwards_.size());
}

// Round-trip every builder field through Build() to guard against brace-init
// order/count drift between the builder and CrossCurrencyCalibrationSpec_.

TEST(XccyCalibrationTest, TestBuildRoundTripsEveryField) {
    auto curves = MakeBaselineCurves();

    CrossCurrencyCalibrationSpecBuilder_ b;
    b.today_ = Today();
    b.valuationTime_ = DateTime_(Today(), 9, 45);
    b.collateralCurrency_ = Dal::Ccy_("USD");
    b.fixings_ = MarketFixingSnapshotNew({{"USD-SOFR-3M", {{DateTime_(Today().AddDays(-1), 11, 0), 0.0425}}}});
    b.basisPair_ = CurrencyPair_New("USD", "EUR");
    b.domesticCurveBlock_ = curves.domesticBlock_;
    b.foreignCurveBlock_ = curves.foreignBlock_;
    b.fxSpot_ = 1.0825;
    b.fxForwardCollateral_ = CollateralType_OIS();
    b.smoothingWeight_ = 1.25;
    b.tolerance_ = 5.0e-11;
    b.fitTolerance_ = 6.0e-7;
    b.initialGuess_ = 0.0025;
    b.maxEvaluations_ = 233;
    b.maxRestarts_ = 17;
    b.solveMode_ = CurveSolveMode_::Value_::APPROXIMATE;
    b.knotDates_ = Vector_<Date_>{Spot(), Spot().AddDays(3650)};
    b.instruments_ = Vector_<Dal::Handle_<Dal::CrossCurrencySwap_>>{};

    auto spec = b.Build();

    ASSERT_EQ(spec.today_, b.today_);
    ASSERT_EQ(spec.valuationTime_, b.valuationTime_);
    ASSERT_EQ(spec.collateralCurrency_, b.collateralCurrency_);
    ASSERT_EQ(spec.fixings_.get(), b.fixings_.get());
    ASSERT_TRUE(spec.basisPair_ == b.basisPair_);
    ASSERT_EQ(spec.domesticCurveBlock_.get(), b.domesticCurveBlock_.get());
    ASSERT_EQ(spec.foreignCurveBlock_.get(), b.foreignCurveBlock_.get());
    ASSERT_NEAR(spec.fxSpot_, 1.0825, 1e-15);
    ASSERT_EQ(spec.fxForwardCollateral_.Switch(), b.fxForwardCollateral_.Switch());
    ASSERT_NEAR(spec.smoothingWeight_, 1.25, 1e-15);
    ASSERT_NEAR(spec.tolerance_, 5.0e-11, 1e-15);
    ASSERT_NEAR(spec.fitTolerance_, 6.0e-7, 1e-15);
    ASSERT_NEAR(spec.initialGuess_, 0.0025, 1e-15);
    ASSERT_EQ(spec.maxEvaluations_, 233);
    ASSERT_EQ(spec.maxRestarts_, 17);
    ASSERT_EQ(spec.solveMode_.Switch(), CurveSolveMode_::Value_::APPROXIMATE);
    ASSERT_EQ(spec.knotDates_.size(), static_cast<size_t>(2));
    ASSERT_TRUE(spec.instruments_.empty());
}

TEST(XccyCalibrationTest, TestJointBuilderRoundTripsEveryField) {
    const DateTime_ valuationTime(Today(), 9, 45);
    const DateTime_ fixingTime(Today().AddDays(-1), 11, 0);
    const auto fixings = MarketFixingSnapshotNew({{"USD-SOFR-3M", {{fixingTime, 0.0425}}}});

    JointCurveDeclaration_ domesticDiscount;
    domesticDiscount.curveName_ = "usd_ois";
    domesticDiscount.instruments_.push_back(DepositNew(Today(), Spot(), Spot().AddDays(30), 0.04, Libor3M()));
    domesticDiscount.knotDates_ = {Spot().AddDays(30), Spot().AddDays(365)};
    domesticDiscount.targetCollateral_ = CollateralType_OIS();
    domesticDiscount.targetTenor_ = PeriodLength_New("1M");
    domesticDiscount.calibrateDiscountCurve_ = true;
    domesticDiscount.baseLayeredOverDiscount_ = false;
    domesticDiscount.parameterization_ = Dal::CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD;
    domesticDiscount.logDfScheme_ = Dal::LogDfScheme_::Value_::LOG_CUBIC_NATURAL;
    domesticDiscount.smoothingWeight_ = 1.25;
    domesticDiscount.initialGuessPerNode_ = {0.01, 0.02};

    JointCurveDeclaration_ foreignForward;
    foreignForward.curveName_ = "eur_6m";
    foreignForward.instruments_.push_back(DepositNew(Today(), Spot(), Spot().AddDays(180), 0.03, Euribor6M()));
    foreignForward.knotDates_ = {Spot().AddDays(180), Spot().AddDays(730)};
    foreignForward.targetCollateral_ = CollateralType_OIS();
    foreignForward.targetTenor_ = PeriodLength_New("6M");
    foreignForward.calibrateDiscountCurve_ = false;
    foreignForward.baseLayeredOverDiscount_ = true;
    foreignForward.parameterization_ = Dal::CurveParameterization_::Value_::ZERO_RATE;
    foreignForward.logDfScheme_ = Dal::LogDfScheme_::Value_::LOG_LINEAR;
    foreignForward.smoothingWeight_ = 1.75;
    foreignForward.initialGuessPerNode_ = {0.03, 0.04};

    const auto xccy = CrossCurrencySwapNew(Today(), Spot(), Spot().AddDays(3650), 0.001, CurrencyPair_New("USD", "EUR"));

    JointXccyCalibrationSpecBuilder_ builder;
    builder.valuationTime_ = valuationTime;
    builder.pair_ = CurrencyPair_New("USD", "EUR");
    builder.collateralCurrency_ = Dal::Ccy_("USD");
    builder.fxSpot_ = 1.0825;
    builder.domestic_.ccy_ = Dal::Ccy_("USD");
    builder.domestic_.liborBasis_ = DayBasis_New("ACT_360");
    builder.domestic_.curves_ = {domesticDiscount};
    builder.foreign_.ccy_ = Dal::Ccy_("EUR");
    builder.foreign_.liborBasis_ = DayBasis_New("ACT_365F");
    builder.foreign_.curves_ = {foreignForward};
    builder.basis_.curveName_ = "usd_eur_basis";
    builder.basis_.instruments_ = {xccy};
    builder.basis_.knotDates_ = {Spot().AddDays(3650)};
    builder.basis_.parameterization_ = Dal::CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD;
    builder.basis_.smoothingWeight_ = 2.25;
    builder.basis_.initialGuessPerNode_ = {0.0025};
    builder.fixings_ = fixings;
    builder.solverOptions_.smoothingWeight_ = 9.5;
    builder.solverOptions_.tolerance_ = 5.0e-11;
    builder.solverOptions_.fitTolerance_ = 6.0e-7;
    builder.solverOptions_.initialGuess_ = 0.0075;
    builder.solverOptions_.maxEvaluations_ = 233;
    builder.solverOptions_.maxRestarts_ = 17;
    builder.solverOptions_.solveMode_ = CurveSolveMode_::Value_::APPROXIMATE;

    const auto spec = builder.Build();

    ASSERT_EQ(spec.valuationTime_, valuationTime);
    ASSERT_TRUE(spec.pair_ == builder.pair_);
    ASSERT_EQ(spec.collateralCurrency_, builder.collateralCurrency_);
    ASSERT_NEAR(spec.fxSpot_, 1.0825, 1.0e-15);
    ASSERT_EQ(spec.domestic_.ccy_, builder.domestic_.ccy_);
    ASSERT_EQ(spec.domestic_.liborBasis_.String(), builder.domestic_.liborBasis_.String());
    ASSERT_EQ(spec.domestic_.curves_.size(), static_cast<size_t>(1));
    ASSERT_EQ(spec.domestic_.curves_.front().curveName_, domesticDiscount.curveName_);
    ASSERT_EQ(spec.domestic_.curves_.front().instruments_.front().get(), domesticDiscount.instruments_.front().get());
    ASSERT_EQ(spec.domestic_.curves_.front().knotDates_, domesticDiscount.knotDates_);
    ASSERT_EQ(spec.domestic_.curves_.front().targetCollateral_, domesticDiscount.targetCollateral_);
    ASSERT_EQ(spec.domestic_.curves_.front().targetTenor_, domesticDiscount.targetTenor_);
    ASSERT_EQ(spec.domestic_.curves_.front().calibrateDiscountCurve_, domesticDiscount.calibrateDiscountCurve_);
    ASSERT_EQ(spec.domestic_.curves_.front().baseLayeredOverDiscount_, domesticDiscount.baseLayeredOverDiscount_);
    ASSERT_EQ(spec.domestic_.curves_.front().parameterization_, domesticDiscount.parameterization_);
    ASSERT_EQ(spec.domestic_.curves_.front().logDfScheme_, domesticDiscount.logDfScheme_);
    ASSERT_NEAR(spec.domestic_.curves_.front().smoothingWeight_, domesticDiscount.smoothingWeight_, 1.0e-15);
    ASSERT_EQ(spec.domestic_.curves_.front().initialGuessPerNode_, domesticDiscount.initialGuessPerNode_);
    ASSERT_EQ(spec.foreign_.ccy_, builder.foreign_.ccy_);
    ASSERT_EQ(spec.foreign_.liborBasis_.String(), builder.foreign_.liborBasis_.String());
    ASSERT_EQ(spec.foreign_.curves_.size(), static_cast<size_t>(1));
    ASSERT_EQ(spec.foreign_.curves_.front().curveName_, foreignForward.curveName_);
    ASSERT_EQ(spec.foreign_.curves_.front().instruments_.front().get(), foreignForward.instruments_.front().get());
    ASSERT_EQ(spec.foreign_.curves_.front().knotDates_, foreignForward.knotDates_);
    ASSERT_EQ(spec.foreign_.curves_.front().targetCollateral_, foreignForward.targetCollateral_);
    ASSERT_EQ(spec.foreign_.curves_.front().targetTenor_, foreignForward.targetTenor_);
    ASSERT_EQ(spec.foreign_.curves_.front().calibrateDiscountCurve_, foreignForward.calibrateDiscountCurve_);
    ASSERT_EQ(spec.foreign_.curves_.front().baseLayeredOverDiscount_, foreignForward.baseLayeredOverDiscount_);
    ASSERT_EQ(spec.foreign_.curves_.front().parameterization_, foreignForward.parameterization_);
    ASSERT_EQ(spec.foreign_.curves_.front().logDfScheme_, foreignForward.logDfScheme_);
    ASSERT_NEAR(spec.foreign_.curves_.front().smoothingWeight_, foreignForward.smoothingWeight_, 1.0e-15);
    ASSERT_EQ(spec.foreign_.curves_.front().initialGuessPerNode_, foreignForward.initialGuessPerNode_);
    ASSERT_EQ(spec.basis_.curveName_, builder.basis_.curveName_);
    ASSERT_EQ(spec.basis_.instruments_.front().get(), xccy.get());
    ASSERT_EQ(spec.basis_.knotDates_, builder.basis_.knotDates_);
    ASSERT_EQ(spec.basis_.parameterization_, builder.basis_.parameterization_);
    ASSERT_NEAR(spec.basis_.smoothingWeight_, 2.25, 1.0e-15);
    ASSERT_EQ(spec.basis_.initialGuessPerNode_, builder.basis_.initialGuessPerNode_);
    ASSERT_EQ(spec.fixings_.get(), fixings.get());
    ASSERT_NEAR(spec.tolerance_, 5.0e-11, 1.0e-15);
    ASSERT_NEAR(spec.fitTolerance_, 6.0e-7, 1.0e-15);
    ASSERT_NEAR(spec.initialGuess_, 0.0075, 1.0e-15);
    ASSERT_EQ(spec.maxEvaluations_, 233);
    ASSERT_EQ(spec.maxRestarts_, 17);
    ASSERT_EQ(spec.solveMode_.Switch(), CurveSolveMode_::Value_::APPROXIMATE);
}

TEST(XccyCalibrationTest, TestJointOptionsAndResultLayoutArePublic) {
    JointXccyCalibrationOptions_ options;
    options.jacobianMode_ = Dal::CurveJacobianMode_::Value_::BUMPED;
    options.computeEffJacobianInverse_ = false;
    options.computeForwardJacobian_ = false;
    using CalibrateWithOptions_ = JointXccyCalibrationResult_ (*)(const Dal::JointXccyCalibrationSpec_&, const JointXccyCalibrationOptions_&);
    const CalibrateWithOptions_ calibrate = static_cast<CalibrateWithOptions_>(&Dal::CalibrateJointXccyMarket);

    ASSERT_NE(calibrate, nullptr);
    ASSERT_EQ(options.jacobianMode_.Switch(), Dal::CurveJacobianMode_::Value_::BUMPED);
    ASSERT_FALSE(options.computeEffJacobianInverse_);
    ASSERT_FALSE(options.computeForwardJacobian_);

    JointXccyCalibrationResult_ result;
    result.parameterRanges_ = {{"domestic:usd_ois", 0, 2}, {"basis:usd_eur", 2, 1}};
    result.residualRanges_ = {{"domestic:usd_ois", 0, 2}, {"xccy:usd_eur", 2, 1}};
    result.marketRates_ = {0.04, 0.041, 0.001};
    result.modelRates_ = {0.04, 0.041, 0.001};
    result.residuals_ = {0.0, 0.0, 0.0};

    ASSERT_EQ(result.parameterRanges_.size(), static_cast<size_t>(2));
    ASSERT_EQ(result.parameterRanges_[1].name_, Dal::String_("basis:usd_eur"));
    ASSERT_EQ(result.parameterRanges_[1].offset_, 2);
    ASSERT_EQ(result.parameterRanges_[1].size_, 1);
    ASSERT_EQ(result.residualRanges_.size(), static_cast<size_t>(2));
    ASSERT_EQ(result.residualRanges_[1].name_, Dal::String_("xccy:usd_eur"));
    ASSERT_EQ(result.marketRates_.size(), static_cast<size_t>(3));
    ASSERT_EQ(result.modelRates_.size(), static_cast<size_t>(3));
    ASSERT_EQ(result.residuals_.size(), static_cast<size_t>(3));
}

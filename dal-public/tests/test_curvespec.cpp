//
// Created by wegamekinglc on 2026/6/20.
//

#include <gtest/gtest.h>

#include <dal-public/src/curvedata.hpp>
#include <dal-public/src/curveinstrument.hpp>
#include <dal-public/src/curveprotocol.hpp>
#include <dal-public/src/curvespec.hpp>

using Dal::CalibrateMultiCurveBundle;
using Dal::CalibrateSingleCurve;
using Dal::CalibrationResult_;
using Dal::CollateralType_OIS;
using Dal::CurveCalibrationSpecBuilder_;
using Dal::CurveKnotPolicy_;
using Dal::CurveParameterization_;
using Dal::CurveSolveMode_;
using Dal::CurveSolverOptions_;
using Dal::Date_;
using Dal::DayBasis_New;
using Dal::DepositNew;
using Dal::DiscountPWLFNew;
using Dal::DiscountZeroRate_;
using Dal::DiscountZeroRateNew;
using Dal::LogDfScheme_;
using Dal::MultiCurveCalibrationSpec_;
using Dal::OISSwapNew;
using Dal::PeriodLength_New;
using Dal::RateIndexConvention_;
using Dal::RateIndexConvention_New;
using Dal::RateLegConvention_;
using Dal::RateLegConvention_New;
using Dal::String_;
using Dal::SwapNew;
using Dal::Vector_;

namespace {

    Date_ Today() { return Date_(2025, 6, 20); }
    Date_ Spot() { return Today().AddDays(2); }

    Dal::RateLegConvention_ Fixed6M() { return RateLegConvention_New(PeriodLength_New("6M"), DayBasis_New("ACT_365F")); }

    Dal::RateLegConvention_ Float3M() { return RateLegConvention_New(PeriodLength_New("3M"), DayBasis_New("ACT_360")); }

    Dal::RateIndexConvention_ Libor3M() { return RateIndexConvention_New(PeriodLength_New("3M"), DayBasis_New("ACT_360"), CollateralType_OIS()); }

    Dal::RateIndexConvention_ OvernightIndex() {
        return RateIndexConvention_New(PeriodLength_New("12M"), DayBasis_New("ACT_360"), CollateralType_OIS());
    }

} // namespace

// CurveCalibrationSpecBuilder_ defaults

TEST(CurveSpecTest, TestBuilderDefaults) {
    CurveCalibrationSpecBuilder_ builder;
    ASSERT_EQ(builder.curveName_, String_("calibrated"));
    ASSERT_TRUE(builder.calibrateDiscountCurve_);
    ASSERT_NEAR(builder.tolerance_, 1.0e-8, 1e-15);
    ASSERT_NEAR(builder.fitTolerance_, 1.0e-6, 1e-15);
    ASSERT_EQ(builder.maxEvaluations_, 200);
    ASSERT_EQ(builder.maxRestarts_, 20);
    ASSERT_NEAR(builder.initialGuess_, 0.05, 1e-15);
    ASSERT_NEAR(builder.smoothingWeight_, 1.0, 1e-15);
    ASSERT_EQ(builder.solveMode_.Switch(), CurveSolveMode_::Value_::EXACT);
    ASSERT_EQ(builder.parameterization_.Switch(), CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD);
}

TEST(CurveSpecTest, TestSingleCurveOptionsOverloadIsPublicAndAdditive) {
    using OptionsOverload_ = Dal::CalibrationResult_ (*)(const Dal::CurveCalibrationSpec_&, const Dal::CurveCalibrationOptions_&);
    const OptionsOverload_ overload = static_cast<OptionsOverload_>(&Dal::CalibrateSingleCurve);
    ASSERT_NE(overload, nullptr);

    Dal::CurveCalibrationOptions_ options;
    EXPECT_EQ(options.jacobianMode_, Dal::CurveJacobianMode_::Value_::ANALYTIC);
    EXPECT_TRUE(options.computeForwardJacobian_);
    EXPECT_TRUE(options.computeEffJacobianInverse_);
}

// Single-curve calibration (EXACT, PIECEWISE_LINEAR_FWD)

TEST(CurveSpecTest, TestCalibrateSingleCurveExact) {
    // Build a flat 4% OIS market using OIS swaps
    CurveCalibrationSpecBuilder_ builder;
    builder.today_ = Today();
    builder.ccy_ = "USD";
    builder.curveName_ = "ois";
    builder.calibrateDiscountCurve_ = true;
    builder.tolerance_ = 1.0e-8;
    builder.initialGuess_ = 0.04;

    Vector_<Date_> knotDates;
    for (int y : {2, 5, 10}) {
        Date_ maturity = Spot().AddDays(y * 365);
        knotDates.push_back(maturity);
        builder.instruments_.push_back(OISSwapNew(Today(), Spot(), maturity, 0.04, Fixed6M(), OvernightIndex(), Float3M()));
    }
    builder.knotDates_ = knotDates;

    auto spec = builder.Build();
    CalibrationResult_ result = CalibrateSingleCurve(spec);

    ASSERT_TRUE(result.curve_ != nullptr);
    ASSERT_GT(result.diagnostics_.marketRates_.size(), static_cast<size_t>(0));
    ASSERT_EQ(result.diagnostics_.marketRates_.size(), result.diagnostics_.modelRates_.size());
    ASSERT_LT(result.diagnostics_.maxAbsResidual_, 1.0e-6);
    ASSERT_LT(result.diagnostics_.rmsResidual_, 1.0e-6);
}

// Single-curve calibration with Jacobian

TEST(CurveSpecTest, TestCalibrateSingleCurveWithBumpedJacobian) {
    // Calibration with BUMPED Jacobian mode (uses APPROXIMATE + PIECEWISE_LINEAR_FWD)
    CurveCalibrationSpecBuilder_ builder;
    builder.today_ = Today();
    builder.ccy_ = "USD";
    builder.curveName_ = "ois_jac_bumped";
    builder.calibrateDiscountCurve_ = true;
    builder.initialGuess_ = 0.04;
    builder.solveMode_ = CurveSolveMode_::Value_::APPROXIMATE;
    builder.parameterization_ = CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD;

    Vector_<Date_> knotDates;
    for (int y : {2, 5, 10}) {
        Date_ maturity = Spot().AddDays(y * 365);
        knotDates.push_back(maturity);
        builder.instruments_.push_back(OISSwapNew(Today(), Spot(), maturity, 0.04, Fixed6M(), OvernightIndex(), Float3M()));
    }
    builder.knotDates_ = knotDates;

    auto spec = builder.Build();
    CalibrationResult_ result = CalibrateSingleCurve(spec, Dal::CurveJacobianMode_::Value_::BUMPED);

    ASSERT_TRUE(result.curve_ != nullptr);
    ASSERT_LT(result.diagnostics_.maxAbsResidual_, 1.0e-6);
    // BUMPED mode completes successfully and returns a calibrated curve
    SUCCEED();
}

TEST(CurveSpecTest, TestCalibrateSingleCurveWithAnalyticJacobian) {
    // LOG_DISCOUNT requires knot 0 == today (anchor), and n+1 knots for n instruments
    CurveCalibrationSpecBuilder_ builder;
    builder.today_ = Today();
    builder.ccy_ = "USD";
    builder.curveName_ = "ois_analytic";
    builder.calibrateDiscountCurve_ = true;
    builder.initialGuess_ = 0.04;
    builder.parameterization_ = CurveParameterization_::Value_::LOG_DISCOUNT;

    Vector_<Date_> knotDates;
    knotDates.push_back(Today()); // anchor knot
    for (int y : {2, 5, 10}) {
        Date_ maturity = Spot().AddDays(y * 365);
        knotDates.push_back(maturity);
        builder.instruments_.push_back(OISSwapNew(Today(), Spot(), maturity, 0.04, Fixed6M(), OvernightIndex(), Float3M()));
    }
    builder.knotDates_ = knotDates;

    auto spec = builder.Build();
    CalibrationResult_ result = CalibrateSingleCurve(spec, Dal::CurveJacobianMode_::Value_::ANALYTIC);

    ASSERT_TRUE(result.curve_ != nullptr);
    ASSERT_LT(result.diagnostics_.maxAbsResidual_, 1.0e-6);
    // ANALYTIC Jacobian should produce a non-empty Jacobian
    ASSERT_GT(result.diagnostics_.jacobian_.Rows(), 0);
    ASSERT_GT(result.diagnostics_.jacobian_.Cols(), 0);
}

TEST(CurveSpecTest, TestCalibrateSingleCurveZeroRateWithAnalyticJacobian) {
    CurveCalibrationSpecBuilder_ builder;
    builder.today_ = Today();
    builder.ccy_ = "USD";
    builder.curveName_ = "ois_zero_rate_analytic";
    builder.calibrateDiscountCurve_ = true;
    builder.initialGuess_ = 0.04;
    builder.parameterization_ = CurveParameterization_::Value_::ZERO_RATE;
    builder.logDfScheme_ = LogDfScheme_::Value_::LOG_CUBIC_NATURAL;

    for (int y : {2, 5, 10}) {
        const Date_ maturity = Spot().AddDays(y * 365);
        builder.knotDates_.push_back(maturity);
        builder.instruments_.push_back(OISSwapNew(Today(), Spot(), maturity, 0.04, Fixed6M(), OvernightIndex(), Float3M()));
    }

    const auto spec = builder.Build();
    const CalibrationResult_ result = CalibrateSingleCurve(spec, Dal::CurveJacobianMode_::Value_::ANALYTIC);

    ASSERT_NE(dynamic_cast<const DiscountZeroRate_*>(result.curve_.get()), nullptr);
    ASSERT_LT(result.diagnostics_.maxAbsResidual_, 1.0e-6);
    ASSERT_EQ(result.diagnostics_.jacobian_.Rows(), 3);
    ASSERT_EQ(result.diagnostics_.jacobian_.Cols(), 3);
}

TEST(CurveSpecTest, TestCalibrateSingleCurveZeroRateWithBaseCurve) {
    CurveCalibrationSpecBuilder_ builder;
    builder.today_ = Today();
    builder.ccy_ = "USD";
    builder.curveName_ = "ois_zero_rate_total";
    builder.calibrateDiscountCurve_ = true;
    builder.initialGuess_ = 0.04;
    builder.parameterization_ = CurveParameterization_::Value_::ZERO_RATE;
    builder.logDfScheme_ = LogDfScheme_::Value_::LOG_LINEAR;

    for (int y : {2, 5, 10}) {
        const Date_ maturity = Spot().AddDays(y * 365);
        builder.knotDates_.push_back(maturity);
        builder.instruments_.push_back(OISSwapNew(Today(), Spot(), maturity, 0.04, Fixed6M(), OvernightIndex(), Float3M()));
    }

    const CalibrationResult_ totalResult = CalibrateSingleCurve(builder.Build(), Dal::CurveJacobianMode_::Value_::ANALYTIC);
    const auto* totalCurve = dynamic_cast<const DiscountZeroRate_*>(totalResult.curve_.get());
    ASSERT_NE(totalCurve, nullptr);
    ASSERT_LT(totalResult.diagnostics_.maxAbsResidual_, 1.0e-6);

    const Vector_<> baseRates{0.01, 0.01, 0.01};
    builder.curveName_ = "ois_zero_rate_spread";
    builder.baseCurve_ = DiscountZeroRateNew("ois_zero_rate_base", "USD", Today(), builder.knotDates_, baseRates);

    const CalibrationResult_ spreadResult = CalibrateSingleCurve(builder.Build(), Dal::CurveJacobianMode_::Value_::ANALYTIC);
    const auto* spreadCurve = dynamic_cast<const DiscountZeroRate_*>(spreadResult.curve_.get());
    ASSERT_NE(spreadCurve, nullptr);
    ASSERT_LT(spreadResult.diagnostics_.maxAbsResidual_, 1.0e-6);

    const Vector_<> totalRates = totalCurve->NodeZeroRates();
    const Vector_<> spreadRates = spreadCurve->NodeZeroRates();
    ASSERT_EQ(totalRates.size(), baseRates.size());
    ASSERT_EQ(spreadRates.size(), baseRates.size());
    for (size_t i = 0; i < baseRates.size(); ++i) {
        ASSERT_NEAR(totalRates[i], baseRates[i] + spreadRates[i], 1.0e-8);
        ASSERT_NEAR((*totalCurve)(Today(), builder.knotDates_[i]), (*spreadCurve)(Today(), builder.knotDates_[i]), 1.0e-10);
    }
}

// Multi-curve calibration (sequential)

TEST(CurveSpecTest, TestCalibrateMultiCurveBundle) {
    MultiCurveCalibrationSpec_ multiSpec;
    multiSpec.name_ = "usd_multi";
    multiSpec.ccy_ = "USD";
    multiSpec.liborBasis_ = DayBasis_New("ACT_365F");

    // Stage 1: calibrate OIS discount curve (single-stage multi-curve)
    {
        CurveCalibrationSpecBuilder_ stageBuilder;
        stageBuilder.today_ = Today();
        stageBuilder.ccy_ = "USD";
        stageBuilder.curveName_ = "ois_dc";
        stageBuilder.calibrateDiscountCurve_ = true;
        stageBuilder.solveMode_ = Dal::CurveSolveMode_::Value_::APPROXIMATE;
        stageBuilder.initialGuess_ = 0.04;

        Vector_<Date_> knotDates;
        for (int y : {2, 5, 10}) {
            Date_ maturity = Spot().AddDays(y * 365);
            knotDates.push_back(maturity);
            stageBuilder.instruments_.push_back(OISSwapNew(Today(), Spot(), maturity, 0.04, Fixed6M(), OvernightIndex(), Float3M()));
        }
        stageBuilder.knotDates_ = knotDates;

        multiSpec.stages_.push_back(stageBuilder.Build());
    }

    auto result = CalibrateMultiCurveBundle(multiSpec);

    ASSERT_EQ(result.discountCurves_.size(), static_cast<size_t>(1));
    ASSERT_EQ(result.diagnostics_.size(), static_cast<size_t>(1));

    // Check stage 1 diagnostics
    ASSERT_LT(result.diagnostics_[0].maxAbsResidual_, 1.0e-6);
    ASSERT_LT(result.diagnostics_[0].rmsResidual_, 1.0e-6);
}

// CurveSolverOptions_ centralizes the shared knob names and the single-curve defaults.

TEST(CurveSpecTest, TestCurveSolverOptionsDefaults) {
    CurveSolverOptions_ opts;
    ASSERT_NEAR(opts.smoothingWeight_, 1.0, 1e-15);
    ASSERT_NEAR(opts.tolerance_, 1.0e-8, 1e-15);
    ASSERT_NEAR(opts.fitTolerance_, 1.0e-6, 1e-15);
    ASSERT_NEAR(opts.initialGuess_, 0.05, 1e-15);
    ASSERT_EQ(opts.maxEvaluations_, 200);
    ASSERT_EQ(opts.maxRestarts_, 20);
    ASSERT_EQ(opts.solveMode_.Switch(), CurveSolveMode_::Value_::EXACT);
}

// Round-trip every builder field through Build() to guard against brace-init
// order/count drift between the builder and CurveCalibrationSpec_.

TEST(CurveSpecTest, TestBuildRoundTripsEveryField) {
    CurveCalibrationSpecBuilder_ b;
    b.today_ = Today();
    b.ccy_ = "USD";
    b.curveName_ = "roundtrip";
    b.calibrateDiscountCurve_ = true;
    b.targetCollateral_ = CollateralType_OIS();
    b.liborBasis_ = DayBasis_New("ACT_365F");

    // Distinct scalar sentinels catch any adjacent-field reorder in the brace-init.
    b.smoothingWeight_ = 1.5;
    b.tolerance_ = 3.0e-9;
    b.fitTolerance_ = 4.0e-7;
    b.initialGuess_ = 0.037;
    b.maxEvaluations_ = 177;
    b.maxRestarts_ = 13;
    b.solveMode_ = CurveSolveMode_::Value_::APPROXIMATE;
    b.parameterization_ = CurveParameterization_::Value_::LOG_DISCOUNT;
    b.knotPolicy_ = CurveKnotPolicy_::Value_::AUGMENTED;
    b.logDfScheme_ = LogDfScheme_::Value_::LOG_CUBIC_NATURAL;
    b.initialGuessPerNode_ = Vector_<>{0.04, 0.04, 0.04, 0.04};

    Vector_<Date_> knotDates;
    knotDates.push_back(b.today_); // LOG_DISCOUNT anchor knot
    for (int y : {2, 5, 10}) {
        Date_ maturity = Spot().AddDays(y * 365);
        knotDates.push_back(maturity);
        b.instruments_.push_back(OISSwapNew(Today(), Spot(), maturity, 0.04, Fixed6M(), OvernightIndex(), Float3M()));
    }
    b.knotDates_ = knotDates;

    auto spec = b.Build();

    ASSERT_EQ(spec.today_, b.today_);
    ASSERT_EQ(spec.ccy_, b.ccy_);
    ASSERT_EQ(spec.curveName_, String_("roundtrip"));
    ASSERT_EQ(spec.calibrateDiscountCurve_, true);
    ASSERT_EQ(spec.targetCollateral_.Switch(), b.targetCollateral_.Switch());
    ASSERT_EQ(spec.liborBasis_, b.liborBasis_);
    ASSERT_NEAR(spec.smoothingWeight_, 1.5, 1e-15);
    ASSERT_NEAR(spec.tolerance_, 3.0e-9, 1e-15);
    ASSERT_NEAR(spec.fitTolerance_, 4.0e-7, 1e-15);
    ASSERT_NEAR(spec.initialGuess_, 0.037, 1e-15);
    ASSERT_EQ(spec.maxEvaluations_, 177);
    ASSERT_EQ(spec.maxRestarts_, 13);
    ASSERT_EQ(spec.solveMode_.Switch(), CurveSolveMode_::Value_::APPROXIMATE);
    ASSERT_EQ(spec.parameterization_.Switch(), CurveParameterization_::Value_::LOG_DISCOUNT);
    ASSERT_EQ(spec.knotPolicy_.Switch(), CurveKnotPolicy_::Value_::AUGMENTED);
    ASSERT_EQ(spec.logDfScheme_.Switch(), LogDfScheme_::Value_::LOG_CUBIC_NATURAL);
    ASSERT_EQ(spec.instruments_.size(), static_cast<size_t>(3));
    ASSERT_EQ(spec.knotDates_.size(), static_cast<size_t>(4));
    ASSERT_EQ(spec.initialGuessPerNode_.size(), static_cast<size_t>(4));
}

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
using Dal::CurveParameterization_;
using Dal::CurveSolveMode_;
using Dal::Date_;
using Dal::DayBasis_New;
using Dal::DepositNew;
using Dal::DiscountPWLFNew;
using Dal::MultiCurveCalibrationSpec_;
using Dal::OISSwapNew;
using Dal::PeriodLength_New;
using Dal::RateIndexConvention_New;
using Dal::RateLegConvention_New;
using Dal::SwapNew;
using Dal::Vector_;
using Dal::RateLegConvention_;
using Dal::RateIndexConvention_;
using Dal::String_;

namespace {

Date_ Today() { return Date_(2025, 6, 20); }
Date_ Spot() { return Today().AddDays(2); }

Dal::RateLegConvention_ Fixed6M() {
    return RateLegConvention_New(PeriodLength_New("6M"), DayBasis_New("ACT_365F"));
}

Dal::RateLegConvention_ Float3M() {
    return RateLegConvention_New(PeriodLength_New("3M"), DayBasis_New("ACT_360"));
}

Dal::RateIndexConvention_ Libor3M() {
    return RateIndexConvention_New(PeriodLength_New("3M"), DayBasis_New("ACT_360"),
                                    CollateralType_OIS());
}

Dal::RateIndexConvention_ OvernightIndex() {
    return RateIndexConvention_New(PeriodLength_New("12M"), DayBasis_New("ACT_360"),
                                    CollateralType_OIS());
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
    ASSERT_EQ(builder.parameterization_.Switch(),
              CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD);
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
        builder.instruments_.push_back(
            OISSwapNew(Today(), Spot(), maturity, 0.04, Fixed6M(), OvernightIndex(), Float3M()));
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
        builder.instruments_.push_back(
            OISSwapNew(Today(), Spot(), maturity, 0.04, Fixed6M(), OvernightIndex(), Float3M()));
    }
    builder.knotDates_ = knotDates;

    auto spec = builder.Build();
    CalibrationResult_ result = CalibrateSingleCurve(
        spec, Dal::CurveJacobianMode_::Value_::BUMPED);

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
        builder.instruments_.push_back(
            OISSwapNew(Today(), Spot(), maturity, 0.04, Fixed6M(), OvernightIndex(), Float3M()));
    }
    builder.knotDates_ = knotDates;

    auto spec = builder.Build();
    CalibrationResult_ result = CalibrateSingleCurve(
        spec, Dal::CurveJacobianMode_::Value_::ANALYTIC);

    ASSERT_TRUE(result.curve_ != nullptr);
    ASSERT_LT(result.diagnostics_.maxAbsResidual_, 1.0e-6);
    // ANALYTIC Jacobian should produce a non-empty Jacobian
    ASSERT_GT(result.diagnostics_.jacobian_.Rows(), 0);
    ASSERT_GT(result.diagnostics_.jacobian_.Cols(), 0);
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
            stageBuilder.instruments_.push_back(
                OISSwapNew(Today(), Spot(), maturity, 0.04, Fixed6M(), OvernightIndex(), Float3M()));
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

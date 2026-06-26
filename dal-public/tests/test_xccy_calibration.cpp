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
using Dal::DayBasis_New;
using Dal::DiscountPWLFNew;
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

Dal::RateLegConvention_ Fixed6M365F() {
    return RateLegConvention_New(PeriodLength_New("6M"), DayBasis_New("ACT_365F"));
}

Dal::RateLegConvention_ Fixed6M360() {
    return RateLegConvention_New(PeriodLength_New("6M"), DayBasis_New("ACT_360"));
}

Dal::RateIndexConvention_ Libor3M() {
    return RateIndexConvention_New(PeriodLength_New("3M"), DayBasis_New("ACT_360"),
                                    CollateralType_OIS());
}

Dal::RateIndexConvention_ Euribor6M() {
    return RateIndexConvention_New(PeriodLength_New("6M"), DayBasis_New("ACT_360"),
                                    CollateralType_OIS());
}

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
            CrossCurrencySwapNew(Today(), Spot(), maturity, 0.01, currencies,
                                  100.0, 100.0 / 1.10,
                                  usdLeg, usdIndex, eurLeg, eurIndex));
    }
    builder.knotDates_ = knotDates;

    auto spec = builder.Build();
    auto result = CalibrateXccyMarket(spec);

    ASSERT_GT(result.diagnostics_.marketRates_.size(), static_cast<size_t>(0));
    ASSERT_EQ(result.diagnostics_.marketRates_.size(),
              result.diagnostics_.modelRates_.size());
    ASSERT_LT(result.diagnostics_.maxAbsResidual_, 1.0e-4);
    ASSERT_LT(result.diagnostics_.rmsResidual_, 1.0e-4);

    // Verify FX forward curve is populated
    ASSERT_GT(result.fxForwardCurve_.dates_.size(), static_cast<size_t>(0));
    ASSERT_EQ(result.fxForwardCurve_.dates_.size(),
              result.fxForwardCurve_.forwards_.size());
}

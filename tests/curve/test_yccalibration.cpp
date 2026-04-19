//
// Created by wegam on 2026/4/19.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/curve/yccalibration.hpp>
#include <dal/curve/piecewiselinear.hpp>
#include <dal/curve/ycimp.hpp>
#include <memory>

using namespace Dal;

TEST(YieldCurveCalibrationTest, TestFlatCurveCalibration) {
    const Date_ today(2024, 1, 15);
    const DayBasis_ basis("ACT_365F");
    const double flatRate = 0.05;

    Vector_<DepositInstrument_> deposits = {
        {Date::AddMonths(today, 3), flatRate},
        {Date::AddMonths(today, 6), flatRate},
    };

    Vector_<SwapInstrument_> swaps = {
        {Date::AddMonths(today, 12), flatRate, 6},
        {Date::AddMonths(today, 24), flatRate, 6},
    };

    Vector_<Date_> knotDates = {
        Date::AddMonths(today, 3),
        Date::AddMonths(today, 6),
        Date::AddMonths(today, 12),
        Date::AddMonths(today, 24),
    };

    std::unique_ptr<DiscountCurve_> dc(CalibrateYieldCurve(today, deposits, swaps, knotDates, basis));

    for (const auto& dep : deposits) {
        double modelRate = DepositRate(*dc, today, dep.maturity_, basis);
        ASSERT_NEAR(modelRate, dep.marketRate_, 1e-6);
    }

    for (const auto& swap : swaps) {
        double modelRate = SwapRate(*dc, today, swap.maturity_, swap.freqMonths_, basis);
        ASSERT_NEAR(modelRate, swap.marketRate_, 1e-6);
    }
}

TEST(YieldCurveCalibrationTest, TestUpwardSlopingCurve) {
    const Date_ today(2024, 1, 15);
    const DayBasis_ basis("ACT_365F");

    Vector_<DepositInstrument_> deposits = {
        {Date::AddMonths(today, 1), 0.0450},
        {Date::AddMonths(today, 3), 0.0460},
        {Date::AddMonths(today, 6), 0.0475},
    };

    Vector_<SwapInstrument_> swaps = {
        {Date::AddMonths(today, 12), 0.0490, 6},
        {Date::AddMonths(today, 24), 0.0500, 6},
        {Date::AddMonths(today, 36), 0.0505, 6},
        {Date::AddMonths(today, 60), 0.0510, 6},
    };

    Vector_<Date_> knotDates = {
        Date::AddMonths(today, 1),
        Date::AddMonths(today, 3),
        Date::AddMonths(today, 6),
        Date::AddMonths(today, 12),
        Date::AddMonths(today, 18),
        Date::AddMonths(today, 24),
        Date::AddMonths(today, 36),
        Date::AddMonths(today, 48),
        Date::AddMonths(today, 60),
    };

    std::unique_ptr<DiscountCurve_> dc(CalibrateYieldCurve(today, deposits, swaps, knotDates, basis));

    for (const auto& dep : deposits) {
        double modelRate = DepositRate(*dc, today, dep.maturity_, basis);
        ASSERT_NEAR(modelRate, dep.marketRate_, 1e-6);
    }

    for (const auto& swap : swaps) {
        double modelRate = SwapRate(*dc, today, swap.maturity_, swap.freqMonths_, basis);
        ASSERT_NEAR(modelRate, swap.marketRate_, 1e-6);
    }
}

TEST(YieldCurveCalibrationTest, TestRoundTrip) {
    const Date_ today(2024, 1, 15);
    const DayBasis_ basis("ACT_365F");

    Vector_<Date_> knotDates = {
        Date::AddMonths(today, 3),
        Date::AddMonths(today, 6),
        Date::AddMonths(today, 12),
        Date::AddMonths(today, 24),
    };

    Vector_<> origLeft = {0.045, 0.048, 0.050, 0.052};
    Vector_<> origRight = {0.046, 0.049, 0.051, 0.053};

    PiecewiseLinear_ origPwl(knotDates, origLeft, origRight);
    std::unique_ptr<DiscountCurve_> origDc(NewDiscountPWLF(String_("original"), origPwl));

    Vector_<DepositInstrument_> deposits = {
        {knotDates[0], DepositRate(*origDc, today, knotDates[0], basis)},
        {knotDates[1], DepositRate(*origDc, today, knotDates[1], basis)},
    };

    Vector_<SwapInstrument_> swaps = {
        {knotDates[2], SwapRate(*origDc, today, knotDates[2], 6, basis), 6},
        {knotDates[3], SwapRate(*origDc, today, knotDates[3], 6, basis), 6},
    };

    std::unique_ptr<DiscountCurve_> calibDc(CalibrateYieldCurve(today, deposits, swaps, knotDates, basis));

    for (const auto& dep : deposits) {
        double modelRate = DepositRate(*calibDc, today, dep.maturity_, basis);
        ASSERT_NEAR(modelRate, dep.marketRate_, 1e-6);
    }

    for (const auto& swap : swaps) {
        double modelRate = SwapRate(*calibDc, today, swap.maturity_, swap.freqMonths_, basis);
        ASSERT_NEAR(modelRate, swap.marketRate_, 1e-6);
    }
}

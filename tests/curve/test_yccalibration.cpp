//
// Created by wegam on 2026/4/19.
//

#include <gtest/gtest.h>
#include <memory>
#include <dal/platform/platform.hpp>
#include <dal/curve/yccalibration.hpp>
#include <dal/curve/piecewiselinear.hpp>
#include <dal/curve/ycimp.hpp>

using namespace Dal;

TEST(YieldCurveCalibrationTest, TestFlatCurveCalibration) {
    const Date_ today(2024, 1, 15);
    const DayBasis_ basis("ACT_365F");
    const double flatRate = 0.05;

    Vector_<Handle_<YCInstrument_>> instruments;
    instruments.push_back(Handle_<YCInstrument_>(new Deposit_(today, Date::AddMonths(today, 3), flatRate, basis)));
    instruments.push_back(Handle_<YCInstrument_>(new Deposit_(today, Date::AddMonths(today, 6), flatRate, basis)));
    instruments.push_back(Handle_<YCInstrument_>(new Swap_(today, Date::AddMonths(today, 12), flatRate, 6, basis)));
    instruments.push_back(Handle_<YCInstrument_>(new Swap_(today, Date::AddMonths(today, 24), flatRate, 6, basis)));

    Vector_<Date_> knotDates = {
        Date::AddMonths(today, 3),
        Date::AddMonths(today, 6),
        Date::AddMonths(today, 12),
        Date::AddMonths(today, 24),
    };

    std::unique_ptr<DiscountCurve_> dc(CalibrateYieldCurve(today, instruments, knotDates));
    CalibrationYieldCurve_ yc(*dc);

    for (const auto& inst : instruments) {
        Handle_<YCInstrument_::Rate_> rate = inst->Precompute(inst, Handle_<YieldCurve_>());
        double modelRate = (*rate)(yc);
        ASSERT_NEAR(modelRate, inst->MarketRate(), 1e-6);
    }
}

TEST(YieldCurveCalibrationTest, TestUpwardSlopingCurve) {
    const Date_ today(2024, 1, 15);
    const DayBasis_ basis("ACT_365F");

    Vector_<Handle_<YCInstrument_>> instruments;
    instruments.push_back(Handle_<YCInstrument_>(new Deposit_(today, Date::AddMonths(today, 1), 0.0450, basis)));
    instruments.push_back(Handle_<YCInstrument_>(new Deposit_(today, Date::AddMonths(today, 3), 0.0460, basis)));
    instruments.push_back(Handle_<YCInstrument_>(new Deposit_(today, Date::AddMonths(today, 6), 0.0475, basis)));
    instruments.push_back(Handle_<YCInstrument_>(new Swap_(today, Date::AddMonths(today, 12), 0.0490, 6, basis)));
    instruments.push_back(Handle_<YCInstrument_>(new Swap_(today, Date::AddMonths(today, 24), 0.0500, 6, basis)));
    instruments.push_back(Handle_<YCInstrument_>(new Swap_(today, Date::AddMonths(today, 36), 0.0505, 6, basis)));
    instruments.push_back(Handle_<YCInstrument_>(new Swap_(today, Date::AddMonths(today, 60), 0.0510, 6, basis)));

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

    std::unique_ptr<DiscountCurve_> dc(CalibrateYieldCurve(today, instruments, knotDates));
    CalibrationYieldCurve_ yc(*dc);

    for (const auto& inst : instruments) {
        Handle_<YCInstrument_::Rate_> rate = inst->Precompute(inst, Handle_<YieldCurve_>());
        double modelRate = (*rate)(yc);
        ASSERT_NEAR(modelRate, inst->MarketRate(), 1e-6);
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

    Vector_<Handle_<YCInstrument_>> instruments;
    instruments.push_back(Handle_<YCInstrument_>(
        new Deposit_(today, knotDates[0], DepositRate(*origDc, today, knotDates[0], basis), basis)));
    instruments.push_back(Handle_<YCInstrument_>(
        new Deposit_(today, knotDates[1], DepositRate(*origDc, today, knotDates[1], basis), basis)));
    instruments.push_back(Handle_<YCInstrument_>(
        new Swap_(today, knotDates[2], SwapRate(*origDc, today, knotDates[2], 6, basis), 6, basis)));
    instruments.push_back(Handle_<YCInstrument_>(
        new Swap_(today, knotDates[3], SwapRate(*origDc, today, knotDates[3], 6, basis), 6, basis)));

    std::unique_ptr<DiscountCurve_> calibDc(CalibrateYieldCurve(today, instruments, knotDates));
    CalibrationYieldCurve_ yc(*calibDc);

    for (const auto& inst : instruments) {
        Handle_<YCInstrument_::Rate_> rate = inst->Precompute(inst, Handle_<YieldCurve_>());
        double modelRate = (*rate)(yc);
        ASSERT_NEAR(modelRate, inst->MarketRate(), 1e-6);
    }
}

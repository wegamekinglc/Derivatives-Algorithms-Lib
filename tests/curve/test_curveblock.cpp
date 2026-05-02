//
// Created by wegam on 2026/4/19.
//

#include <gtest/gtest.h>
#include <cmath>
#include <memory>
#include <dal/platform/platform.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/piecewiselinear.hpp>
#include <dal/curve/ycimp.hpp>

using namespace Dal;

namespace {
    constexpr double SOLVER_INITIAL_GUESS = 0.05;
    constexpr double MIN_DISTANCE_FROM_GUESS = 0.02;

    void AssertQuotesFarFromInitialGuess(const Vector_<Handle_<YCInstrument_>>& instruments) {
        for (const auto& inst : instruments)
            ASSERT_GT(std::fabs(inst->MarketRate() - SOLVER_INITIAL_GUESS), MIN_DISTANCE_FROM_GUESS);
    }
} // namespace

TEST(CurveBlockTest, TestFlatCurveCalibration) {
    const Date_ today(2024, 1, 15);
    const DayBasis_ basis("ACT_365F");
    const String_ ccy = "USD";
    const double flatRate = 0.015;

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

    AssertQuotesFarFromInitialGuess(instruments);

    std::unique_ptr<DiscountCurve_> dc(CalibrateYieldCurve(today, ccy, instruments, knotDates));
    CurveBlock_ yc(*dc);

    for (const auto& inst : instruments) {
        Handle_<YCInstrument_::Rate_> rate = inst->Precompute(inst, Handle_<YieldCurve_>());
        double modelRate = (*rate)(yc);
        ASSERT_NEAR(modelRate, inst->MarketRate(), 1e-8);
    }
}

TEST(CurveBlockTest, TestUpwardSlopingCurve) {
    const Date_ today(2024, 1, 15);
    const String_ ccy = "USD";
    const DayBasis_ basis("ACT_365F");

    Vector_<Handle_<YCInstrument_>> instruments;
    instruments.push_back(Handle_<YCInstrument_>(new Deposit_(today, Date::AddMonths(today, 1), 0.0110, basis)));
    instruments.push_back(Handle_<YCInstrument_>(new Deposit_(today, Date::AddMonths(today, 3), 0.0125, basis)));
    instruments.push_back(Handle_<YCInstrument_>(new Deposit_(today, Date::AddMonths(today, 6), 0.0140, basis)));
    instruments.push_back(Handle_<YCInstrument_>(new Swap_(today, Date::AddMonths(today, 12), 0.0160, 6, basis)));
    instruments.push_back(Handle_<YCInstrument_>(new Swap_(today, Date::AddMonths(today, 24), 0.0175, 6, basis)));
    instruments.push_back(Handle_<YCInstrument_>(new Swap_(today, Date::AddMonths(today, 36), 0.0190, 6, basis)));
    instruments.push_back(Handle_<YCInstrument_>(new Swap_(today, Date::AddMonths(today, 60), 0.0210, 6, basis)));

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

    AssertQuotesFarFromInitialGuess(instruments);

    std::unique_ptr<DiscountCurve_> dc(CalibrateYieldCurve(today, ccy, instruments, knotDates));
    CurveBlock_ yc(*dc);

    for (const auto& inst : instruments) {
        Handle_<YCInstrument_::Rate_> rate = inst->Precompute(inst, Handle_<YieldCurve_>());
        double modelRate = (*rate)(yc);
        ASSERT_NEAR(modelRate, inst->MarketRate(), 1e-8);
    }
}

TEST(CurveBlockTest, TestRoundTrip) {
    const Date_ today(2024, 1, 15);
    const String_ ccy = "USD";
    const DayBasis_ basis("ACT_365F");

    Vector_<Date_> knotDates = {
        Date::AddMonths(today, 3),
        Date::AddMonths(today, 6),
        Date::AddMonths(today, 12),
        Date::AddMonths(today, 24),
    };

    Vector_<> origLeft = {0.012, 0.014, 0.017, 0.020};
    Vector_<> origRight = {0.013, 0.015, 0.018, 0.021};

    PiecewiseLinear_ origPwl(knotDates, origLeft, origRight);
    std::unique_ptr<DiscountCurve_> origDc(NewDiscountPWLF(String_("original"), ccy, origPwl));

    Vector_<Handle_<YCInstrument_>> instruments;
    instruments.push_back(Handle_<YCInstrument_>(
        new Deposit_(today, knotDates[0], 0.012, basis)));
    instruments.push_back(Handle_<YCInstrument_>(
        new Deposit_(today, knotDates[1], 0.014, basis)));
    instruments.push_back(Handle_<YCInstrument_>(
        new Swap_(today, knotDates[2], 0.017, 6, basis)));
    instruments.push_back(Handle_<YCInstrument_>(
        new Swap_(today, knotDates[3], 0.020, 6, basis)));

    AssertQuotesFarFromInitialGuess(instruments);

    std::unique_ptr<DiscountCurve_> calibDc(CalibrateYieldCurve(today, ccy, instruments, knotDates));
    CurveBlock_ yc(*calibDc);

    for (const auto& inst : instruments) {
        Handle_<YCInstrument_::Rate_> rate = inst->Precompute(inst, Handle_<YieldCurve_>());
        double modelRate = (*rate)(yc);
        ASSERT_NEAR(modelRate, inst->MarketRate(), 1e-8);
    }
}

TEST(CurveBlockTest, TestCalibrationWithSTIR) {
    const Date_ today(2024, 1, 15);
    const String_ ccy = "USD";
    const DayBasis_ basis("ACT_365F");

    Vector_<Handle_<YCInstrument_>> instruments;
    instruments.push_back(Handle_<YCInstrument_>(new Deposit_(today, Date::AddMonths(today, 1), 0.0110, basis)));
    instruments.push_back(Handle_<YCInstrument_>(new STIR_(today, Date::AddMonths(today, 3), Date::AddMonths(today, 6), 0.0130, basis)));
    instruments.push_back(Handle_<YCInstrument_>(new STIR_(today, Date::AddMonths(today, 6), Date::AddMonths(today, 9), 0.0140, basis)));
    instruments.push_back(Handle_<YCInstrument_>(new Swap_(today, Date::AddMonths(today, 12), 0.0160, 3, basis)));
    instruments.push_back(Handle_<YCInstrument_>(new Swap_(today, Date::AddMonths(today, 24), 0.0180, 6, basis)));

    Vector_<Date_> knotDates = {
        Date::AddMonths(today, 1),
        Date::AddMonths(today, 3),
        Date::AddMonths(today, 6),
        Date::AddMonths(today, 9),
        Date::AddMonths(today, 12),
        Date::AddMonths(today, 24),
    };

    AssertQuotesFarFromInitialGuess(instruments);

    std::unique_ptr<DiscountCurve_> dc(CalibrateYieldCurve(today, ccy, instruments, knotDates));
    CurveBlock_ yc(*dc);

    for (const auto& inst : instruments) {
        Handle_<YCInstrument_::Rate_> rate = inst->Precompute(inst, Handle_<YieldCurve_>());
        double modelRate = (*rate)(yc);
        ASSERT_NEAR(modelRate, inst->MarketRate(), 1e-8);
    }
}


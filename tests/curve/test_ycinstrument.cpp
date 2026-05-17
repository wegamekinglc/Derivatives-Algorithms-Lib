//
// Created by GitHub Copilot on 2026/5/17.
//

#include <gtest/gtest.h>
#include <cmath>
#include <memory>
#include <dal/platform/platform.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/ycimp.hpp>
#include <dal/curve/ycinstrument.hpp>
#include <dal/curve/piecewiselinear.hpp>

using namespace Dal;

namespace {
    Handle_<DiscountCurve_> MakeFlatDiscountCurve(const String_& name,
                                                  const String_& ccy,
                                                  const Date_& today,
                                                  double rate) {
        const Vector_<Date_> knotDates = {
            Date::AddMonths(today, 3),
            Date::AddMonths(today, 6),
            Date::AddMonths(today, 12),
            Date::AddMonths(today, 24),
        };
        const Vector_<> values(knotDates.size(), rate);
        return Handle_<DiscountCurve_>(NewDiscountPWLF(name, ccy, PiecewiseLinear_(knotDates, values, values)));
    }

    double ExpectedSimpleRate(const DiscountCurve_& dc,
                              const Date_& from,
                              const Date_& to,
                              const DayBasis_& basis) {
        const double df = dc(from, to);
        return (1.0 / df - 1.0) / basis(from, to, nullptr);
    }

    double ExpectedSwapRate(const DiscountCurve_& dc,
                            const Date_& today,
                            const Date_& maturity,
                            int freqMonths,
                            const DayBasis_& basis) {
        double annuity = 0.0;
        Date_ start = today;
        while (start < maturity) {
            Date_ end = Date::AddMonths(start, freqMonths);
            if (end > maturity)
                end = maturity;
            annuity += basis(start, end, nullptr) * dc(today, end);
            start = end;
        }
        return (1.0 - dc(today, maturity)) / annuity;
    }
} // namespace

TEST(YCInstrumentTest, TestDepositPrecomputeMatchesDiscountCurve) {
    const Date_ today(2024, 1, 15);
    const Date_ maturity = Date::AddMonths(today, 6);
    const DayBasis_ basis("ACT_365F");
    const Handle_<DiscountCurve_> dc = MakeFlatDiscountCurve("ois", "USD", today, 0.02);
    const CurveBlock_ curve(dc, basis);
    const Handle_<YCInstrument_> deposit(new Deposit_(today, maturity, 0.021, basis));

    ASSERT_EQ(deposit->Name(), "Deposit");
    ASSERT_EQ(deposit->TimeSpan().first, today);
    ASSERT_EQ(deposit->TimeSpan().second, maturity);

    const Handle_<YCInstrument_::Rate_> rate = deposit->Precompute(deposit, Handle_<YieldCurve_>());
    const double expected = ExpectedSimpleRate(*dc, today, maturity, basis);
    ASSERT_NEAR((*rate)(curve), expected, 1e-12);
}

TEST(YCInstrumentTest, TestSwapPrecomputeMatchesParRate) {
    const Date_ today(2024, 1, 15);
    const Date_ maturity = Date::AddMonths(today, 24);
    const DayBasis_ basis("ACT_365F");
    const Handle_<DiscountCurve_> dc = MakeFlatDiscountCurve("ois", "USD", today, 0.025);
    const CurveBlock_ curve(dc, basis);
    const Handle_<YCInstrument_> swap(new Swap_(today, maturity, 0.026, 6, basis));

    ASSERT_EQ(swap->Name(), "Swap");
    ASSERT_EQ(swap->TimeSpan().first, today);
    ASSERT_EQ(swap->TimeSpan().second, maturity);

    const Handle_<YCInstrument_::Rate_> rate = swap->Precompute(swap, Handle_<YieldCurve_>());
    const double expected = ExpectedSwapRate(*dc, today, maturity, 6, basis);
    ASSERT_NEAR((*rate)(curve), expected, 1e-12);
}

TEST(YCInstrumentTest, TestSTIRPrecomputeMatchesForwardRate) {
    const Date_ today(2024, 1, 15);
    const Date_ start = Date::AddMonths(today, 3);
    const Date_ maturity = Date::AddMonths(today, 6);
    const DayBasis_ basis("ACT_365F");
    const Handle_<DiscountCurve_> dc = MakeFlatDiscountCurve("ois", "USD", today, 0.03);
    const CurveBlock_ curve(dc, basis);
    const Handle_<YCInstrument_> stir(new STIR_(today, start, maturity, 0.031, basis));

    ASSERT_EQ(stir->Name(), "STIR");
    ASSERT_EQ(stir->TimeSpan().first, start);
    ASSERT_EQ(stir->TimeSpan().second, maturity);

    const Handle_<YCInstrument_::Rate_> rate = stir->Precompute(stir, Handle_<YieldCurve_>());
    const double expected = ExpectedSimpleRate(*dc, start, maturity, basis);
    ASSERT_NEAR((*rate)(curve), expected, 1e-12);
}

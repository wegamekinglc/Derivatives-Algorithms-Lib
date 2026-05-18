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

    double ExpectedBasisSpread(const DiscountCurve_& discount,
                               const DiscountCurve_& spreadForecast,
                               const DiscountCurve_& referenceForecast,
                               const Date_& today,
                               const Date_& maturity,
                               int spreadFreqMonths,
                               int referenceFreqMonths,
                               const DayBasis_& spreadBasis,
                               const DayBasis_& referenceBasis) {
        double spreadLegPv = 0.0;
        double spreadAnnuity = 0.0;
        Date_ start = today;
        while (start < maturity) {
            Date_ end = Date::AddMonths(start, spreadFreqMonths);
            if (end > maturity)
                end = maturity;
            const double tau = spreadBasis(start, end, nullptr);
            const double df = discount(today, end);
            spreadLegPv += ExpectedSimpleRate(spreadForecast, start, end, spreadBasis) * tau * df;
            spreadAnnuity += tau * df;
            start = end;
        }

        double referenceLegPv = 0.0;
        start = today;
        while (start < maturity) {
            Date_ end = Date::AddMonths(start, referenceFreqMonths);
            if (end > maturity)
                end = maturity;
            referenceLegPv += ExpectedSimpleRate(referenceForecast, start, end, referenceBasis)
                              * referenceBasis(start, end, nullptr) * discount(today, end);
            start = end;
        }
        return (referenceLegPv - spreadLegPv) / spreadAnnuity;
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

TEST(YCInstrumentTest, TestFraUsesTenorSpecificForwardCurve) {
    const Date_ today(2024, 1, 15);
    const Date_ start = Date::AddMonths(today, 3);
    const Date_ maturity = Date::AddMonths(today, 6);
    const DayBasis_ basis("ACT_360");
    const Handle_<DiscountCurve_> ois = MakeFlatDiscountCurve("ois", "USD", today, 0.01);
    const Handle_<DiscountCurve_> libor3m = MakeFlatDiscountCurve("libor3m", "USD", today, 0.03);
    const CurveBlock_ curve("bundle",
                            "USD",
                            {{CollateralType_(CollateralType_::Value_::OIS), ois}},
                            {{PeriodLength_("3M"), libor3m}},
                            basis);

    RateIndexConvention_ convention;
    convention.useProjectionCurve_ = true;
    convention.forecastTenor_ = PeriodLength_("3M");
    convention.dayBasis_ = basis;
    convention.collateral_ = CollateralType_(CollateralType_::Value_::OIS);
    const Handle_<YCInstrument_> fra(new FRA_(today, start, maturity, 0.0, convention));

    const Handle_<YCInstrument_::Rate_> rate = fra->Precompute(fra, Handle_<YieldCurve_>());
    const double expected = ExpectedSimpleRate(*libor3m, start, maturity, basis);
    ASSERT_NEAR((*rate)(curve), expected, 1e-12);
}

TEST(YCInstrumentTest, TestFutureAppliesConvexityAdjustment) {
    const Date_ today(2024, 1, 15);
    const Date_ start = Date::AddMonths(today, 3);
    const Date_ maturity = Date::AddMonths(today, 6);
    const DayBasis_ basis("ACT_360");
    const Handle_<DiscountCurve_> dc = MakeFlatDiscountCurve("ois", "USD", today, 0.03);
    const CurveBlock_ curve(dc, basis);

    RateIndexConvention_ convention;
    convention.dayBasis_ = basis;
    const Handle_<YCInstrument_> future(new Future_(today, start, maturity, 0.0, convention, 0.0015));

    const Handle_<YCInstrument_::Rate_> rate = future->Precompute(future, Handle_<YieldCurve_>());
    const double expected = ExpectedSimpleRate(*dc, start, maturity, basis) - 0.0015;
    ASSERT_NEAR((*rate)(curve), expected, 1e-12);
}

TEST(YCInstrumentTest, TestBasisSwapUsesSeparateForwardCurves) {
    const Date_ today(2024, 1, 15);
    const Date_ maturity = Date::AddMonths(today, 24);
    const DayBasis_ basis("ACT_360");
    const Handle_<DiscountCurve_> ois = MakeFlatDiscountCurve("ois", "USD", today, 0.01);
    const Handle_<DiscountCurve_> libor3m = MakeFlatDiscountCurve("libor3m", "USD", today, 0.03);
    const Handle_<DiscountCurve_> libor6m = MakeFlatDiscountCurve("libor6m", "USD", today, 0.035);
    const CurveBlock_ curve("bundle",
                            "USD",
                            {{CollateralType_(CollateralType_::Value_::OIS), ois}},
                            {{PeriodLength_("3M"), libor3m}, {PeriodLength_("6M"), libor6m}},
                            basis);

    RateIndexConvention_ spreadConvention;
    spreadConvention.useProjectionCurve_ = true;
    spreadConvention.forecastTenor_ = PeriodLength_("3M");
    spreadConvention.dayBasis_ = basis;
    spreadConvention.collateral_ = CollateralType_(CollateralType_::Value_::OIS);

    RateIndexConvention_ referenceConvention(spreadConvention);
    referenceConvention.forecastTenor_ = PeriodLength_("6M");

    RateLegConvention_ spreadLeg;
    spreadLeg.paymentFrequency_ = PeriodLength_("3M");
    spreadLeg.dayBasis_ = basis;
    RateLegConvention_ referenceLeg;
    referenceLeg.paymentFrequency_ = PeriodLength_("6M");
    referenceLeg.dayBasis_ = basis;

    const Handle_<YCInstrument_> basisSwap(
        new BasisSwap_(today, today, maturity, 0.0, spreadConvention, spreadLeg, referenceConvention, referenceLeg));

    const Handle_<YCInstrument_::Rate_> rate = basisSwap->Precompute(basisSwap, Handle_<YieldCurve_>());
    const double expected = ExpectedBasisSpread(*ois, *libor3m, *libor6m, today, maturity, 3, 6, basis, basis);
    ASSERT_NEAR((*rate)(curve), expected, 1e-12);
}

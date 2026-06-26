//
// Created by wegamekinglc on 2026/6/20.
//

#include <gtest/gtest.h>

#include <dal-public/src/curveinstrument.hpp>
#include <dal-public/src/curveprotocol.hpp>

using Dal::BasisSwapNew;
using Dal::CrossCurrencySwapNew;
using Dal::CurrencyPair_New;
using Dal::Date_;
using Dal::DayBasis_New;
using Dal::DepositNew;
using Dal::FRANew;
using Dal::FutureNew;
using Dal::Handle_;
using Dal::OISSwapNew;
using Dal::PeriodLength_New;
using Dal::RateIndexConvention_New;
using Dal::RateLegConvention_New;
using Dal::SwapNew;
using Dal::CollateralType_OIS;
using Dal::YCInstrument_;
using Dal::RateLegConvention_;
using Dal::RateIndexConvention_;

namespace {
Date_ Today() { return Date_(2025, 6, 20); }
Date_ Spot() { return Today().AddDays(2); }

Dal::RateLegConvention_ Fixed6M() {
    return RateLegConvention_New(PeriodLength_New("6M"), DayBasis_New("ACT_365F"));
}

Dal::RateIndexConvention_ Libor3M() {
    return RateIndexConvention_New(PeriodLength_New("3M"), DayBasis_New("ACT_360"),
                                    CollateralType_OIS());
}

Dal::RateIndexConvention_ OvernightIndex() {
    return RateIndexConvention_New(PeriodLength_New("12M"), DayBasis_New("ACT_360"),
                                    CollateralType_OIS());
}

Dal::RateLegConvention_ Float3M() {
    return RateLegConvention_New(PeriodLength_New("3M"), DayBasis_New("ACT_360"));
}
} // namespace

// Deposit

TEST(CurveInstrumentTest, TestDepositNew) {
    Date_ start = Spot();
    Date_ maturity = start.AddDays(90);
    auto inst = DepositNew(Today(), start, maturity, 0.05, Libor3M());
    ASSERT_TRUE(inst != nullptr);
}

TEST(CurveInstrumentTest, TestDepositNewVariousMaturities) {
    for (int days : {30, 90, 180, 365}) {
        Date_ start = Spot();
        Date_ maturity = start.AddDays(days);
        auto inst = DepositNew(Today(), start, maturity, 0.04, Libor3M());
        ASSERT_TRUE(inst != nullptr);
    }
}

// FRA

TEST(CurveInstrumentTest, TestFRANew) {
    Date_ start = Spot().AddDays(180);
    Date_ maturity = start.AddDays(90);
    auto inst = FRANew(Today(), start, maturity, 0.045, Libor3M());
    ASSERT_TRUE(inst != nullptr);
}

// Future

TEST(CurveInstrumentTest, TestFutureNew) {
    Date_ start = Spot().AddDays(90);
    Date_ maturity = start.AddDays(90);
    auto inst = FutureNew(Today(), start, maturity, 0.045, Libor3M());
    ASSERT_TRUE(inst != nullptr);
}

TEST(CurveInstrumentTest, TestFutureNewWithConvexity) {
    Date_ start = Spot().AddDays(90);
    Date_ maturity = start.AddDays(90);
    auto inst = FutureNew(Today(), start, maturity, 0.045, Libor3M(), 0.0005);
    ASSERT_TRUE(inst != nullptr);
}

// Vanilla Swap

TEST(CurveInstrumentTest, TestSwapNew) {
    Date_ start = Spot();
    Date_ maturity = start.AddDays(1825); // 5 years
    auto inst = SwapNew(Today(), start, maturity, 0.04, Fixed6M(), Libor3M(), Float3M());
    ASSERT_TRUE(inst != nullptr);
}

TEST(CurveInstrumentTest, TestSwapNewVariousMaturities) {
    for (int years : {1, 2, 5, 10, 30}) {
        Date_ start = Spot();
        Date_ maturity = start.AddDays(years * 365);
        auto inst = SwapNew(Today(), start, maturity, 0.04, Fixed6M(), Libor3M(), Float3M());
        ASSERT_TRUE(inst != nullptr);
    }
}

// OIS Swap

TEST(CurveInstrumentTest, TestOISSwapNew) {
    Date_ start = Spot();
    Date_ maturity = start.AddDays(1825);
    auto inst = OISSwapNew(Today(), start, maturity, 0.035, Fixed6M(), OvernightIndex(), Float3M());
    ASSERT_TRUE(inst != nullptr);
}

// Basis Swap

TEST(CurveInstrumentTest, TestBasisSwapNew) {
    Date_ start = Spot();
    Date_ maturity = start.AddDays(3650); // 10 years
    auto inst = BasisSwapNew(Today(), start, maturity, 0.0025,
                              Libor3M(), Float3M(),  // spread leg: 3M Libor
                              OvernightIndex(), Float3M());  // ref leg: OIS
    ASSERT_TRUE(inst != nullptr);
}

// Cross-Currency Swap

TEST(CurveInstrumentTest, TestCrossCurrencySwapNew) {
    Date_ start = Spot();
    Date_ maturity = start.AddDays(3650);
    auto currencies = CurrencyPair_New("USD", "EUR");
    auto domesticLeg = RateLegConvention_New(PeriodLength_New("6M"), DayBasis_New("ACT_365F"));
    auto domesticIndex = RateIndexConvention_New(PeriodLength_New("3M"), DayBasis_New("ACT_360"),
                                                   CollateralType_OIS());
    auto foreignLeg = RateLegConvention_New(PeriodLength_New("6M"), DayBasis_New("ACT_360"));
    auto foreignIndex = RateIndexConvention_New(PeriodLength_New("6M"), DayBasis_New("ACT_360"),
                                                  CollateralType_OIS());
    auto inst = CrossCurrencySwapNew(Today(), start, maturity, 0.001,
                                      currencies, 100.0, 90.0,
                                      domesticLeg, domesticIndex,
                                      foreignLeg, foreignIndex);
    ASSERT_TRUE(inst != nullptr);
}

TEST(CurveInstrumentTest, TestCrossCurrencySwapNewDefaults) {
    Date_ start = Spot();
    Date_ maturity = start.AddDays(3650);
    auto currencies = CurrencyPair_New("USD", "EUR");
    auto inst = CrossCurrencySwapNew(Today(), start, maturity, 0.001, currencies);
    ASSERT_TRUE(inst != nullptr);
}

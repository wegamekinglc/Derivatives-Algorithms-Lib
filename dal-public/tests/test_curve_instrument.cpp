//
// Created by wegamekinglc on 2026/6/20.
//

#include <gtest/gtest.h>

#include <dal-public/src/curveinstrument.hpp>
#include <dal-public/src/curveprotocol.hpp>

using Dal::BasisSwapNew;
using Dal::CollateralType_OIS;
using Dal::CrossCurrencySwapConfigBuilder_;
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
using Dal::RateIndexConvention_;
using Dal::RateIndexConvention_New;
using Dal::RateLegConvention_;
using Dal::RateLegConvention_New;
using Dal::SwapNew;
using Dal::XccyNotionalMode_;
using Dal::YCInstrument_;

namespace {
    Date_ Today() { return Date_(2025, 6, 20); }
    Date_ Spot() { return Today().AddDays(2); }

    Dal::RateLegConvention_ Fixed6M() { return RateLegConvention_New(PeriodLength_New("6M"), DayBasis_New("ACT_365F")); }

    Dal::RateIndexConvention_ Libor3M() { return RateIndexConvention_New(PeriodLength_New("3M"), DayBasis_New("ACT_360"), CollateralType_OIS()); }

    Dal::RateIndexConvention_ OvernightIndex() {
        return RateIndexConvention_New(PeriodLength_New("12M"), DayBasis_New("ACT_360"), CollateralType_OIS());
    }

    Dal::RateLegConvention_ Float3M() { return RateLegConvention_New(PeriodLength_New("3M"), DayBasis_New("ACT_360")); }
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
    Date_ maturity = start.AddDays(3650);                                            // 10 years
    auto inst = BasisSwapNew(Today(), start, maturity, 0.0025, Libor3M(), Float3M(), // spread leg: 3M Libor
                             OvernightIndex(), Float3M());                           // ref leg: OIS
    ASSERT_TRUE(inst != nullptr);
}

// Cross-Currency Swap

TEST(CurveInstrumentTest, TestCrossCurrencySwapNew) {
    Date_ start = Spot();
    Date_ maturity = start.AddDays(3650);
    auto currencies = CurrencyPair_New("USD", "EUR");
    auto domesticLeg = RateLegConvention_New(PeriodLength_New("6M"), DayBasis_New("ACT_365F"));
    auto domesticIndex = RateIndexConvention_New(PeriodLength_New("3M"), DayBasis_New("ACT_360"), CollateralType_OIS());
    auto foreignLeg = RateLegConvention_New(PeriodLength_New("6M"), DayBasis_New("ACT_360"));
    auto foreignIndex = RateIndexConvention_New(PeriodLength_New("6M"), DayBasis_New("ACT_360"), CollateralType_OIS());
    auto inst = CrossCurrencySwapNew(Today(), start, maturity, 0.001, currencies, 100.0, 90.0, domesticLeg, domesticIndex, foreignLeg, foreignIndex);
    ASSERT_TRUE(inst != nullptr);
}

TEST(CurveInstrumentTest, TestCrossCurrencySwapNewDefaults) {
    Date_ start = Spot();
    Date_ maturity = start.AddDays(3650);
    auto currencies = CurrencyPair_New("USD", "EUR");
    auto inst = CrossCurrencySwapNew(Today(), start, maturity, 0.001, currencies);
    ASSERT_TRUE(inst != nullptr);
}

TEST(CurveInstrumentTest, TestCrossCurrencySwapConfigBuilderAndOverloadRoundTripEveryField) {
    CrossCurrencySwapConfigBuilder_ builder;
    builder.pair_ = CurrencyPair_New("USD", "EUR");
    builder.domesticNotional_ = 125.0;
    builder.foreignNotional_ = 113.5;
    builder.convention_.initialNotionalExchange_ = false;
    builder.convention_.finalNotionalExchange_ = true;
    builder.convention_.spreadOnForeignLeg_ = false;
    builder.convention_.domesticLeg_ = Fixed6M();
    builder.convention_.domesticIndex_ = Libor3M();
    builder.convention_.foreignLeg_ = Float3M();
    builder.convention_.foreignIndex_ = OvernightIndex();
    builder.notionalMode_ = XccyNotionalMode_::Value_::RESETTABLE;
    builder.fxReset_ = Dal::FxResetConventionNew(2, Dal::Holidays_(""), Dal::BizDayConvention_("Following"), 10, 30);
    builder.domesticRateFixing_.indexName_ = "USD-SOFR-3M";
    builder.domesticRateFixing_.fixingHour_ = 11;
    builder.domesticRateFixing_.fixingMinute_ = 5;
    builder.foreignRateFixing_.indexName_ = "EUR-ESTR-3M";
    builder.foreignRateFixing_.fixingHour_ = 12;
    builder.foreignRateFixing_.fixingMinute_ = 15;

    const auto config = builder.Build();
    const auto instrument = CrossCurrencySwapNew(Today(), Spot(), Spot().AddDays(3650), 0.0015, config);
    const auto& actual = instrument->Config();

    ASSERT_TRUE(actual.pair_ == builder.pair_);
    ASSERT_NEAR(actual.domesticNotional_, 125.0, 1.0e-15);
    ASSERT_NEAR(actual.foreignNotional_, 113.5, 1.0e-15);
    ASSERT_FALSE(actual.convention_.initialNotionalExchange_);
    ASSERT_TRUE(actual.convention_.finalNotionalExchange_);
    ASSERT_FALSE(actual.convention_.spreadOnForeignLeg_);
    ASSERT_EQ(actual.convention_.domesticLeg_.paymentFrequency_.String(), builder.convention_.domesticLeg_.paymentFrequency_.String());
    ASSERT_EQ(actual.convention_.domesticIndex_.forecastTenor_.String(), builder.convention_.domesticIndex_.forecastTenor_.String());
    ASSERT_EQ(actual.convention_.foreignLeg_.paymentFrequency_.String(), builder.convention_.foreignLeg_.paymentFrequency_.String());
    ASSERT_EQ(actual.convention_.foreignIndex_.forecastTenor_.String(), builder.convention_.foreignIndex_.forecastTenor_.String());
    ASSERT_EQ(actual.notionalMode_.Switch(), XccyNotionalMode_::Value_::RESETTABLE);
    ASSERT_EQ(actual.fxReset_.fixingLag_, 2);
    ASSERT_EQ(actual.fxReset_.fixingHolidays_, Dal::Holidays_(""));
    ASSERT_EQ(actual.fxReset_.fixingConvention_, Dal::BizDayConvention_("Following"));
    ASSERT_EQ(actual.fxReset_.fixingHour_, 10);
    ASSERT_EQ(actual.fxReset_.fixingMinute_, 30);
    ASSERT_EQ(actual.domesticRateFixing_.indexName_, Dal::String_("USD-SOFR-3M"));
    ASSERT_EQ(actual.domesticRateFixing_.fixingHour_, 11);
    ASSERT_EQ(actual.domesticRateFixing_.fixingMinute_, 5);
    ASSERT_EQ(actual.foreignRateFixing_.indexName_, Dal::String_("EUR-ESTR-3M"));
    ASSERT_EQ(actual.foreignRateFixing_.fixingHour_, 12);
    ASSERT_EQ(actual.foreignRateFixing_.fixingMinute_, 15);
}

TEST(CurveInstrumentTest, TestCrossCurrencySwapNewLegacyOverloadRemainsFixed) {
    const auto pair = CurrencyPair_New("USD", "EUR");
    const auto domesticLeg = Fixed6M();
    const auto domesticIndex = Libor3M();
    const auto foreignLeg = Float3M();
    const auto foreignIndex = OvernightIndex();

    const auto instrument =
        CrossCurrencySwapNew(Today(), Spot(), Spot().AddDays(3650), 0.002, pair, 140.0, 127.0, domesticLeg, domesticIndex, foreignLeg, foreignIndex);
    const auto& config = instrument->Config();

    ASSERT_EQ(config.notionalMode_.Switch(), XccyNotionalMode_::Value_::FIXED);
    ASSERT_TRUE(config.pair_ == pair);
    ASSERT_NEAR(config.domesticNotional_, 140.0, 1.0e-15);
    ASSERT_NEAR(config.foreignNotional_, 127.0, 1.0e-15);
    ASSERT_EQ(config.convention_.domesticLeg_.paymentFrequency_.String(), domesticLeg.paymentFrequency_.String());
    ASSERT_EQ(config.convention_.domesticIndex_.forecastTenor_.String(), domesticIndex.forecastTenor_.String());
    ASSERT_EQ(config.convention_.foreignLeg_.paymentFrequency_.String(), foreignLeg.paymentFrequency_.String());
    ASSERT_EQ(config.convention_.foreignIndex_.forecastTenor_.String(), foreignIndex.forecastTenor_.String());
}

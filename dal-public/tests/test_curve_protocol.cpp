//
// Created by wegamekinglc on 2026/6/20.
//

#include <gtest/gtest.h>

#include <dal-public/src/curveprotocol.hpp>

using Dal::CollateralType_;
using Dal::CollateralType_Libor;
using Dal::CollateralType_OIS;
using Dal::CurrencyPair_;
using Dal::CurrencyPair_New;
using Dal::DayBasis_;
using Dal::DayBasis_New;
using Dal::PeriodLength_;
using Dal::PeriodLength_New;
using Dal::RateIndexConvention_;
using Dal::RateIndexConvention_New;
using Dal::RateLegConvention_;
using Dal::RateLegConvention_New;

TEST(CurveProtocolTest, TestCollateralTypeOIS) {
    CollateralType_ ct = CollateralType_OIS();
    ASSERT_EQ(ct.Switch(), CollateralType_::Value_::OIS);
}

TEST(CurveProtocolTest, TestCollateralTypeLibor) {
    PeriodLength_ tenor = PeriodLength_New("3M");
    CollateralType_ ct = CollateralType_Libor(tenor);
    ASSERT_EQ(ct.Switch(), CollateralType_::Value_::GC);
}

TEST(CurveProtocolTest, TestPeriodLengthNew) {
    PeriodLength_ pl = PeriodLength_New("6M");
    ASSERT_TRUE(pl.String() != nullptr);
}

TEST(CurveProtocolTest, TestPeriodLengthNewVariousTenors) {
    for (const char* iso : {"1M", "3M", "6M", "12M"}) {
        PeriodLength_ pl = PeriodLength_New(iso);
        ASSERT_TRUE(pl.String() != nullptr);
    }
}

TEST(CurveProtocolTest, TestDayBasisNew) {
    DayBasis_ db = DayBasis_New("ACT_365F");
    ASSERT_TRUE(db.String() != nullptr);
}

TEST(CurveProtocolTest, TestDayBasisNewVarious) {
    for (const char* name : {"ACT_365F", "ACT_360", "30_360"}) {
        DayBasis_ db = DayBasis_New(name);
        ASSERT_TRUE(db.String() != nullptr);
    }
}

TEST(CurveProtocolTest, TestRateLegConventionNew) {
    PeriodLength_ freq = PeriodLength_New("6M");
    DayBasis_ basis = DayBasis_New("ACT_365F");
    RateLegConvention_ rlc = RateLegConvention_New(freq, basis);
    // Verify the fields were set correctly
    ASSERT_EQ(rlc.paymentFrequency_.String(), freq.String());
    ASSERT_EQ(rlc.dayBasis_.String(), basis.String());
    ASSERT_EQ(rlc.paymentLag_, 0);
    ASSERT_FALSE(rlc.endOfMonth_);
}

TEST(CurveProtocolTest, TestRateIndexConventionNew) {
    PeriodLength_ tenor = PeriodLength_New("3M");
    DayBasis_ basis = DayBasis_New("ACT_360");
    CollateralType_ collateral = CollateralType_OIS();
    RateIndexConvention_ ric = RateIndexConvention_New(tenor, basis, collateral);
    ASSERT_EQ(ric.forecastTenor_.String(), tenor.String());
    ASSERT_EQ(ric.dayBasis_.String(), basis.String());
    ASSERT_EQ(ric.collateral_.Switch(), CollateralType_::Value_::OIS);
    ASSERT_FALSE(ric.useProjectionCurve_);
    ASSERT_EQ(ric.spotLag_, 0);
    ASSERT_FALSE(ric.endOfMonth_);
}

TEST(CurveProtocolTest, TestRateIndexConventionNewWithProjectionCurve) {
    PeriodLength_ tenor = PeriodLength_New("3M");
    DayBasis_ basis = DayBasis_New("ACT_360");
    CollateralType_ collateral = CollateralType_OIS();
    RateIndexConvention_ ric = RateIndexConvention_New(tenor, basis, collateral, true);
    ASSERT_TRUE(ric.useProjectionCurve_);
}

TEST(CurveProtocolTest, TestCurrencyPairNew) {
    CurrencyPair_ pair = CurrencyPair_New("USD", "EUR");
    ASSERT_TRUE(pair.domestic_.String() != nullptr);
    ASSERT_TRUE(pair.foreign_.String() != nullptr);
}

TEST(CurveProtocolTest, TestCurrencyPairNewVarious) {
    for (const auto& [dom, frn] : {
             std::pair<const char*, const char*>{"USD", "EUR"},
             std::pair<const char*, const char*>{"GBP", "JPY"},
             std::pair<const char*, const char*>{"EUR", "CHF"}}) {
        CurrencyPair_ pair = CurrencyPair_New(dom, frn);
        ASSERT_TRUE(pair.domestic_.String() != nullptr);
        ASSERT_TRUE(pair.foreign_.String() != nullptr);
    }
}

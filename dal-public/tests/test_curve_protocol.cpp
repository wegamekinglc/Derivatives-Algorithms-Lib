//
// Created by wegamekinglc on 2026/6/20.
//

#include <gtest/gtest.h>

#include <map>

#include <dal-public/src/curveprotocol.hpp>

using Dal::CollateralType_;
using Dal::CollateralType_Libor;
using Dal::CollateralType_OIS;
using Dal::CurrencyPair_;
using Dal::CurrencyPair_New;
using Dal::Date_;
using Dal::DateTime_;
using Dal::DayBasis_;
using Dal::DayBasis_New;
using Dal::FxResetConventionNew;
using Dal::Holidays_;
using Dal::MarketFixingSnapshotNew;
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
    for (const auto& [dom, frn] : {std::pair<const char*, const char*>{"USD", "EUR"}, std::pair<const char*, const char*>{"GBP", "JPY"},
                                   std::pair<const char*, const char*>{"EUR", "CHF"}}) {
        CurrencyPair_ pair = CurrencyPair_New(dom, frn);
        ASSERT_TRUE(pair.domestic_.String() != nullptr);
        ASSERT_TRUE(pair.foreign_.String() != nullptr);
    }
}

TEST(CurveProtocolTest, TestFxResetConventionNewRoundTripsEveryField) {
    const auto reset = FxResetConventionNew(2, Holidays_(""), Dal::BizDayConvention_("Following"), 10, 30);

    ASSERT_EQ(reset.fixingLag_, 2);
    ASSERT_EQ(reset.fixingHolidays_, Holidays_(""));
    ASSERT_EQ(reset.fixingConvention_, Dal::BizDayConvention_("Following"));
    ASSERT_EQ(reset.fixingHour_, 10);
    ASSERT_EQ(reset.fixingMinute_, 30);
}

TEST(CurveProtocolTest, TestMarketFixingSnapshotNewCopiesNestedValues) {
    const DateTime_ rateTime(Date_(2025, 6, 19), 11, 0);
    const DateTime_ fxTime(Date_(2025, 6, 19), 10, 30);
    std::map<Dal::String_, std::map<DateTime_, double>> values = {
        {"USD-SOFR-3M", {{rateTime, 0.04325}}},
        {"FX[EUR/USD]", {{fxTime, 1.0825}}},
    };

    const auto snapshot = MarketFixingSnapshotNew(values);
    values["USD-SOFR-3M"][rateTime] = 0.99;

    ASSERT_NE(snapshot, nullptr);
    ASSERT_NEAR(snapshot->Require("USD-SOFR-3M", rateTime, "public snapshot test"), 0.04325, 1.0e-15);
    ASSERT_NEAR(snapshot->Require("FX[EUR/USD]", fxTime, "public snapshot test"), 1.0825, 1.0e-15);
}

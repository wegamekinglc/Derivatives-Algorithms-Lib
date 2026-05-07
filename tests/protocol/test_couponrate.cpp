//
// Created by wegam on 2023/5/21.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/time/periodlength.hpp>
#include <dal/protocol/couponrate.hpp>
#include <dal/protocol/clearer.hpp>

using namespace Dal;

TEST(ProtocolTest, TestFixedRate) {
    FixedRate_ fr(0.05);
    ASSERT_NEAR(fr.rate_, 0.05, 0.001);
}

TEST(ProtocolTest, TestLiborRate) {
    DateTime_ fix_date(Date_(2023, 5, 21));
    Ccy_ ccy("USD");
    TradedRate_ trade_rate = FindRate(PeriodLength_("QUARTERLY"), Clearer_::Value_::CME);
    LiborRate_ libor(fix_date, ccy, trade_rate);

    ASSERT_EQ(libor.fixDate_, fix_date);
    ASSERT_EQ(libor.ccy_, ccy);
    ASSERT_EQ(libor.rate_, trade_rate);
}

TEST(ProtocolTest, TestFindRateAcrossClearersAndPeriods) {
    ASSERT_EQ(FindRate(PeriodLength_("3M"), Clearer_("CME")), TradedRate_::Value_::LIBOR_3M_CME);
    ASSERT_EQ(FindRate(PeriodLength_("3M"), Clearer_("LCH")), TradedRate_::Value_::LIBOR_3M_LCH);
    ASSERT_EQ(FindRate(PeriodLength_("6M"), Clearer_("CME")), TradedRate_::Value_::LIBOR_6M_CME);
    ASSERT_EQ(FindRate(PeriodLength_("6M"), Clearer_("LCH")), TradedRate_::Value_::LIBOR_6M_LCH);
}

TEST(ProtocolTest, TestTradedRateMetadata) {
    TradedRate_ quarterlyFuture("LIBOR3MFUT");
    TradedRate_ semiAnnualSwap("LIBOR6MLCH");

    ASSERT_EQ(quarterlyFuture.Period(), PeriodLength_("3M"));
    ASSERT_EQ(quarterlyFuture.Clearer(), Clearer_::Value_::CME);
    ASSERT_EQ(semiAnnualSwap.Period(), PeriodLength_("6M"));
    ASSERT_EQ(semiAnnualSwap.Clearer(), Clearer_::Value_::LCH);
}

TEST(ProtocolTest, TestFindRateThrowsOnUnsupportedPeriod) {
    ASSERT_THROW(FindRate(PeriodLength_("1M"), Clearer_("CME")), Dal::Exception_);
}

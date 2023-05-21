//
// Created by wegam on 2023/5/21.
//

#include <gtest/gtest.h>
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

    ASSERT_EQ(libor.rate_, trade_rate);
}

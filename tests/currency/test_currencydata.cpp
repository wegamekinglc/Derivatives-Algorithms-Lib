//
// Created by wegam on 2022/2/10.
//

#include <gtest/gtest.h>
#include <dal/currency/currencydata.hpp>
#include <dal/time/holidays.hpp>

using namespace Dal;

TEST(CurrencyTest, TestFactRead) {
    ASSERT_EQ(Ccy::Conventions::LiborFixDays()(Ccy_("CNY")), 1);
    ASSERT_EQ(Ccy::Conventions::LiborFixDays()(Ccy_("USD")), 2);

    ASSERT_EQ(Ccy::Conventions::LiborFixHolidays()(Ccy_("CNY")).String(), "CN.IB");
}

TEST(CurrencyDataTest, TestFactWrite) {
    Ccy::Conventions::LiborFixDays().XWrite()(Ccy_("EUR"), 1);
    ASSERT_EQ(Ccy::Conventions::LiborFixDays()(Ccy_("EUR")), 1);

    Ccy::Conventions::LiborFixHolidays().XWrite()(Ccy_("CNY"), Holidays_("CN.SSE"));
    ASSERT_EQ(Ccy::Conventions::LiborFixHolidays()(Ccy_("CNY")).String(), "CN.SSE");
}

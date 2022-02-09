//
// Created by wegam on 2022/2/10.
//

#include <gtest/gtest.h>
#include <dal/currency/currencydata.hpp>

using namespace Dal;

TEST(CurrencyDataTest, TestFactRead) {
    ASSERT_EQ(Ccy::Conventions::LiborFixDays()(Ccy_("CNY")), 1);
    ASSERT_EQ(Ccy::Conventions::LiborFixDays()(Ccy_("USD")), 2);
}

TEST(CurrencyDataTest, TestFactWrite) {
    Ccy::Conventions::LiborFixDays().XWrite()(Ccy_("EUR"), 1);
    ASSERT_EQ(Ccy::Conventions::LiborFixDays()(Ccy_("EUR")), 1);
}

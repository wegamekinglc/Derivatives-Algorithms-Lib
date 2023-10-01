//
// Created by wegam on 2022/12/9.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/time/date.hpp>
#include <dal/indice/parser/equity.hpp>
#include <dal/protocol/underlying.hpp>


TEST(ProtocolTest, TestUnderlying) {
    using namespace Dal;
    Underlying_ underlying;

    Ccy_ cny("CNY");
    Date_ pay_date(2022, 12, 9);

    underlying.Include(cny, pay_date);
    ASSERT_EQ(underlying.payCcys_.size(), 1);

    String_ name = "EQ[IBM]";
    Handle_<Index_> index(Index::EquityParser(name));
    DateTime_ fix_date(pay_date, 0.5);
    underlying.Include(index, fix_date);
    ASSERT_EQ(underlying.indices_.size(), 1);

    underlying.Include("EQ[IBM]", pay_date);
    ASSERT_EQ(underlying.credits_.size(), 1);
}
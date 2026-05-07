//
// Created by Copilot on 2026/5/7.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/currency/currency.hpp>
#include <dal/protocol/conventions.hpp>
#include <dal/time/date.hpp>
#include <dal/time/datetime.hpp>

using namespace Dal;

TEST(ProtocolTest, TestLiborStartFromFixUsesCurrencySpecificFixDays) {
    Date_ fixDate(2024, 3, 13);

    ASSERT_EQ(Libor::StartFromFix(Ccy_("CNY"), fixDate), Date_(2024, 3, 14));
}

TEST(ProtocolTest, TestLiborFixFromStartReturnsTenAmFixingTime) {
    ASSERT_EQ(Libor::FixFromStart(Ccy_("CNY"), Date_(2024, 3, 14)), DateTime_(Date_(2024, 3, 13), 10));
}

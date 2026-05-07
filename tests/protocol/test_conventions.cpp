//
// Created by Copilot on 2026/5/7.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/protocol/conventions.hpp>

using namespace Dal;

TEST(ProtocolTest, TestLiborStartFromFixUsesCurrencySpecificFixDays) {
    Date_ fixDate(2024, 3, 13);

    ASSERT_EQ(Libor::StartFromFix(Ccy_("USD"), fixDate), Date_(2024, 3, 15));
    ASSERT_EQ(Libor::StartFromFix(Ccy_("CNY"), fixDate), Date_(2024, 3, 14));
}

TEST(ProtocolTest, TestLiborFixFromStartReturnsTenAmFixingTime) {
    ASSERT_EQ(Libor::FixFromStart(Ccy_("USD"), Date_(2024, 3, 15)), DateTime_(Date_(2024, 3, 13), 10));
    ASSERT_EQ(Libor::FixFromStart(Ccy_("CNY"), Date_(2024, 3, 14)), DateTime_(Date_(2024, 3, 13), 10));
}

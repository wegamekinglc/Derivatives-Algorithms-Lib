//
// Created by Copilot on 2026/5/7.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/protocol/clearer.hpp>

using namespace Dal;

TEST(ProtocolTest, TestClearerParsingAndListAll) {
    const Vector_<Clearer_> all = ClearerListAll();

    ASSERT_EQ(all.size(), 2);
    ASSERT_EQ(Clearer_("CME"), Clearer_::Value_::CME);
    ASSERT_EQ(Clearer_("LCH"), Clearer_::Value_::LCH);
    ASSERT_EQ(String_(all[0].String()), String_("CME"));
    ASSERT_EQ(String_(all[1].String()), String_("LCH"));
}

TEST(ProtocolTest, TestClearerThrowsOnInvalidString) {
    ASSERT_THROW(Clearer_("ICE"), Dal::Exception_);
}

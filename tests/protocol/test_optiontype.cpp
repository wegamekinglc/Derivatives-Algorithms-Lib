//
// Created by Copilot on 2026/5/7.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/math/operators.hpp>
#include <dal/protocol/optiontype.hpp>

using namespace Dal;

TEST(ProtocolTest, TestOptionTypePayouts) {
    OptionType_ call("C");
    OptionType_ put("P");
    OptionType_ straddle("C+P");

    ASSERT_NEAR(call.Payout(120.0, 100.0), 20.0, 1e-10);
    ASSERT_NEAR(call.Payout(80.0, 100.0), 0.0, 1e-10);
    ASSERT_NEAR(put.Payout(80.0, 100.0), 20.0, 1e-10);
    ASSERT_NEAR(put.Payout(120.0, 100.0), 0.0, 1e-10);
    ASSERT_NEAR(straddle.Payout(120.0, 100.0), 20.0, 1e-10);
    ASSERT_NEAR(straddle.Payout(80.0, 100.0), 20.0, 1e-10);
}

TEST(ProtocolTest, TestOptionTypeParsingAndListAll) {
    const Vector_<OptionType_> all = OptionTypeListAll();

    ASSERT_EQ(all.size(), 3);
    ASSERT_EQ(OptionType_("CALL"), OptionType_::Value_::CALL);
    ASSERT_EQ(OptionType_("put"), OptionType_::Value_::PUT);
    ASSERT_EQ(OptionType_("V"), OptionType_::Value_::STRADDLE);
    ASSERT_EQ(String_(all[0].String()), String_("CALL"));
    ASSERT_EQ(String_(all[1].String()), String_("PUT"));
    ASSERT_EQ(String_(all[2].String()), String_("STRADDLE"));
}

TEST(ProtocolTest, TestOptionTypeThrowsOnInvalidString) {
    ASSERT_THROW(OptionType_("digital"), Dal::Exception_);
}

//
// Created by Copilot on 2026/5/7.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/protocol/collateraltype.hpp>

using namespace Dal;

TEST(ProtocolTest, TestCollateralTypeParsingAndListAll) {
    const Vector_<CollateralType_> all = CollateralTypeListAll();

    ASSERT_EQ(all.size(), 3);
    ASSERT_EQ(CollateralType_("OIS"), CollateralType_::Value_::OIS);
    ASSERT_EQ(CollateralType_("GC"), CollateralType_::Value_::GC);
    ASSERT_EQ(CollateralType_("NONE"), CollateralType_::Value_::NONE);
    ASSERT_EQ(String_(all[0].String()), String_("OIS"));
    ASSERT_EQ(String_(all[1].String()), String_("GC"));
    ASSERT_EQ(String_(all[2].String()), String_("NONE"));
}

TEST(ProtocolTest, TestCollateralTypeThrowsOnInvalidString) {
    ASSERT_THROW(CollateralType_("CSA"), Dal::Exception_);
}

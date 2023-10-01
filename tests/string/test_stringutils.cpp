//
// Created by wegamekinglc on 2020/5/2.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/utilities/exceptions.hpp>
#include <dal/string/stringutils.hpp>

using Dal::String_;
using Dal::Vector_;
using Dal::String::ToBool;
using Dal::String::ToBoolVector;

TEST(StringUtilsTest, TestToBoolTrue) {
    auto s = String_("TRUE");
    ASSERT_TRUE(ToBool(s));

    s = String_("true");
    ASSERT_TRUE(ToBool(s));
}

TEST(StringUtilsTest, TestToBoolFalse) {
    auto s = String_("FALSE");
    ASSERT_FALSE(ToBool(s));

    s = String_("false");
    ASSERT_FALSE(ToBool(s));
}

TEST(StringUtilsTest, TestToBoolSingleCharTrue) {
    auto s = String_("1");
    ASSERT_TRUE(ToBool(s));

    s = String_("T");
    ASSERT_TRUE(ToBool(s));
}

TEST(StringUtilsTest, TestToBoolSingleCharFalse) {
    auto s = String_("0");
    ASSERT_FALSE(ToBool(s));

    s = String_("F");
    ASSERT_FALSE(ToBool(s));
}

TEST(StringUtilsTest, TestToBoolRaise) {
    auto s = String_("tf");
    ASSERT_THROW(ToBool(s), Dal::Exception_);
}

TEST(StringUtilsTest, TestToBoolVector) {
    auto s = String_("True,False,0,1");
    auto calculated = ToBoolVector(s);
    Vector_<bool> expected = {true, false, false, true};

    ASSERT_EQ(calculated, expected);
}
//
// Created by Cheng Li on 2018/1/7.
//

#include <gtest/gtest.h>
#include <dal/platform/strict.hpp>
#include <dal/math/vectors.hpp>
#include <dal/string/strings.hpp>

using dal::String_;
using dal::Vector_;
using base_t = std::basic_string<char, dal::ci_traits>;

using namespace dal::string;

TEST(StringsTest, TestEmptyString) {
    String_ s;
    ASSERT_EQ(s.size(), 0);
}

TEST(StringsTest, TestBuildFromCStyleString) {
    const char* src = "hello";
    String_ s(src);
    ASSERT_EQ(s.size(), 5);
}

TEST(StringsTest, TestBuildFromBasicString) {
    base_t src("hello");
    String_ s(src);
    ASSERT_EQ(s.size(), 5);
}

TEST(StringsTest, TestBuildFromNumberOfChar) {
    const int n = 5;
    String_ s(n, 'a');
    ASSERT_EQ(s.size(), n);

    for(auto i = 0; i != n; ++i)
        ASSERT_EQ(s[i], 'a');
}

TEST(StringsTest, TestBuildWithIterator) {
    String_ s1("hello");
    String_ s2(s1.begin(), s1.end());

    ASSERT_EQ(s2.size(), 5);

    for(auto i = 0; i != s2.size(); ++i)
        ASSERT_EQ(s1[i], s2[i]);
}

TEST(StringsTest, TestBuildWithVanillaString) {
    std::string src("hello");
    String_ s2(src);

    ASSERT_EQ(s2.size(), 5);
    for(auto i = 0; i != s2.size(); ++i)
        ASSERT_EQ(s2[i], src[i]);
}

TEST(StringsTest, TestStringSwap) {
    String_ s1("hello");
    String_ s2;

    s2.Swap(&s1);

    ASSERT_EQ(s1.size(), 0);
    ASSERT_EQ(s2.size(), 5);
}

TEST(StringsTest, TestStringEqual) {
    String_ s1("HELLO");
    String_ s2("hello");
    String_ s3("HELLE");

    ASSERT_TRUE(s1 == s2);
    ASSERT_FALSE(s1 == s3);
}

TEST(StringsTest, TestStringEqualWithBaseString) {
    String_ s1("HELLO");
    base_t s2("hello");
    base_t s3("HELLE");

    ASSERT_TRUE(s1 == s2);
    ASSERT_FALSE(s3 == s1);
}

TEST(StringsTest, TestStringEqualWithCStyleString) {
    String_ s1("HELLO");
    const char* s2 = "hello";
    const char* s3 = "HELLE";

    ASSERT_TRUE(s1 == s2);
    ASSERT_FALSE(s3 == s1);
}

TEST(StringsTest, TestSplit) {
    String_ s1("1,,2,3");
    Vector_<String_> ret_val = Split(s1, ',', true);
    Vector_<String_> exp_val = {String_("1"), String_(""), String_("2"), String_("3")};

    for(auto i = 0; i != ret_val.size(); ++i)
        ASSERT_EQ(ret_val[i], exp_val[i]);
}

TEST(StringsTest, TestSplitWithEmptyRemoved) {
    String_ s1("1,,2,3");
    Vector_<String_> ret_val = Split(s1, ',', false);
    Vector_<String_> exp_val = {String_("1"), String_("2"), String_("3")};

    for(auto i = 0; i != ret_val.size(); ++i)
        ASSERT_EQ(ret_val[i], exp_val[i]);
}

TEST(StringsTest, TestStringIsNumber) {
    String_ s1("a");
    ASSERT_FALSE(IsNumber(s1));

    String_ s2("1.");
    ASSERT_TRUE(IsNumber(s2));
}

TEST(StringsTest, TestStringToDouble) {
    String_ s1("1.2");
    ASSERT_DOUBLE_EQ(1.2, ToDouble(s1));
}

TEST(StringsTest, TestStringToInt) {
    String_ s1("1");
    ASSERT_EQ(1, ToInt(s1));
}

TEST(StringsTest, TestFromDouble) {
    double n1 = 1.;
    String_ s1 = FromDouble(n1);
    ASSERT_DOUBLE_EQ(ToDouble(s1), n1);
}

TEST(StringsTest, TestFromInt) {
    int n1 = 1;
    String_ s1 = FromInt(n1);
    ASSERT_DOUBLE_EQ(ToInt(s1), n1);
}

TEST(StringsTest, TestCondensed) {
    String_ s1("123,abc,\t");
    auto s2 = Condensed(s1);
    ASSERT_TRUE(s2 == "123,ABC,");
}

TEST(StringsTest, TestEquivalent) {
    String_ s1("   123,abc,\t");
    auto flag = Equivalent(s1, "123,abc,");
    ASSERT_TRUE(flag);

    flag = Equivalent(s1, "123,abc,\t");
    ASSERT_FALSE(flag);
}

TEST(StringsTest, TestNextName) {
    String_ s1;
    auto s2 = NextName(s1);
    ASSERT_EQ(s2, "0");

    s1 = String_("123456789");
    s2 = NextName(s1);
    ASSERT_EQ(s2, "123456790");
}

TEST(StringsTest, TestAccumulate) {
    Vector_<String_> c = {String_("1"), String_("2"), String_("3")};
    auto s1 = Accumulate(c, String_(","));
    ASSERT_EQ(s1, "1,2,3");
}
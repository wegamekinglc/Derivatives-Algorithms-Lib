//
// Created by Cheng Li on 2018/1/7.
//

#include <gtest/gtest.h>
#include <dal/platform/strict.hpp>
#include <dal/string/strings.hpp>

using dal::String_;
using base_t = std::basic_string<char, dal::ci_traits>;

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
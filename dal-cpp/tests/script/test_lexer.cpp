//
// Created by wegam on 2026/5/30.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/script/lexer.hpp>

using namespace Dal;
using namespace Dal::Script;

TEST(ScriptLexerTest, TestTokenizeAssignment) {
    auto tokens = Tokenize("x = 2");
    ASSERT_EQ(tokens.size(), 3);
    ASSERT_EQ(tokens[0], "x");
    ASSERT_EQ(tokens[1], "=");
    ASSERT_EQ(tokens[2], "2");
}

TEST(ScriptLexerTest, TestTokenizeOperatorsAndParens) {
    auto tokens = Tokenize("MAX(spot()-K,0.0)");
    Vector_<String_> expected = {"MAX", "(", "spot", "(", ")", "-", "K", ",", "0.0", ")"};
    ASSERT_EQ(tokens.size(), expected.size());
    for (auto i = 0; i < expected.size(); ++i)
        ASSERT_EQ(tokens[i], expected[i]);
}

TEST(ScriptLexerTest, TestTokenizeComparators) {
    auto tokens = Tokenize("a >= b != c <= d");
    Vector_<String_> expected = {"a", ">=", "b", "!=", "c", "<=", "d"};
    ASSERT_EQ(tokens.size(), expected.size());
    for (auto i = 0; i < expected.size(); ++i)
        ASSERT_EQ(tokens[i], expected[i]);
}

TEST(ScriptLexerTest, TestTokenizeScheduleColon) {
    auto tokens = Tokenize("START: 2022-05-07");
    Vector_<String_> expected = {"START", ":", "2022", "-", "05", "-", "07"};
    ASSERT_EQ(tokens.size(), expected.size());
    for (auto i = 0; i < expected.size(); ++i)
        ASSERT_EQ(tokens[i], expected[i]);
}

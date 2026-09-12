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
    for (size_t i = 0; i < expected.size(); ++i)
        ASSERT_EQ(tokens[i], expected[i]);
}

TEST(ScriptLexerTest, TestTokenizeComparators) {
    auto tokens = Tokenize("a >= b != c <= d");
    Vector_<String_> expected = {"a", ">=", "b", "!=", "c", "<=", "d"};
    ASSERT_EQ(tokens.size(), expected.size());
    for (size_t i = 0; i < expected.size(); ++i)
        ASSERT_EQ(tokens[i], expected[i]);
}

TEST(ScriptLexerTest, TestTokenizeScheduleColon) {
    auto tokens = Tokenize("START: 2022-05-07");
    Vector_<String_> expected = {"START", ":", "2022", "-", "05", "-", "07"};
    ASSERT_EQ(tokens.size(), expected.size());
    for (size_t i = 0; i < expected.size(); ++i)
        ASSERT_EQ(tokens[i], expected[i]);
}

TEST(ScriptLexerTest, TestIndexLiteralTokens) {
    const auto tokens = Tokenize("x = FIX(FX[EUR/USD]) + FIX(EQ[Aapl]@2026-12-31, 2026-09-11)");
    ASSERT_EQ(tokens[4], "FX[EUR/USD]");
    ASSERT_EQ(tokens[9], "EQ[Aapl]@2026-12-31");
    const auto positioned = Lex("FIX(FX[EUR/USD])");
    const auto* literal = std::get_if<IndexLiteral_>(&positioned[2].value_);
    ASSERT_NE(literal, nullptr);
    ASSERT_EQ(literal->raw_, "FX[EUR/USD]");
    ASSERT_EQ(positioned[2].source_.offset_, 4);
}

//
// Created by wegam on 2022/6/5.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/script/parser.hpp>
#include <dal/script/node.hpp>
#include <dal/script/visitor/all.hpp>

using namespace Dal;
using namespace Dal::Script;

namespace {
    // Validate the operand order of the NodeSub_ inside a comparison node.
    // The parser encodes both `<` and `>` as NodeSup_ (and both `<=`/`>=` as
    // NodeSupEqual_), distinguishing them only by swapping the subtraction
    // operands: `x > 2` becomes `x - 2`, while `x < 2` becomes `2 - x`. With
    // constFirst=true the subtraction is `c - v`; otherwise it is `v - c`.
    void AssertCmpSubOrder(const Node_* cmp, bool constFirst, double c, const String_& v) {
        ASSERT_NE(cmp, nullptr);
        auto sub = dynamic_cast<const NodeSub_*>(cmp->arguments_[0].get());
        ASSERT_NE(sub, nullptr);
        const Node_* lhs = sub->arguments_[0].get();
        const Node_* rhs = sub->arguments_[1].get();
        auto cn = dynamic_cast<const NodeConst_*>(constFirst ? lhs : rhs);
        auto vn = dynamic_cast<const NodeVar_*>(constFirst ? rhs : lhs);
        ASSERT_NE(cn, nullptr);
        ASSERT_NE(vn, nullptr);
        ASSERT_DOUBLE_EQ(cn->constVal_, c);
        ASSERT_EQ(vn->name_, v);
    }
} // namespace

TEST(ScriptTest, TestParseAssign) {
    Parser_ parser;
    String_ event = "x = 2";
    auto res = parser.Parse(event);
    ASSERT_EQ(res.size(), 1);
    auto toTest1 = dynamic_cast<NodeAssign_*>(res[0].get());
    ASSERT_NE(dynamic_cast<NodeVar_*>(toTest1->arguments_[0].get()), nullptr);
    auto toTest3 = dynamic_cast<NodeConst_*>(toTest1->arguments_[1].get());
    ASSERT_NEAR(toTest3->constVal_, 2.0, 1e-10);
}

TEST(ScriptTest, TestParseLog) {
    Parser_ parser;
    String_ event = R"(
        y = 2.0
        x = Log(y)
    )";
    auto res = parser.Parse(event);
    ASSERT_EQ(res.size(), 2);
    auto toTest1 = dynamic_cast<NodeLog_*>(res[1]->arguments_[1].get());
    ASSERT_NE(toTest1, nullptr);
}

TEST(ScriptTest, TestParseExp) {
    Parser_ parser;
    String_ event = R"(
        y = 2.0
        x = Exp(y)
    )";
    auto res = parser.Parse(event);
    ASSERT_EQ(res.size(), 2);
    auto toTest1 = dynamic_cast<NodeExp_*>(res[1]->arguments_[1].get());
    ASSERT_NE(toTest1, nullptr);
}

TEST(ScriptTest, TestParseSqrt) {
    Parser_ parser;
    String_ event = R"(
        y = 2.0
        x = Sqrt(y)
    )";
    auto res = parser.Parse(event);
    ASSERT_EQ(res.size(), 2);
    auto toTest1 = dynamic_cast<NodeSqrt_*>(res[1]->arguments_[1].get());
    ASSERT_NE(toTest1, nullptr);
}

TEST(ScriptTest, TestParserDCF) {
    Parser_ parser;
    String_ event = "x = DCF(ACT365F, 2023-04-23, 2024-04-23)";
    auto res = parser.Parse(event);
    const auto val = dynamic_cast<NodeConst_*>(res[0]->arguments_[1].get())->constVal_;
    ASSERT_NEAR(val, 1.00274, 1e-5);
}

TEST(ScriptTest, TestParserDCFRejectsExtraArguments) {
    Parser_ parser;
    ASSERT_THROW(parser.Parse("x = DCF(ACT365F, 2023-04-23, 2024-04-23, 2025-04-23)"), ScriptError_);
}

TEST(ScriptTest, TestParseIf) {
    Parser_ parser;
    String_ event = R"(
        IF x >= 2 THEN
            y = 3 + x
        END
    )";
    auto res = parser.Parse(event);
    ASSERT_EQ(res.size(), 1);
    auto toTest1 = dynamic_cast<NodeIf_*>(res[0].get());
    ASSERT_EQ(toTest1->firstElse_, -1);

    auto toTest2 = dynamic_cast<NodeSupEqual_*>(toTest1->arguments_[0].get());
    auto toTest3 = dynamic_cast<NodeSub_*>(toTest2->arguments_[0].get());
    ASSERT_NE(dynamic_cast<NodeVar_*>(toTest3->arguments_[0].get()), nullptr);
    ASSERT_NE(dynamic_cast<NodeConst_*>(toTest3->arguments_[1].get()), nullptr);

    auto toTest6 = dynamic_cast<NodeAssign_*>(toTest1->arguments_[1].get());
    ASSERT_NE(dynamic_cast<NodeVar_*>(toTest6->arguments_[0].get()), nullptr);
    auto toTest8 = dynamic_cast<NodeAdd_*>(toTest6->arguments_[1].get());
    ASSERT_NE(dynamic_cast<NodeConst_*>(toTest8->arguments_[0].get()), nullptr);
    ASSERT_NE(dynamic_cast<NodeVar_*>(toTest8->arguments_[1].get()), nullptr);
}

TEST(ScriptTest, TestParseIfWithElse) {
    Parser_ parser;
    String_ event = R"(
        IF x >= 2 THEN
            y = 3 + x
        ELSE
            y = x
        END
    )";
    auto res = parser.Parse(event);
    ASSERT_EQ(res.size(), 1);
    auto toTest1 = dynamic_cast<NodeIf_*>(res[0].get());
    ASSERT_TRUE(toTest1);
    ASSERT_EQ(toTest1->firstElse_, 2);

    auto toTest2 = dynamic_cast<NodeSupEqual_*>(toTest1->arguments_[0].get());
    auto toTest3 = dynamic_cast<NodeSub_*>(toTest2->arguments_[0].get());
    ASSERT_NE(dynamic_cast<NodeVar_*>(toTest3->arguments_[0].get()), nullptr);
    ASSERT_NE(dynamic_cast<NodeConst_*>(toTest3->arguments_[1].get()), nullptr);

    auto toTest6 = dynamic_cast<NodeAssign_*>(toTest1->arguments_[1].get());
    ASSERT_NE(dynamic_cast<NodeVar_*>(toTest6->arguments_[0].get()), nullptr);
    auto toTest8 = dynamic_cast<NodeAdd_*>(toTest6->arguments_[1].get());
    ASSERT_NE(dynamic_cast<NodeConst_*>(toTest8->arguments_[0].get()), nullptr);
    ASSERT_NE(dynamic_cast<NodeVar_*>(toTest8->arguments_[1].get()), nullptr);

    auto toTest11 = dynamic_cast<NodeAssign_*>(toTest1->arguments_[2].get());
    ASSERT_NE(dynamic_cast<NodeVar_*>(toTest11->arguments_[0].get()), nullptr);
    ASSERT_NE(dynamic_cast<NodeVar_*>(toTest11->arguments_[1].get()), nullptr);
}

TEST(ScriptTest, TestParserWithInvaildVaribaleName) {
    Parser_ parser;
    String_ event = R"(
        0X12 = 2.0;
    )";
    ASSERT_THROW(parser.Parse(event), ScriptError_);
}

TEST(ScriptTest, TestParserWithConflictVariableName) {
    Parser_ parser;
    String_ event = R"(
        PAYS = 2.0;
    )";
    ASSERT_THROW(parser.Parse(event), ScriptError_);
}

TEST(ScriptTest, TestParsePrecedenceAddMul) {
    Parser_ parser;
    String_ event = "x = 2 + 3 * 4";
    auto res = parser.Parse(event);
    auto assign = dynamic_cast<NodeAssign_*>(res[0].get());
    auto add = dynamic_cast<NodeAdd_*>(assign->arguments_[1].get());
    ASSERT_NE(add, nullptr);
    ASSERT_NE(dynamic_cast<NodeConst_*>(add->arguments_[0].get()), nullptr);
    ASSERT_NE(dynamic_cast<NodeMulti_*>(add->arguments_[1].get()), nullptr);
}

TEST(ScriptTest, TestParsePrecedenceMulAdd) {
    Parser_ parser;
    String_ event = "x = 2 * 3 + 4";
    auto res = parser.Parse(event);
    auto assign = dynamic_cast<NodeAssign_*>(res[0].get());
    auto add = dynamic_cast<NodeAdd_*>(assign->arguments_[1].get());
    ASSERT_NE(add, nullptr);
    ASSERT_NE(dynamic_cast<NodeMulti_*>(add->arguments_[0].get()), nullptr);
    ASSERT_NE(dynamic_cast<NodeConst_*>(add->arguments_[1].get()), nullptr);
}

TEST(ScriptTest, TestParsePrecedencePowOverMul) {
    Parser_ parser;
    String_ event = "x = 2 * 3 ^ 4";
    auto res = parser.Parse(event);
    auto assign = dynamic_cast<NodeAssign_*>(res[0].get());
    auto mul = dynamic_cast<NodeMulti_*>(assign->arguments_[1].get());
    ASSERT_NE(mul, nullptr);
    ASSERT_NE(dynamic_cast<NodeConst_*>(mul->arguments_[0].get()), nullptr);
    ASSERT_NE(dynamic_cast<NodePow_*>(mul->arguments_[1].get()), nullptr);
}

TEST(ScriptTest, TestParseParenthesesOverridePrecedence) {
    Parser_ parser;
    String_ event = "x = (2 + 3) * 4";
    auto res = parser.Parse(event);
    auto assign = dynamic_cast<NodeAssign_*>(res[0].get());
    auto mul = dynamic_cast<NodeMulti_*>(assign->arguments_[1].get());
    ASSERT_NE(mul, nullptr);
    ASSERT_NE(dynamic_cast<NodeAdd_*>(mul->arguments_[0].get()), nullptr);
    ASSERT_NE(dynamic_cast<NodeConst_*>(mul->arguments_[1].get()), nullptr);
}

TEST(ScriptTest, TestParseUnaryMinus) {
    Parser_ parser;
    String_ event = "x = -3";
    auto res = parser.Parse(event);
    auto assign = dynamic_cast<NodeAssign_*>(res[0].get());
    auto uminus = dynamic_cast<NodeUMinus_*>(assign->arguments_[1].get());
    ASSERT_NE(uminus, nullptr);
    ASSERT_NE(dynamic_cast<NodeConst_*>(uminus->arguments_[0].get()), nullptr);
}

TEST(ScriptTest, TestParseUnaryPlus) {
    Parser_ parser;
    String_ event = "x = +3";
    auto res = parser.Parse(event);
    auto assign = dynamic_cast<NodeAssign_*>(res[0].get());
    auto uplus = dynamic_cast<NodeUPlus_*>(assign->arguments_[1].get());
    ASSERT_NE(uplus, nullptr);
}


TEST(ScriptTest, TestParseMin) {
    Parser_ parser;
    String_ event = "x = MIN(2, 3)";
    auto res = parser.Parse(event);
    auto assign = dynamic_cast<NodeAssign_*>(res[0].get());
    auto min = dynamic_cast<NodeMin_*>(assign->arguments_[1].get());
    ASSERT_NE(min, nullptr);
    ASSERT_EQ(min->arguments_.size(), 2);
}

TEST(ScriptTest, TestParseMax) {
    Parser_ parser;
    String_ event = "x = MAX(2, 3, 4)";
    auto res = parser.Parse(event);
    auto assign = dynamic_cast<NodeAssign_*>(res[0].get());
    auto max = dynamic_cast<NodeMax_*>(assign->arguments_[1].get());
    ASSERT_NE(max, nullptr);
    ASSERT_EQ(max->arguments_.size(), 3);
}

TEST(ScriptTest, TestParseMinWrongArgCountThrows) {
    Parser_ parser;
    String_ event = "x = MIN(2)";
    ASSERT_THROW(parser.Parse(event), ScriptError_);
}

TEST(ScriptTest, TestParsePow) {
    Parser_ parser;
    String_ event = "x = 2 ^ 3";
    auto res = parser.Parse(event);
    auto assign = dynamic_cast<NodeAssign_*>(res[0].get());
    auto pow = dynamic_cast<NodePow_*>(assign->arguments_[1].get());
    ASSERT_NE(pow, nullptr);
    ASSERT_NE(dynamic_cast<NodeConst_*>(pow->arguments_[0].get()), nullptr);
    ASSERT_NE(dynamic_cast<NodeConst_*>(pow->arguments_[1].get()), nullptr);
}


TEST(ScriptTest, TestParseCondGreater) {
    Parser_ parser;
    String_ event = "IF x > 2 THEN y = 1 END";
    auto res = parser.Parse(event);
    auto ifNode = dynamic_cast<NodeIf_*>(res[0].get());
    ASSERT_NE(dynamic_cast<NodeSup_*>(ifNode->arguments_[0].get()), nullptr);
}

TEST(ScriptTest, TestParseCondLess) {
    Parser_ parser;
    String_ event = "IF x < 2 THEN y = 1 END";
    auto res = parser.Parse(event);
    auto ifNode = dynamic_cast<NodeIf_*>(res[0].get());
    auto sup = dynamic_cast<NodeSup_*>(ifNode->arguments_[0].get());
    ASSERT_NE(sup, nullptr);
    AssertCmpSubOrder(sup, true, 2.0, "x"); // x < 2  ->  2 - x > 0
}

TEST(ScriptTest, TestParseCondLessEqual) {
    Parser_ parser;
    String_ event = "IF x <= 2 THEN y = 1 END";
    auto res = parser.Parse(event);
    auto ifNode = dynamic_cast<NodeIf_*>(res[0].get());
    auto supEq = dynamic_cast<NodeSupEqual_*>(ifNode->arguments_[0].get());
    ASSERT_NE(supEq, nullptr);
    AssertCmpSubOrder(supEq, true, 2.0, "x"); // x <= 2  ->  2 - x >= 0
}

TEST(ScriptTest, TestParseCondEqual) {
    Parser_ parser;
    String_ event = "IF x = 2 THEN y = 1 END";
    auto res = parser.Parse(event);
    auto ifNode = dynamic_cast<NodeIf_*>(res[0].get());
    ASSERT_NE(dynamic_cast<NodeEqual_*>(ifNode->arguments_[0].get()), nullptr);
}

TEST(ScriptTest, TestParseCondNotEqual) {
    Parser_ parser;
    String_ event = "IF x != 2 THEN y = 1 END";
    auto res = parser.Parse(event);
    auto ifNode = dynamic_cast<NodeIf_*>(res[0].get());
    auto notNode = dynamic_cast<NodeNot_*>(ifNode->arguments_[0].get());
    ASSERT_NE(notNode, nullptr);
    ASSERT_NE(dynamic_cast<NodeEqual_*>(notNode->arguments_[0].get()), nullptr);
}


TEST(ScriptTest, TestParseCondAnd) {
    Parser_ parser;
    String_ event = "IF x > 2 AND x < 5 THEN y = 1 END";
    auto res = parser.Parse(event);
    auto ifNode = dynamic_cast<NodeIf_*>(res[0].get());
    auto andNode = dynamic_cast<NodeAnd_*>(ifNode->arguments_[0].get());
    ASSERT_NE(andNode, nullptr);
    auto greater = dynamic_cast<NodeSup_*>(andNode->arguments_[0].get());
    auto less = dynamic_cast<NodeSup_*>(andNode->arguments_[1].get());
    ASSERT_NE(greater, nullptr);
    ASSERT_NE(less, nullptr);
    AssertCmpSubOrder(greater, false, 2.0, "x"); // x > 2  ->  x - 2 > 0
    AssertCmpSubOrder(less, true, 5.0, "x");      // x < 5  ->  5 - x > 0
}

TEST(ScriptTest, TestParseCondOr) {
    Parser_ parser;
    String_ event = "IF x > 2 OR x < 5 THEN y = 1 END";
    auto res = parser.Parse(event);
    auto ifNode = dynamic_cast<NodeIf_*>(res[0].get());
    auto orNode = dynamic_cast<NodeOr_*>(ifNode->arguments_[0].get());
    ASSERT_NE(orNode, nullptr);
}

TEST(ScriptTest, TestParseCondAndBindsTighterThanOr) {
    Parser_ parser;
    String_ event = "IF x > 1 OR x > 2 AND x > 3 THEN y = 1 END";
    auto res = parser.Parse(event);
    auto ifNode = dynamic_cast<NodeIf_*>(res[0].get());
    auto orNode = dynamic_cast<NodeOr_*>(ifNode->arguments_[0].get());
    ASSERT_NE(orNode, nullptr);
    ASSERT_NE(dynamic_cast<NodeSup_*>(orNode->arguments_[0].get()), nullptr);
    ASSERT_NE(dynamic_cast<NodeAnd_*>(orNode->arguments_[1].get()), nullptr);
}

TEST(ScriptTest, TestParseUnbalancedParenThrows) {
    Parser_ parser;
    String_ event = "x = (2 + 3";
    ASSERT_THROW(parser.Parse(event), ScriptError_);
}

TEST(ScriptTest, TestParseDanglingPlusThrows) {
    Parser_ parser;
    String_ event = "x = 2 +";
    ASSERT_THROW(parser.Parse(event), ScriptError_);
}

TEST(ScriptTest, TestParseTerminalFunctionAndConditionTokensThrow) {
    Parser_ parser;
    for (const String_& event : {String_("x = DCF"), String_("x = LOG"), String_("IF x > 1")}) {
        SCOPED_TRACE(event.c_str());
        ASSERT_THROW(parser.Parse(event), ScriptError_);
    }
}

TEST(ScriptTest, TestParseIfWithoutThenThrows) {
    Parser_ parser;
    String_ event = "IF x > 2 y = 1 END";
    ASSERT_THROW(parser.Parse(event), ScriptError_);
}

TEST(ScriptTest, TestParseStatementWithoutInstructionThrows) {
    Parser_ parser;
    String_ event = "x 2";
    ASSERT_THROW(parser.Parse(event), ScriptError_);
}

TEST(ScriptTest, TestParseCondInvalidComparatorThrows) {
    Parser_ parser;
    String_ event = "IF x + 2 THEN y = 1 END";
    ASSERT_THROW(parser.Parse(event), ScriptError_);
}

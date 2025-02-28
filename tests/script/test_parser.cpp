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
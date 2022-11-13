//
// Created by wegam on 2022/6/5.
//

#include <gtest/gtest.h>
#include <dal/script/parser.hpp>
#include <dal/script/node.hpp>

using namespace Dal;
using namespace Dal::Script;

TEST(ParserTest, TestParseAssign) {
    String_ event = "x = 2";
    auto res = Parse(event);
    ASSERT_EQ(res.size(), 1);
    auto& toTest1 = std::get<std::unique_ptr<NodeAssign_>>(res[0]);
    auto& toTest2 = std::get<std::unique_ptr<NodeVar_>>(toTest1->arguments_[0]);
    auto& toTest3 = std::get<std::unique_ptr<NodeConst_>>(toTest1->arguments_[1]);
    ASSERT_NEAR(toTest3->val_, 2.0, 1e-10);
}

TEST(ParserTest, TestParseIf) {
    String_ event = R"(
        IF x >= 2 THEN
            y = 3 + x
        ENDIF
    )";
    auto res = Parse(event);
    ASSERT_EQ(res.size(), 1);
    auto& toTest1 = std::get<std::unique_ptr<NodeIf_>>(res[0]);
    ASSERT_EQ(toTest1->firstElse_, -1);

    auto& toTest2 = std::get<std::unique_ptr<NodeSupEqual_>>(toTest1->arguments_[0]);
    auto& toTest3 = std::get<std::unique_ptr<NodeMinus_>>(toTest2->arguments_[0]);
    auto& toTest4 = std::get<std::unique_ptr<NodeVar_>>(toTest3->arguments_[0]);
    auto& toTest5 = std::get<std::unique_ptr<NodeConst_>>(toTest3->arguments_[1]);

    auto& toTest6 = std::get<std::unique_ptr<NodeAssign_>>(toTest1->arguments_[1]);
    auto& toTest7 = std::get<std::unique_ptr<NodeVar_>>(toTest6->arguments_[0]);
    auto& toTest8 = std::get<std::unique_ptr<NodePlus_>>(toTest6->arguments_[1]);
    auto& toTest9 = std::get<std::unique_ptr<NodeConst_>>(toTest8->arguments_[0]);
    auto& toTest10 = std::get<std::unique_ptr<NodeVar_>>(toTest8->arguments_[1]);
}

TEST(ParserTest, TestParseIfWithElse) {
    String_ event = R"(
        IF x >= 2 THEN
            y = 3 + x
        ELSE
            y = x
        ENDIF
    )";
    auto res = Parse(event);
    ASSERT_EQ(res.size(), 1);
    auto& toTest1 = std::get<std::unique_ptr<NodeIf_>>(res[0]);
    ASSERT_TRUE(toTest1);
    ASSERT_EQ(toTest1->firstElse_, 2);

    auto& toTest2 = std::get<std::unique_ptr<NodeSupEqual_>>(toTest1->arguments_[0]);
    auto& toTest3 = std::get<std::unique_ptr<NodeMinus_>>(toTest2->arguments_[0]);
    auto& toTest4 = std::get<std::unique_ptr<NodeVar_>>(toTest3->arguments_[0]);
    auto& toTest5 = std::get<std::unique_ptr<NodeConst_>>(toTest3->arguments_[1]);

    auto& toTest6 = std::get<std::unique_ptr<NodeAssign_>>(toTest1->arguments_[1]);
    auto& toTest7 = std::get<std::unique_ptr<NodeVar_>>(toTest6->arguments_[0]);
    auto& toTest8 = std::get<std::unique_ptr<NodePlus_>>(toTest6->arguments_[1]);
    auto& toTest9 = std::get<std::unique_ptr<NodeConst_>>(toTest8->arguments_[0]);
    auto& toTest10 = std::get<std::unique_ptr<NodeVar_>>(toTest8->arguments_[1]);

    auto& toTest11 = std::get<std::unique_ptr<NodeAssign_>>(toTest1->arguments_[2]);
    auto& toTest12 = std::get<std::unique_ptr<NodeVar_>>(toTest11->arguments_[0]);
    auto& toTest13 = std::get<std::unique_ptr<NodeVar_>>(toTest11->arguments_[1]);
}
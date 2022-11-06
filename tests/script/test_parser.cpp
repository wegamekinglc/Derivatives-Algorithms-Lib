//
// Created by wegam on 2022/6/5.
//

#include <gtest/gtest.h>
#include <dal/script/parser.hpp>

using namespace Dal;
using namespace Dal::Script;

TEST(ParserTest, TestParseAssign) {
    String_ event = "x = 2";
    auto res = Parse(event);
    ASSERT_EQ(res.size(), 1);
    auto toTest1 = dynamic_cast<NodeAssign_*>(res[0].get());
    ASSERT_TRUE(toTest1);
    auto toTest2 = dynamic_cast<NodeVar_*>((*res[0]).arguments_[0].get());
    ASSERT_TRUE(toTest2);
    auto toTest3 = dynamic_cast<NodeConst_*>((*res[0]).arguments_[1].get());
    ASSERT_TRUE(toTest3);
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
    auto toTest1 = dynamic_cast<NodeIf_*>(res[0].get());
    ASSERT_TRUE(toTest1);
    ASSERT_EQ(toTest1->firstElse_, -1);

    auto toTest2 = dynamic_cast<NodeSupEqual_*>(toTest1->arguments_[0].get());
    ASSERT_TRUE(toTest2);
    auto toTest3 = dynamic_cast<NodeMinus_*>(toTest2->arguments_[0].get());
    ASSERT_TRUE(toTest3);
    auto toTest4 = dynamic_cast<NodeVar_*>(toTest3->arguments_[0].get());
    ASSERT_TRUE(toTest4);
    auto toTest5 = dynamic_cast<NodeConst_*>(toTest3->arguments_[1].get());
    ASSERT_TRUE(toTest5);

    auto toTest6 = dynamic_cast<NodeAssign_*>(toTest1->arguments_[1].get());
    ASSERT_TRUE(toTest6);
    auto toTest7 = dynamic_cast<NodeVar_*>(toTest6->arguments_[0].get());
    ASSERT_TRUE(toTest7);
    auto toTest8 = dynamic_cast<NodePlus_*>(toTest6->arguments_[1].get());
    ASSERT_TRUE(toTest8);
    auto toTest9 = dynamic_cast<NodeConst_*>(toTest8->arguments_[0].get());
    ASSERT_TRUE(toTest9);
    auto toTest10 = dynamic_cast<NodeVar_*>(toTest8->arguments_[1].get());
    ASSERT_TRUE(toTest10);
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
    auto toTest1 = dynamic_cast<NodeIf_*>(res[0].get());
    ASSERT_TRUE(toTest1);
    ASSERT_EQ(toTest1->firstElse_, 2);

    auto toTest2 = dynamic_cast<NodeSupEqual_*>(toTest1->arguments_[0].get());
    ASSERT_TRUE(toTest2);
    auto toTest3 = dynamic_cast<NodeMinus_*>(toTest2->arguments_[0].get());
    ASSERT_TRUE(toTest3);
    auto toTest4 = dynamic_cast<NodeVar_*>(toTest3->arguments_[0].get());
    ASSERT_TRUE(toTest4);
    auto toTest5 = dynamic_cast<NodeConst_*>(toTest3->arguments_[1].get());
    ASSERT_TRUE(toTest5);

    auto toTest6 = dynamic_cast<NodeAssign_*>(toTest1->arguments_[1].get());
    ASSERT_TRUE(toTest6);
    auto toTest7 = dynamic_cast<NodeVar_*>(toTest6->arguments_[0].get());
    ASSERT_TRUE(toTest7);
    auto toTest8 = dynamic_cast<NodePlus_*>(toTest6->arguments_[1].get());
    ASSERT_TRUE(toTest8);
    auto toTest9 = dynamic_cast<NodeConst_*>(toTest8->arguments_[0].get());
    ASSERT_TRUE(toTest9);
    auto toTest10 = dynamic_cast<NodeVar_*>(toTest8->arguments_[1].get());
    ASSERT_TRUE(toTest10);

    auto toTest11 = dynamic_cast<NodeAssign_*>(toTest1->arguments_[2].get());
    ASSERT_TRUE(toTest11);
    auto toTest12 = dynamic_cast<NodeVar_*>(toTest11->arguments_[0].get());
    ASSERT_TRUE(toTest12);
    auto toTest13 = dynamic_cast<NodeVar_*>(toTest11->arguments_[1].get());
    ASSERT_TRUE(toTest13);
}
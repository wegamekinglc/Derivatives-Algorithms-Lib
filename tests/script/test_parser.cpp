//
// Created by wegam on 2022/6/5.
//

#include <gtest/gtest.h>
#include <dal/script/parser.hpp>
#include <dal/script/node.hpp>
#include <dal/script/visitor/all.hpp>

using namespace Dal;
using namespace Dal::Script;

TEST(ParserTest, TestParseAssign) {
    String_ event = "x = 2";
    auto res = Parse(event);
    ASSERT_EQ(res.size(), 1);
    auto toTest1 = dynamic_cast<NodeAssign*>(res[0].get());
    ASSERT_NE(dynamic_cast<NodeVar*>(toTest1->arguments[0].get()), nullptr);
    auto toTest3 = dynamic_cast<NodeConst*>(toTest1->arguments[1].get());
    ASSERT_NEAR(toTest3->constVal, 2.0, 1e-10);
}

TEST(ParserTest, TestParseIf) {
    String_ event = R"(
        IF x >= 2 THEN
            y = 3 + x
        ENDIF
    )";
    auto res = Parse(event);
    ASSERT_EQ(res.size(), 1);
    auto toTest1 = dynamic_cast<NodeIf*>(res[0].get());
    ASSERT_EQ(toTest1->firstElse, -1);

    auto toTest2 = dynamic_cast<NodeSupEqual*>(toTest1->arguments[0].get());
    auto toTest3 = dynamic_cast<NodeSub*>(toTest2->arguments[0].get());
    ASSERT_NE(dynamic_cast<NodeVar*>(toTest3->arguments[0].get()), nullptr);
    ASSERT_NE(dynamic_cast<NodeConst*>(toTest3->arguments[1].get()), nullptr);

    auto toTest6 = dynamic_cast<NodeAssign*>(toTest1->arguments[1].get());
    ASSERT_NE(dynamic_cast<NodeVar*>(toTest6->arguments[0].get()), nullptr);
    auto toTest8 = dynamic_cast<NodeAdd*>(toTest6->arguments[1].get());
    ASSERT_NE(dynamic_cast<NodeConst*>(toTest8->arguments[0].get()), nullptr);
    ASSERT_NE(dynamic_cast<NodeVar*>(toTest8->arguments[1].get()), nullptr);
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
    auto toTest1 = dynamic_cast<NodeIf*>(res[0].get());
    ASSERT_TRUE(toTest1);
    ASSERT_EQ(toTest1->firstElse, 2);

    auto toTest2 = dynamic_cast<NodeSupEqual*>(toTest1->arguments[0].get());
    auto toTest3 = dynamic_cast<NodeSub*>(toTest2->arguments[0].get());
    ASSERT_NE(dynamic_cast<NodeVar*>(toTest3->arguments[0].get()), nullptr);
    ASSERT_NE(dynamic_cast<NodeConst*>(toTest3->arguments[1].get()), nullptr);

    auto toTest6 = dynamic_cast<NodeAssign*>(toTest1->arguments[1].get());
    ASSERT_NE(dynamic_cast<NodeVar*>(toTest6->arguments[0].get()), nullptr);
    auto toTest8 = dynamic_cast<NodeAdd*>(toTest6->arguments[1].get());
    ASSERT_NE(dynamic_cast<NodeConst*>(toTest8->arguments[0].get()), nullptr);
    ASSERT_NE(dynamic_cast<NodeVar*>(toTest8->arguments[1].get()), nullptr);

    auto toTest11 = dynamic_cast<NodeAssign*>(toTest1->arguments[2].get());
    ASSERT_NE(dynamic_cast<NodeVar*>(toTest11->arguments[0].get()), nullptr);
    ASSERT_NE(dynamic_cast<NodeVar*>(toTest11->arguments[1].get()), nullptr);
}
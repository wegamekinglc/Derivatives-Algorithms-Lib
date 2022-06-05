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
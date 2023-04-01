//
// Created by wegam on 2022/5/22.
//

#include <gtest/gtest.h>
#include <dal/script/visitor/all.hpp>

using namespace Dal;
using namespace Dal::Script;

TEST(VisitorTest, TestDebuggerVisit) {
    Expression_ var1 = MakeBaseNode<NodeVar_>("x");
    Expression_ const1 = MakeBaseNode<NodeConst_>(20);

    Debugger_ visitor;
    var1->Accept(visitor);
    ASSERT_EQ(visitor.String(), String_("VAR[x,-1]\n"));

    const1->Accept(visitor);
    ASSERT_EQ(visitor.String(), String_("CONST[20.000000]\n"));
}
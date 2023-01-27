//
// Created by wegam on 2022/5/22.
//

#include <gtest/gtest.h>
#include <dal/script/visitor/all.hpp>

using namespace Dal;
using namespace Dal::Script;

TEST(DebuggerTest, TestDebuggerVisit) {
    Expression var1 = MakeBaseNode<NodeVar>("x");
    Expression const1 = MakeBaseNode<NodeConst>(20);

    Debugger_ visitor;
    var1->accept(visitor);
    ASSERT_EQ(visitor.String(), String_("VAR[x,-1]\n"));

    const1->accept(visitor);
    ASSERT_EQ(visitor.String(), String_("CONST[20.000000]\n"));
}
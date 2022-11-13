//
// Created by wegam on 2022/5/22.
//

#include <gtest/gtest.h>
#include <dal/script/visitor/debugger.hpp>

using namespace Dal;
using namespace Dal::Script;

TEST(DebuggerTest, TestDebuggerVisit) {
    ScriptNode_ var1 = std::make_unique<NodeVar_>("x");
    ScriptNode_ const1 = std::make_unique<NodeConst_>(20);

    Debugger_ visitor;
    visitor.Visit(var1);
    ASSERT_EQ(visitor.String(), String_("VAR[-1]\n"));

    visitor.Visit(const1);
    ASSERT_EQ(visitor.String(), String_("CONST[20.000000]\n"));
}
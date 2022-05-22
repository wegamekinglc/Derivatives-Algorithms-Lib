//
// Created by wegam on 2022/5/22.
//

#include <gtest/gtest.h>
#include <dal/script/visitor/debugger.hpp>

using namespace Dal;
using namespace Dal::Script;

TEST(DebuggerTest, TestDebuggerVisit) {
    NodeVar_ var1("x");
    NodeVar_ var2("y");
    NodeConst_ const1(20.0);

    std::unique_ptr<Debugger_> visitor = std::make_unique<Debugger_>();
    visitor->Visit(&var1);
    ASSERT_EQ(visitor->String(), String_("VAR[-1]\n"));

    visitor->Visit(&const1);
    ASSERT_EQ(visitor->String(), String_("CONST[20.000000]\n"));
}
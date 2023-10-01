//
// Created by wegam on 2022/5/22.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/script/visitor/all.hpp>
#include <dal/script/event.hpp>

using namespace Dal;
using namespace Dal::Script;

TEST(ScriptTest, TestDebuggerVisit) {
    Expression_ var1 = MakeBaseNode<NodeVar_>("x");
    Expression_ const1 = MakeBaseNode<NodeConst_>(20);

    Debugger_ visitor;
    var1->Accept(visitor);
    ASSERT_EQ(visitor.String(), String_("VAR[x,-1,0.000000]\n"));

    const1->Accept(visitor);
    ASSERT_EQ(visitor.String(), String_("CONST[20.000000]\n"));
}

TEST(ScriptTest, TestDebuggerVisitWithEmptyEvent) {
    Vector_<Cell_> dates;
    Vector_<String_> events;
    ScriptProduct_ product(dates, events);

    std::ostringstream out;
    product.Debug(out);
    String_ desc(out.str());
    Vector_<String_> rtn = String::Split(desc, '\n', true);
    ASSERT_EQ(rtn.size(), 0);
}
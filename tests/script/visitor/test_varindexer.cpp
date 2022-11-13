//
// Created by wegam on 2022/5/21.
//

#include <gtest/gtest.h>
#include <dal/script/visitor/varindexer.hpp>

using namespace Dal;
using namespace Dal::Script;

TEST(VarIndexerTest, TestVarIndexerVisit) {
    ScriptNode_ var1 = std::make_unique<NodeVar_>("x");
    ScriptNode_ var2 = std::make_unique<NodeVar_>("y");
    ScriptNode_ const1 = std::make_unique<NodeConst_>(20);

    VarIndexer_ visitor;
    visitor.Visit(var1);
    visitor.Visit(const1);
    visitor.Visit(var2);

    ASSERT_EQ(std::get<std::unique_ptr<NodeVar_>>(var1)->index_, 0);
    ASSERT_EQ(std::get<std::unique_ptr<NodeVar_>>(var2)->index_, 1);

    Vector_<String_> names = visitor.GetVarNames();
    ASSERT_EQ(names[0], "x");
    ASSERT_EQ(names[1], "y");
}
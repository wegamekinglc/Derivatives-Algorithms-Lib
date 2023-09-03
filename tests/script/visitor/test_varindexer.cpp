//
// Created by wegam on 2022/5/21.
//

#include <gtest/gtest.h>
#include <dal/script/visitor/all.hpp>

using namespace Dal;
using namespace Dal::Script;

TEST(ScriptTest, TestVarIndexerVisit) {
    Expression_ var1 = MakeBaseNode<NodeVar_>("x");
    Expression_ var2 = MakeBaseNode<NodeVar_>("y");
    Expression_ const1 = MakeBaseNode<NodeConst_>(20);

    VarIndexer_ visitor;
    var1->Accept(visitor);
    const1->Accept(visitor);
    var2->Accept(visitor);

    ASSERT_EQ(dynamic_cast<NodeVar_*>(var1.get())->index_, 0);
    ASSERT_EQ(dynamic_cast<NodeVar_*>(var2.get())->index_, 1);

    Vector_<String_> names = visitor.VarNames();
    ASSERT_EQ(names[0], "x");
    ASSERT_EQ(names[1], "y");
}
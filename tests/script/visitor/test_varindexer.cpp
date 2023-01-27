//
// Created by wegam on 2022/5/21.
//

#include <gtest/gtest.h>
#include <dal/script/visitor/all.hpp>

using namespace Dal;
using namespace Dal::Script;

TEST(VarIndexerTest, TestVarIndexerVisit) {
    Expression var1 = MakeBaseNode<NodeVar>("x");
    Expression var2 = MakeBaseNode<NodeVar>("y");
    Expression const1 = MakeBaseNode<NodeConst>(20);

    VarIndexer_ visitor;
    var1->accept(visitor);
    const1->accept(visitor);
    var2->accept(visitor);

    ASSERT_EQ(dynamic_cast<NodeVar*>(var1.get())->index, 0);
    ASSERT_EQ(dynamic_cast<NodeVar*>(var2.get())->index, 1);

    Vector_<String_> names = visitor.getVarNames();
    ASSERT_EQ(names[0], "x");
    ASSERT_EQ(names[1], "y");
}
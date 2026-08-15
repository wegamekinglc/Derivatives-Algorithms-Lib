//
// Created by wegam on 2022/5/21.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
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

TEST(VarIndexerTest, TestRevisitingVarKeepsIndex) {
    Expression_ first = MakeBaseNode<NodeVar_>("x");
    Expression_ second = MakeBaseNode<NodeVar_>("x");

    VarIndexer_ visitor;
    first->Accept(visitor);
    second->Accept(visitor);

    ASSERT_EQ(dynamic_cast<NodeVar_*>(first.get())->index_, 0);
    ASSERT_EQ(dynamic_cast<NodeVar_*>(second.get())->index_, 0);
    ASSERT_EQ(visitor.VarNames().size(), 1u);
}

TEST(VarIndexerTest, TestConstVarIndexing) {
    Expression_ constVar = MakeBaseNode<NodeConstVar_>("STRIKE", 11.0);

    VarIndexer_ visitor;
    constVar->Accept(visitor);

    ASSERT_EQ(dynamic_cast<NodeConstVar_*>(constVar.get())->index_, 0);
    ASSERT_EQ(visitor.VarNames().size(), 0u);

    const Vector_<String_> names = visitor.ConstVarNames();
    ASSERT_EQ(names.size(), 1u);
    ASSERT_EQ(names[0], "STRIKE");

    const Vector_<double> values = visitor.ConstVarValues();
    ASSERT_EQ(values.size(), 1u);
    ASSERT_DOUBLE_EQ(values[0], 11.0);
}

TEST(VarIndexerTest, TestRevisitingConstVarKeepsIndexAndValue) {
    Expression_ first = MakeBaseNode<NodeConstVar_>("STRIKE", 11.0);
    Expression_ second = MakeBaseNode<NodeConstVar_>("STRIKE", 11.0);

    VarIndexer_ visitor;
    first->Accept(visitor);
    second->Accept(visitor);

    ASSERT_EQ(dynamic_cast<NodeConstVar_*>(first.get())->index_, 0);
    ASSERT_EQ(dynamic_cast<NodeConstVar_*>(second.get())->index_, 0);
    ASSERT_EQ(visitor.ConstVarNames().size(), 1u);
    ASSERT_EQ(visitor.ConstVarValues().size(), 1u);
}

TEST(VarIndexerTest, TestVarsAndConstVarsHaveSeparateIndexSpaces) {
    Expression_ var1 = MakeBaseNode<NodeVar_>("x");
    Expression_ constVar = MakeBaseNode<NodeConstVar_>("STRIKE", 11.0);
    Expression_ var2 = MakeBaseNode<NodeVar_>("y");

    VarIndexer_ visitor;
    var1->Accept(visitor);
    constVar->Accept(visitor);
    var2->Accept(visitor);

    ASSERT_EQ(dynamic_cast<NodeVar_*>(var1.get())->index_, 0);
    ASSERT_EQ(dynamic_cast<NodeVar_*>(var2.get())->index_, 1);
    ASSERT_EQ(dynamic_cast<NodeConstVar_*>(constVar.get())->index_, 0);

    ASSERT_EQ(visitor.VarNames().size(), 2u);
    ASSERT_EQ(visitor.ConstVarNames().size(), 1u);
}

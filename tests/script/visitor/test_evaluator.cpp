//
// Created by wegam on 2022/6/3.
//

#include <gtest/gtest.h>
#include <dal/script/visitor/varindexer.hpp>
#include <dal/script/visitor/evaluator.hpp>

using namespace Dal;
using namespace Dal::Script;

TEST(EvaluatorTest, TestEvaluator) {
    ScriptNode_ const1 = MakeBaseNode<NodeConst_>(20.0);
    ScriptNode_ const2 = MakeBaseNode<NodeConst_>(30.0);

    auto plusExpr = BuildBinary<NodePlus_>(const1, const2);

    ScriptNode_ var = MakeBaseNode<NodeVar_>("x");
    auto assignExpr = BuildBinary<NodeAssign_>(var, plusExpr);

    VarIndexer_ visitor1;
    Evaluator_<double> visitor2(1);
    visitor1.Visit(assignExpr);
    visitor2.Visit(assignExpr);
    ASSERT_DOUBLE_EQ(visitor2.VarVals()[0], 50);
}
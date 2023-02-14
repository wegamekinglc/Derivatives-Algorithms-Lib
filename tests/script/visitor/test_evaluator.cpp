//
// Created by wegam on 2022/6/3.
//

#include <gtest/gtest.h>
#include <dal/script/node.hpp>
#include <dal/script/visitor/all.hpp>
#include <dal/script/visitor/evaluator.hpp>

using namespace Dal;
using namespace Dal::Script;

TEST(EvaluatorTest, TestEvaluator) {
    Expression_ const1 = MakeBaseNode<NodeConst_>(20.0);
    Expression_ const2 = MakeBaseNode<NodeConst_>(30.0);

    auto plusExpr = MakeBaseBinary<NodeAdd_>(const1, const2);

    Expression_ var = MakeBaseNode<NodeVar_>("x");
    auto assignExpr = MakeBinary<NodeAssign_>(var, plusExpr);

    VarIndexer_ visitor1;
    Evaluator_<double> visitor2(1);
    assignExpr->Accept(visitor1);
    assignExpr->Accept(visitor2);
    ASSERT_DOUBLE_EQ(visitor2.VarVals()[0], 50);
}
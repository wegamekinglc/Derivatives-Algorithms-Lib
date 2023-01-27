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
    Expression const1 = MakeBaseNode<NodeConst>(20.0);
    Expression const2 = MakeBaseNode<NodeConst>(30.0);

    auto plusExpr = MakeBaseBinary<NodeAdd>(const1, const2);

    Expression var = MakeBaseNode<NodeVar>("x");
    auto assignExpr = MakeBinary<NodeAssign>(var, plusExpr);

    VarIndexer_ visitor1;
    Evaluator_<double> visitor2(1);
    assignExpr->accept(visitor1);
    assignExpr->accept(visitor2);
    ASSERT_DOUBLE_EQ(visitor2.VarVals()[0], 50);
}
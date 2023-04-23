//
// Created by wegam on 2022/6/3.
//

#include <gtest/gtest.h>
#include <dal/script/node.hpp>
#include <dal/script/visitor/all.hpp>
#include <dal/script/visitor/evaluator.hpp>

using namespace Dal;
using namespace Dal::Script;

TEST(VisitorTest, TestEvaluator) {
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

TEST(VisitorTest, TestEvaluatorWithSqrt) {
    auto const1 = MakeBaseNode<NodeConst_>(2.0);
    auto expExpr = MakeBaseNode<NodeSqrt_>();
    Vector_<Expression_> args;
    args.push_back(std::move(const1));
    expExpr->arguments_ = std::move(args);
    auto var = MakeBaseNode<NodeVar_>("x");
    auto assignExpr = MakeBinary<NodeAssign_>(var, expExpr);

    Evaluator_<double> visitor2(1);
    VarIndexer_ visitor1;
    assignExpr->Accept(visitor1);
    assignExpr->Accept(visitor2);
    ASSERT_DOUBLE_EQ(visitor2.VarVals()[0], 1.4142135623730951);
}

TEST(VisitorTest, TestEvaluatorWithLog) {
    auto const1 = MakeBaseNode<NodeConst_>(2.0);
    auto expExpr = MakeBaseNode<NodeLog_>();
    Vector_<Expression_> args;
    args.push_back(std::move(const1));
    expExpr->arguments_ = std::move(args);
    auto var = MakeBaseNode<NodeVar_>("x");
    auto assignExpr = MakeBinary<NodeAssign_>(var, expExpr);

    Evaluator_<double> visitor2(1);
    VarIndexer_ visitor1;
    assignExpr->Accept(visitor1);
    assignExpr->Accept(visitor2);
    ASSERT_DOUBLE_EQ(visitor2.VarVals()[0], 0.69314718055994529);
}

TEST(VisitorTest, TestEvaluatorWithExp) {
    auto const1 = MakeBaseNode<NodeConst_>(2.0);
    auto expExpr = MakeBaseNode<NodeExp_>();
    Vector_<Expression_> args;
    args.push_back(std::move(const1));
    expExpr->arguments_ = std::move(args);
    auto var = MakeBaseNode<NodeVar_>("x");
    auto assignExpr = MakeBinary<NodeAssign_>(var, expExpr);

    Evaluator_<double> visitor2(1);
    VarIndexer_ visitor1;
    assignExpr->Accept(visitor1);
    assignExpr->Accept(visitor2);
    ASSERT_DOUBLE_EQ(visitor2.VarVals()[0], 7.3890560989306504);
}
//
// Created by wegam on 2023/8/20.
//

#include <gtest/gtest.h>
#include <dal/script/node.hpp>
#include <dal/script/visitor/all.hpp>
#include <dal/script/visitor/pastevaluator.hpp>

using namespace Dal;
using namespace Dal::Script;


TEST(VisitorTest, TestPastEvaluator) {
    auto const1 = MakeBaseNode<NodeConst_>(2.0);
    auto expExpr = MakeBaseNode<NodeSqrt_>();
    Vector_<Expression_> args;
    args.push_back(std::move(const1));
    expExpr->arguments_ = std::move(args);
    auto var = MakeBaseNode<NodeVar_>("x");
    auto assignExpr = MakeBinary<NodeAssign_>(var, expExpr);
    auto var2 = MakeBaseNode<NodeVar_>("y");
    var = MakeBaseNode<NodeVar_>("x");
    auto paysExpr = MakeBinary<NodePays_>(var2, var);

    VarIndexer_ visitor1;
    assignExpr->Accept(visitor1);
    paysExpr->Accept(visitor1);

    PastEvaluator_ visitor2(visitor1.VarNames().size());
    assignExpr->Accept(visitor2);
    paysExpr->Accept(visitor2);
    ASSERT_DOUBLE_EQ(visitor2.VarVals()[0], 1.4142135623730951);
    ASSERT_DOUBLE_EQ(visitor2.VarVals()[1], 0.0);
}
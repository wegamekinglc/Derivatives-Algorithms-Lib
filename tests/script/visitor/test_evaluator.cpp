//
// Created by wegam on 2022/6/3.
//

#include <gtest/gtest.h>
#include <dal/script/visitor/evaluator.hpp>

using namespace Dal;
using namespace Dal::Script;

TEST(EvaluatorTest, TestEvaluator) {
    std::unique_ptr<Node_> const1 = MakeNode<NodeConst_>(20.0);
    std::unique_ptr<Node_> const2 = MakeNode<NodeConst_>(30.0);

    auto plusExpr = BuildBinary<NodePlus_>(const1, const2);
    std::unique_ptr<ConstVisitor_> visitor = std::make_unique<Evaluator_<double>>(0);
    visitor->Visit(plusExpr);

    ASSERT_EQ(visitor->)
}
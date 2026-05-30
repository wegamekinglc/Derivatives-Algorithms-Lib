//
// Created by wegam on 2026/05/30.
//

#include <gtest/gtest.h>
#include <cmath>
#include <dal/platform/platform.hpp>
#include <dal/script/visitor/all.hpp>
#include <dal/script/parser.hpp>
#include <dal/script/node.hpp>

using namespace Dal;
using namespace Dal::Script;

namespace {
    // Parse, index variables, then run the constant processor over every statement.
    Event_ ParseAndConstProcess(const String_& event) {
        Parser_ parser;
        Event_ res = parser.Parse(event);

        VarIndexer_ indexer;
        for (auto& stat : res)
            stat->Accept(indexer);

        ConstProcessor_ proc(indexer.VarNames().size());
        for (auto& stat : res)
            stat->Accept(proc);

        return res;
    }

    // RHS expression of an assignment statement.
    const ExprNode_* AssignRhs(const Statement_& stat) {
        return dynamic_cast<const ExprNode_*>(stat->arguments_[1].get());
    }
} // namespace

TEST(ScriptTest, TestConstProcessorFoldAddition) {
    auto res = ParseAndConstProcess("x = 2 + 3");
    const ExprNode_* rhs = AssignRhs(res[0]);
    ASSERT_NE(dynamic_cast<const NodeAdd_*>(res[0]->arguments_[1].get()), nullptr);
    ASSERT_TRUE(rhs->isConst_);
    ASSERT_NEAR(rhs->constVal_, 5.0, 1e-10);
}

TEST(ScriptTest, TestConstProcessorFoldSubtraction) {
    auto res = ParseAndConstProcess("x = 7 - 4");
    const ExprNode_* rhs = AssignRhs(res[0]);
    ASSERT_TRUE(rhs->isConst_);
    ASSERT_NEAR(rhs->constVal_, 3.0, 1e-10);
}

TEST(ScriptTest, TestConstProcessorFoldMultiplication) {
    auto res = ParseAndConstProcess("x = 3 * 4");
    const ExprNode_* rhs = AssignRhs(res[0]);
    ASSERT_TRUE(rhs->isConst_);
    ASSERT_NEAR(rhs->constVal_, 12.0, 1e-10);
}

TEST(ScriptTest, TestConstProcessorFoldDivision) {
    auto res = ParseAndConstProcess("x = 6 / 2");
    const ExprNode_* rhs = AssignRhs(res[0]);
    ASSERT_TRUE(rhs->isConst_);
    ASSERT_NEAR(rhs->constVal_, 3.0, 1e-10);
}

TEST(ScriptTest, TestConstProcessorFoldPowerOperator) {
    auto res = ParseAndConstProcess("x = 2 ^ 3");
    const ExprNode_* rhs = AssignRhs(res[0]);
    ASSERT_NE(dynamic_cast<const NodePow_*>(res[0]->arguments_[1].get()), nullptr);
    ASSERT_TRUE(rhs->isConst_);
    ASSERT_NEAR(rhs->constVal_, 8.0, 1e-10);
}

TEST(ScriptTest, TestConstProcessorFoldMaxMin) {
    {
        auto res = ParseAndConstProcess("x = MAX(2, 3)");
        const ExprNode_* rhs = AssignRhs(res[0]);
        ASSERT_NE(dynamic_cast<const NodeMax_*>(res[0]->arguments_[1].get()), nullptr);
        ASSERT_TRUE(rhs->isConst_);
        ASSERT_NEAR(rhs->constVal_, 3.0, 1e-10);
    }
    {
        auto res = ParseAndConstProcess("x = MIN(2, 3)");
        const ExprNode_* rhs = AssignRhs(res[0]);
        ASSERT_NE(dynamic_cast<const NodeMin_*>(res[0]->arguments_[1].get()), nullptr);
        ASSERT_TRUE(rhs->isConst_);
        ASSERT_NEAR(rhs->constVal_, 2.0, 1e-10);
    }
}

TEST(ScriptTest, TestConstProcessorFoldFunctions) {
    {
        auto res = ParseAndConstProcess("x = SQRT(4)");
        const ExprNode_* rhs = AssignRhs(res[0]);
        ASSERT_TRUE(rhs->isConst_);
        ASSERT_NEAR(rhs->constVal_, 2.0, 1e-10);
    }
    {
        auto res = ParseAndConstProcess("x = EXP(0)");
        const ExprNode_* rhs = AssignRhs(res[0]);
        ASSERT_TRUE(rhs->isConst_);
        ASSERT_NEAR(rhs->constVal_, 1.0, 1e-10);
    }
    {
        auto res = ParseAndConstProcess("x = LOG(1)");
        const ExprNode_* rhs = AssignRhs(res[0]);
        ASSERT_TRUE(rhs->isConst_);
        ASSERT_NEAR(rhs->constVal_, 0.0, 1e-10);
    }
}

TEST(ScriptTest, TestConstProcessorFoldUnaryMinus) {
    auto res = ParseAndConstProcess("x = -5");
    const ExprNode_* rhs = AssignRhs(res[0]);
    ASSERT_NE(dynamic_cast<const NodeUMinus_*>(res[0]->arguments_[1].get()), nullptr);
    ASSERT_TRUE(rhs->isConst_);
    ASSERT_NEAR(rhs->constVal_, -5.0, 1e-10);
}

TEST(ScriptTest, TestConstProcessorFoldNestedExpression) {
    auto res = ParseAndConstProcess("x = (2 + 3) * 4 - 1");
    const ExprNode_* rhs = AssignRhs(res[0]);
    ASSERT_TRUE(rhs->isConst_);
    ASSERT_NEAR(rhs->constVal_, 19.0, 1e-10);
}

TEST(ScriptTest, TestConstProcessorPropagatesConstVariable) {
    String_ event = R"(
        y = 2
        x = y + 1
    )";
    auto res = ParseAndConstProcess(event);

    const auto* add = dynamic_cast<const NodeAdd_*>(res[1]->arguments_[1].get());
    ASSERT_NE(add, nullptr);
    ASSERT_TRUE(add->isConst_);
    ASSERT_NEAR(add->constVal_, 3.0, 1e-10);

    const auto* var = dynamic_cast<const NodeVar_*>(add->arguments_[0].get());
    ASSERT_NE(var, nullptr);
    ASSERT_TRUE(var->isConst_);
    ASSERT_NEAR(var->constVal_, 2.0, 1e-10);
}

TEST(ScriptTest, TestConstProcessorSpotNotConst) {
    auto res = ParseAndConstProcess("x = spot() + 1");
    const ExprNode_* rhs = AssignRhs(res[0]);
    ASSERT_NE(dynamic_cast<const NodeAdd_*>(res[0]->arguments_[1].get()), nullptr);
    ASSERT_FALSE(rhs->isConst_);
}

TEST(ScriptTest, TestConstProcessorMixedConstNonConst) {
    String_ event = R"(
        x = spot()
        y = x + 2
    )";
    auto res = ParseAndConstProcess(event);

    const auto* add = dynamic_cast<const NodeAdd_*>(res[1]->arguments_[1].get());
    ASSERT_NE(add, nullptr);
    ASSERT_FALSE(add->isConst_);

    const auto* var = dynamic_cast<const NodeVar_*>(add->arguments_[0].get());
    ASSERT_NE(var, nullptr);
    ASSERT_FALSE(var->isConst_);
}

TEST(ScriptTest, TestConstProcessorReassignmentClearsConst) {
    String_ event = R"(
        x = 2
        x = spot()
        y = x
    )";
    auto res = ParseAndConstProcess(event);

    const auto* var = dynamic_cast<const NodeVar_*>(res[2]->arguments_[1].get());
    ASSERT_NE(var, nullptr);
    ASSERT_FALSE(var->isConst_);
}

TEST(ScriptTest, TestConstProcessorConditionalAssignmentNotConst) {
    String_ event = R"(
        IF spot() >= 1 THEN
            x = 2
        END
        y = x
    )";
    auto res = ParseAndConstProcess(event);

    const auto* var = dynamic_cast<const NodeVar_*>(res[1]->arguments_[1].get());
    ASSERT_NE(var, nullptr);
    ASSERT_FALSE(var->isConst_);
}

TEST(ScriptTest, TestConstProcessorUnconditionalAssignmentStaysConst) {
    String_ event = R"(
        x = 2
        y = x
    )";
    auto res = ParseAndConstProcess(event);

    const auto* var = dynamic_cast<const NodeVar_*>(res[1]->arguments_[1].get());
    ASSERT_NE(var, nullptr);
    ASSERT_TRUE(var->isConst_);
    ASSERT_NEAR(var->constVal_, 2.0, 1e-10);
}

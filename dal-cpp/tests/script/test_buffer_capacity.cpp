//
// Created by Codex on 2026/10/06.
//

#include <gtest/gtest.h>

#include <dal/math/buffercapacity.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/visitor/evalstate.hpp>

using namespace Dal;

TEST(BufferEvaluatorCapacityTest, TestInlineStateAndDynamicArraysFitOneAdmittedPayload) {
    const Vector_<> variables{1.0, 2.0, 3.0};
    const Vector_<> constants{4.0};
    const auto fixedBytes = sizeof(Script::EvalStateCore_<double>);
    const auto arrayBytes = (2 * variables.size() + constants.size()) * sizeof(double);
    BufferCapacityBudget_ budget(fixedBytes + arrayBytes);
    {
        BufferCapacityScope_ scope(&budget, fixedBytes);
        ASSERT_EQ(budget.CapacityBytes(), fixedBytes);
        Script::EvalStateCore_<double> evaluator(variables, constants);
        ASSERT_EQ(budget.CapacityBytes(), fixedBytes + arrayBytes);
        ASSERT_THROW(evaluator.variables_.reserve(4), Exception_);
        evaluator.Init();
        ASSERT_DOUBLE_EQ(evaluator.VarVals()[2], 3.0);
        ASSERT_DOUBLE_EQ(evaluator.ConstVarVals()[0], 4.0);
        ASSERT_EQ(budget.CapacityBytes(), fixedBytes + arrayBytes);
    }
    ASSERT_EQ(budget.CapacityBytes(), 0);
    BufferCapacityBudget_ insufficient(fixedBytes - 1);
    ASSERT_THROW(BufferCapacityScope_ scope(&insufficient, fixedBytes), Exception_);
    ASSERT_EQ(insufficient.CapacityBytes(), 0);
}

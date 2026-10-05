//
// Created by Codex on 2026-10-06.
//

#include <gtest/gtest.h>

#include <limits>

#include <dal/script/weightedrisk.hpp>

using namespace Dal;
using namespace Dal::Script;

namespace {
    ScriptProduct_ WeightedProduct(const String_& payoff = "b") {
        ScriptProduct_ product({Cell_(Date_(2027, 1, 1))}, {"a = 1 b = 2 pay = a + b"}, payoff);
        product.IndexVariables();
        return product;
    }

    Vector_<RiskCoordinate_> WeightedInputs() {
        return {{"model:0", "spot", "model", 0, 2.0, "model-coordinate", std::nullopt, 1.0},
                {"model:1", "vol", "model", 1, 0.2, "model-coordinate", std::nullopt, 1.0},
                {"constant:0", "strike", "constant", 0, 3.0, "script-number", std::nullopt, 1.0}};
    }
} // namespace

TEST(WeightedRiskPlanTest, TestOutputIdentitySelectionAndExactBudget) {
    const auto product = WeightedProduct();
    const auto axis = ScriptRiskOutputAxis(product);
    ASSERT_EQ(axis.size(), 3);
    ASSERT_EQ(axis[0].id_, "output:0");
    ASSERT_EQ(axis[1].id_, "payoff");
    ASSERT_EQ(axis[2].id_, "output:2");
    WeightedRiskRequest_ request;
    request.selection_.outputs_ = Vector_<String_>{"OUTPUT:2", "output:0"};
    request.selection_.inputs_ = Vector_<String_>{"constant:0", "MODEL:1"};
    request.selection_.reportFactors_ = Vector_<>{0.5, 0.01};
    request.weights_ = Vector_<>{2.0, -1.0};
    request.selection_.numericPayloadBudgetBytes_ = 7 * sizeof(double);
    const auto plan = PlanWeightedRiskRequest(product, WeightedInputs(), Date_(2026, 1, 1), request, true);
    ASSERT_EQ(plan.OutputAxis()[0].id_, "output:2");
    ASSERT_EQ(plan.OutputAxis()[0].label_, "pay");
    ASSERT_EQ(plan.OutputAxis()[0].slot_, 2);
    ASSERT_EQ(plan.OutputAxis()[1].id_, "output:0");
    ASSERT_EQ(plan.Weights(), Vector_<>({2.0, -1.0}));
    ASSERT_EQ(plan.NumericPayloadBytes(), 7 * sizeof(double));
    ASSERT_EQ(plan.CompleteOutputAxis().size(), 3);
    ASSERT_EQ(plan.CompleteInputAxis().size(), 3);
    ASSERT_EQ(*plan.InputRequest().inputs_, Vector_<String_>({"constant:0", "MODEL:1"}));
    ASSERT_EQ(*plan.InputRequest().reportFactors_, Vector_<>({0.5, 0.01}));
    ASSERT_TRUE(plan.EnableAad());
    ASSERT_EQ(plan.EvaluationDate(), Date_(2026, 1, 1));
    request.weights_->front() = 99.0;
    request.selection_.outputs_->clear();
    ASSERT_DOUBLE_EQ(plan.Weights()[0], 2.0);
    ASSERT_EQ(plan.OutputAxis().size(), 2);
}

TEST(WeightedRiskPlanTest, TestOmittedOutputsAndWeightsSelectUnitPayoff) {
    const auto product = WeightedProduct();
    const auto plan = PlanWeightedRiskRequest(product, WeightedInputs(), Date_(2026, 1, 1), {}, true);
    ASSERT_EQ(plan.OutputAxis().size(), 1);
    ASSERT_EQ(plan.OutputAxis()[0].id_, "payoff");
    ASSERT_EQ(plan.OutputAxis()[0].label_, "b");
    ASSERT_EQ(plan.OutputAxis()[0].slot_, 1);
    ASSERT_EQ(plan.Weights(), Vector_<>({1.0}));
    ASSERT_FALSE(plan.InputRequest().inputs_);
    ASSERT_FALSE(plan.InputRequest().outputs_);
    ASSERT_EQ(plan.NumericPayloadBytes(), 6 * sizeof(double));
    WeightedRiskRequest_ request;
    request.selection_.outputs_ = Vector_<String_>{"output:0", "payoff", "output:2"};
    const auto multiple = PlanWeightedRiskRequest(product, WeightedInputs(), Date_(2026, 1, 1), request, true);
    ASSERT_EQ(multiple.Weights(), Vector_<>({1.0, 1.0, 1.0}));
}

TEST(WeightedRiskPlanTest, TestSignedAndZeroWeightsRemainPassiveCopies) {
    const auto product = WeightedProduct();
    auto inputs = WeightedInputs();
    WeightedRiskRequest_ request;
    request.selection_.outputs_ = Vector_<String_>{"output:0", "payoff", "output:2"};
    request.weights_ = Vector_<>{0.0, -2.0, 0.5};
    const auto plan = PlanWeightedRiskRequest(product, inputs, Date_(2026, 1, 1), request, true);
    ASSERT_EQ(plan.Weights(), Vector_<>({0.0, -2.0, 0.5}));
    ASSERT_EQ(plan.CompleteInputAxis().size(), inputs.size());
    inputs[0].value_ = 999.0;
    request.weights_->clear();
    ASSERT_DOUBLE_EQ(plan.CompleteInputAxis()[0].value_, 2.0);
    ASSERT_EQ(plan.Weights().size(), 3);
}

TEST(WeightedRiskPlanTest, TestOutputSelectionErrorsUseCanonicalIds) {
    const auto product = WeightedProduct();
    const Vector_<Vector_<String_>> invalid = {{}, {"unknown"}, {"b"}, {"output:1"}, {"vector:0"}, {"payoff", "PAYOFF"}, {"output:0", "OUTPUT:0"}};
    for (const auto& outputs : invalid) {
        WeightedRiskRequest_ request;
        request.selection_.outputs_ = outputs;
        ASSERT_THROW(static_cast<void>(PlanWeightedRiskRequest(product, WeightedInputs(), Date_(2026, 1, 1), request, true)), ScriptError_);
    }
}

TEST(WeightedRiskPlanTest, TestWeightDimensionAndNonfiniteRejection) {
    const auto product = WeightedProduct();
    const Vector_<Vector_<double>> invalid = {{}, {1.0, 2.0}, {std::numeric_limits<double>::infinity()}, {std::numeric_limits<double>::quiet_NaN()}};
    for (const auto& weights : invalid) {
        WeightedRiskRequest_ request;
        request.weights_ = weights;
        ASSERT_THROW(static_cast<void>(PlanWeightedRiskRequest(product, WeightedInputs(), Date_(2026, 1, 1), request, true)), ScriptError_);
    }
}

TEST(WeightedRiskPlanTest, TestBudgetExactBoundaryAndOverflow) {
    const auto product = WeightedProduct();
    WeightedRiskRequest_ request;
    request.selection_.numericPayloadBudgetBytes_ = 6 * sizeof(double);
    ASSERT_NO_THROW(static_cast<void>(PlanWeightedRiskRequest(product, WeightedInputs(), Date_(2026, 1, 1), request, true)));
    request.selection_.numericPayloadBudgetBytes_ = 6 * sizeof(double) - 1;
    ASSERT_THROW(static_cast<void>(PlanWeightedRiskRequest(product, WeightedInputs(), Date_(2026, 1, 1), request, true)), ScriptError_);
    ASSERT_EQ(WeightedRiskResultPayloadBytes(1, 0), 3 * sizeof(double));
    ASSERT_EQ(WeightedRiskResultPayloadBytes(2, 3), 8 * sizeof(double));
    const size_t maximum = std::numeric_limits<size_t>::max();
    ASSERT_THROW(static_cast<void>(WeightedRiskResultPayloadBytes(0, 0)), ScriptError_);
    ASSERT_THROW(static_cast<void>(WeightedRiskResultPayloadBytes(maximum, 0)), ScriptError_);
    ASSERT_THROW(static_cast<void>(WeightedRiskResultPayloadBytes(1, maximum)), ScriptError_);
    const size_t elements = maximum / sizeof(double);
    const size_t components = (elements - 1) / 2;
    ASSERT_EQ(WeightedRiskResultPayloadBytes(components, 0), (1 + 2 * components) * sizeof(double));
    ASSERT_THROW(static_cast<void>(WeightedRiskResultPayloadBytes(components + 1, 0)), ScriptError_);
}

TEST(WeightedRiskPlanTest, TestNativeEmptyInputsAndPriceOnlyKeepDistinctMethods) {
    const auto product = WeightedProduct();
    WeightedRiskRequest_ request;
    request.selection_.inputs_ = Vector_<String_>{};
    request.selection_.numericPayloadBudgetBytes_ = 3 * sizeof(double);
    const auto native = PlanWeightedRiskRequest(product, WeightedInputs(), Date_(2026, 1, 1), request, true);
    ASSERT_TRUE(native.EnableAad());
    ASSERT_TRUE(native.InputRequest().inputs_->empty());
    ASSERT_EQ(native.NumericPayloadBytes(), 3 * sizeof(double));
    const auto passive = PlanWeightedRiskRequest(product, WeightedInputs(), Date_(2026, 1, 1), {}, false);
    ASSERT_FALSE(passive.EnableAad());
    ASSERT_TRUE(passive.InputRequest().inputs_->empty());
    ASSERT_EQ(passive.NumericPayloadBytes(), native.NumericPayloadBytes());
    request.selection_.inputs_ = Vector_<String_>{"model:0"};
    ASSERT_THROW(static_cast<void>(PlanWeightedRiskRequest(product, WeightedInputs(), Date_(2026, 1, 1), request, false)), ScriptError_);
}

TEST(WeightedRiskPlanTest, TestSharedInputAndReportValidation) {
    const auto product = WeightedProduct();
    for (const Vector_<String_>& inputs : {Vector_<String_>{"unknown"}, Vector_<String_>{"model:0", "MODEL:0"}}) {
        WeightedRiskRequest_ request;
        request.selection_.inputs_ = inputs;
        ASSERT_THROW(static_cast<void>(PlanWeightedRiskRequest(product, WeightedInputs(), Date_(2026, 1, 1), request, true)), ScriptError_);
    }
    for (const double invalid : {0.0, -1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        WeightedRiskRequest_ request;
        request.selection_.inputs_ = Vector_<String_>{"model:0"};
        request.selection_.reportFactors_ = Vector_<>{invalid};
        ASSERT_THROW(static_cast<void>(PlanWeightedRiskRequest(product, WeightedInputs(), Date_(2026, 1, 1), request, true)), ScriptError_);
    }
    auto invalidAxis = WeightedInputs();
    invalidAxis[0].value_ = std::numeric_limits<double>::quiet_NaN();
    ASSERT_THROW(static_cast<void>(PlanWeightedRiskRequest(product, invalidAxis, Date_(2026, 1, 1), {}, true)), ScriptError_);
}

TEST(WeightedRiskPlanTest, TestUnsupportedProductsRejectBeforePreparation) {
    const auto product = WeightedProduct();
    ASSERT_THROW(static_cast<void>(PlanWeightedRiskRequest(product, WeightedInputs(), Date_(), {}, true)), ScriptError_);
    ASSERT_THROW(static_cast<void>(PlanWeightedRiskRequest(product, WeightedInputs(), Date_(2028, 1, 1), {}, true)), ScriptError_);
    ScriptProduct_ unindexed({Cell_(Date_(2027, 1, 1))}, {"a = 1"});
    ASSERT_THROW(static_cast<void>(ScriptRiskOutputAxis(unindexed)), ScriptError_);
    ScriptProduct_ exercise({Cell_(Date_(2027, 1, 1))}, {"EXERCISE 1"});
    exercise.IndexVariables();
    ASSERT_THROW(static_cast<void>(PlanWeightedRiskRequest(exercise, WeightedInputs(), Date_(2026, 1, 1), {}, true)), ScriptError_);
    ScriptProduct_ vectorOnly({Cell_(Date_(2027, 1, 1))}, {"APPEND(v, 2)"});
    vectorOnly.IndexVariables();
    ASSERT_THROW(static_cast<void>(ScriptRiskOutputAxis(vectorOnly)), ScriptError_);
    ScriptProduct_ vectorScalar({Cell_(Date_(2027, 1, 1))}, {"APPEND(v, 2) pay = SUM(v)"});
    vectorScalar.IndexVariables();
    const auto axis = ScriptRiskOutputAxis(vectorScalar);
    ASSERT_EQ(axis.size(), 1);
    ASSERT_EQ(axis.front().id_, "payoff");
    ASSERT_EQ(axis.front().label_, "pay");
}

TEST(WeightedRiskPlanTest, TestPreparedAxesRequireSameSlotsInputsAndDate) {
    auto product = WeightedProduct();
    const Date_ date(2026, 1, 1);
    const auto inputs = WeightedInputs();
    const auto plan = PlanWeightedRiskRequest(product, inputs, date, {}, true);
    ASSERT_THROW(ValidateWeightedRiskPreparedAxes(plan, product, inputs), ScriptError_);
    product.PartitionEvents(date);
    ASSERT_NO_THROW(ValidateWeightedRiskPreparedAxes(plan, product, inputs));
    auto reordered = WeightedProduct("pay");
    reordered.PartitionEvents(date);
    ASSERT_THROW(ValidateWeightedRiskPreparedAxes(plan, reordered, inputs), ScriptError_);
    auto wrongDate = WeightedProduct();
    wrongDate.PartitionEvents(Date_(2026, 1, 2));
    ASSERT_THROW(ValidateWeightedRiskPreparedAxes(plan, wrongDate, inputs), ScriptError_);
    for (size_t change = 0; change < 3; ++change) {
        auto changed = inputs;
        if (change == 0)
            changed[0].value_ = 99.0;
        else if (change == 1)
            changed[0].physicalUnit_ = "invented";
        else
            changed[0].reportScale_ = 0.5;
        ASSERT_THROW(ValidateWeightedRiskPreparedAxes(plan, product, changed), ScriptError_);
    }
    auto shortened = inputs;
    shortened.pop_back();
    ASSERT_THROW(ValidateWeightedRiskPreparedAxes(plan, product, shortened), ScriptError_);
    ASSERT_NO_THROW(ValidateWeightedRiskPreparedAxes(plan, product, inputs));
}

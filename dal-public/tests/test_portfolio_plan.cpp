//
// Created by Codex on 2026/10/6.
//

#include <gtest/gtest.h>

#include <limits>

#include <dal-public/src/portfolioplaninternal.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/platform/platform.hpp>

#include <script_test_observers.hpp>

using namespace Dal;
using namespace Dal::Script;

namespace {
    Handle_<ScriptPortfolioData_> Portfolio() {
        const Handle_<ModelData_> model(new BSModelData_("", 100.0, 0.2));
        const Handle_<ScriptProductData_> product(new ScriptProductData_("", {Cell_("X"), Cell_(Date_(2025, 1, 1)), Cell_(Date_(2027, 1, 1))},
                                                                         {"5", "h = FIX(EQ[PORTFOLIO_PLAN_UNREAD])", "a = X pay PAYS h + X"}));
        return Handle_<ScriptPortfolioData_>(new ScriptPortfolioData_("", {{"A", product, model}, {"B", product, model}}));
    }
} // namespace

TEST(PortfolioPlanTest, TestDefaultsOwnAllPayoffsAndSharedPrivateAxes) {
    RegisterAll_::Init();
    const Handle_<ModelData_> model(new BSModelData_("", 100.0, 0.2));
    const Handle_<ScriptProductData_> product(new ScriptProductData_("", {Cell_("X"), Cell_(Date_(2027, 1, 1))}, {"5", "a = X pay PAYS SPOT() + X"}));
    const Handle_<ScriptPortfolioData_> data(new ScriptPortfolioData_("", {{"A", product, model}, {"B", product, model}}));
    const auto plan = Dal::Detail::PlanPortfolioWeightedRequest(data, {}, true);
    ASSERT_EQ(plan.Outputs().size(), 2);
    ASSERT_EQ(plan.Outputs()[0].coordinate_.id_, "trade:0:payoff");
    ASSERT_EQ(plan.Outputs()[1].coordinate_.id_, "trade:1:payoff");
    ASSERT_EQ(plan.Outputs()[0].weight_, 1.0);
    ASSERT_EQ(plan.Outputs()[1].weight_, 1.0);
    ASSERT_EQ(plan.InputPositions(), Vector_<size_t>({0, 1, 2, 3, 4, 5}));
    ASSERT_EQ(plan.InputAxis().size(), 6);
    ASSERT_EQ(plan.InputAxis()[4].id_, "trade:0:constant:0");
    ASSERT_EQ(plan.InputAxis()[5].id_, "trade:1:constant:0");
    ASSERT_EQ(plan.NumericPayloadBytes(), 11 * sizeof(double));
    ASSERT_TRUE(plan.EnableAad());
}

TEST(PortfolioPlanTest, TestReorderedSelectionsAndFactorsAreOwned) {
    RegisterAll_::Init();
    auto data = Portfolio();
    WeightedRiskRequest_ request;
    request.selection_.outputs_ = Vector_<String_>{"trade:1:payoff", "trade:0:output:1"};
    request.selection_.inputs_ = Vector_<String_>{"trade:1:constant:0", "model:0:parameter:0", "trade:0:constant:0"};
    request.selection_.reportFactors_ = Vector_<>{2.0, 0.5, 3.0};
    request.weights_ = Vector_<>{-1.0, 2.0};
    request.selection_.numericPayloadBudgetBytes_ = 8 * sizeof(double);
    const auto plan = Dal::Detail::PlanPortfolioWeightedRequest(data, request, true);
    request.selection_.outputs_->clear();
    request.selection_.inputs_->clear();
    (*request.weights_)[0] = 999;
    (*request.selection_.reportFactors_)[0] = 999;
    data.reset();
    ASSERT_EQ(plan.Outputs().size(), 2);
    ASSERT_EQ(plan.Outputs()[0].tradePosition_, 1);
    ASSERT_EQ(plan.Outputs()[1].tradePosition_, 0);
    ASSERT_EQ(plan.Outputs()[0].coordinate_.id_, "trade:1:payoff");
    ASSERT_EQ(plan.Outputs()[1].coordinate_.label_, "a");
    ASSERT_EQ(plan.Outputs()[0].weight_, -1.0);
    ASSERT_EQ(plan.Outputs()[1].weight_, 2.0);
    ASSERT_EQ(plan.InputPositions(), Vector_<size_t>({5, 0, 4}));
    ASSERT_EQ(plan.InputAxis()[0].reportScale_, 2.0);
    ASSERT_EQ(plan.InputAxis()[1].reportScale_, 0.5);
    ASSERT_EQ(plan.InputAxis()[2].reportScale_, 3.0);
    ASSERT_EQ(plan.Axes().InputAxis()[0].reportScale_, 1.0);
    ASSERT_EQ(plan.NumericPayloadBytes(), 8 * sizeof(double));
    ASSERT_TRUE(plan.Portfolio());
}

TEST(PortfolioPlanTest, TestNativeEmptyAndPassiveInputsRetainExactPayloadShape) {
    RegisterAll_::Init();
    const auto data = Portfolio();
    WeightedRiskRequest_ empty;
    empty.selection_.inputs_ = Vector_<String_>{};
    empty.selection_.numericPayloadBudgetBytes_ = 5 * sizeof(double);
    const auto native = Dal::Detail::PlanPortfolioWeightedRequest(data, empty, true);
    const auto passive = Dal::Detail::PlanPortfolioWeightedRequest(data, {}, false);
    ASSERT_TRUE(native.EnableAad());
    ASSERT_FALSE(passive.EnableAad());
    ASSERT_TRUE(native.InputPositions().empty());
    ASSERT_TRUE(passive.InputPositions().empty());
    ASSERT_TRUE(native.InputAxis().empty());
    ASSERT_TRUE(passive.InputAxis().empty());
    ASSERT_EQ(native.NumericPayloadBytes(), 5 * sizeof(double));
    ASSERT_EQ(passive.NumericPayloadBytes(), 5 * sizeof(double));
    ASSERT_EQ(native.Axes().InputAxis().size(), 6);
    ASSERT_EQ(passive.Axes().InputAxis().size(), 6);
    WeightedRiskRequest_ invalid;
    invalid.selection_.inputs_ = Vector_<String_>{"model:0:parameter:0"};
    ASSERT_THROW(static_cast<void>(Dal::Detail::PlanPortfolioWeightedRequest(data, invalid, false)), ScriptError_);
}

TEST(PortfolioPlanTest, TestDistinctEqualOwnersAndLegalAliasesKeepGlobalCoordinates) {
    RegisterAll_::Init();
    const Handle_<ModelData_> first(new BSModelData_("same", 100.0, 0.2));
    const Handle_<ModelData_> second(new BSModelData_("same", 100.0, 0.2));
    const Handle_<ScriptProductData_> product(
        new ScriptProductData_("", {Cell_("X"), Cell_(Date_(2027, 1, 1))}, {"5", "a = X b = a pay PAYS SPOT() + X"}));
    const Handle_<ScriptPortfolioData_> data(new ScriptPortfolioData_("", {{"A", product, first}, {"B", product, second}}));
    WeightedRiskRequest_ request;
    request.selection_.outputs_ = Vector_<String_>{"trade:1:output:1", "trade:0:output:0", "trade:0:output:1"};
    request.selection_.inputs_ = Vector_<String_>{"model:1:parameter:0", "trade:1:constant:0", "model:0:parameter:0"};
    request.weights_ = Vector_<>{0.0, -1.0, 2.0};
    const auto plan = Dal::Detail::PlanPortfolioWeightedRequest(data, request, true);
    ASSERT_EQ(plan.Axes().InputAxis().size(), 10);
    ASSERT_EQ(plan.InputPositions(), Vector_<size_t>({4, 9, 0}));
    ASSERT_EQ(plan.InputAxis()[0].id_, "model:1:parameter:0");
    ASSERT_EQ(plan.InputAxis()[2].id_, "model:0:parameter:0");
    ASSERT_EQ(plan.InputAxis()[0].ordinal_, plan.InputAxis()[2].ordinal_);
    ASSERT_EQ(plan.Outputs().size(), 3);
    ASSERT_EQ(plan.Outputs()[0].weight_, 0.0);
    ASSERT_EQ(plan.Outputs()[1].coordinate_.label_, "a");
    ASSERT_EQ(plan.Outputs()[2].coordinate_.label_, "b");
    ASSERT_EQ(plan.NumericPayloadBytes(), 10 * sizeof(double));
}

TEST(PortfolioPlanTest, TestMalformedSelectionsAndPayloadRejectBeforeHistoryOrTasks) {
    RegisterAll_::Init();
    const auto data = Portfolio();
    Script::TestSupport::RejectFixingReads_ history;
    Script::TestSupport::RejectSubmissions_ tasks;
    const Dal::Detail::ScopedFixingReadObserver_ observeHistory(&history);
    const Script::Detail::ScopedSimulationObserver_ observeTasks(&tasks);
    Vector_<WeightedRiskRequest_> invalid;
    WeightedRiskRequest_ request;
    for (const auto& outputs : Vector_<Vector_<String_>>{{}, {"trade:0:payoff", "TRADE:0:PAYOFF"}, {"trade:9:payoff"}}) {
        request.selection_.outputs_ = outputs;
        invalid.push_back(request);
    }
    request = {};
    for (const auto& inputs : Vector_<Vector_<String_>>{{"model:0:parameter:0", "MODEL:0:PARAMETER:0"}, {"model:9:parameter:0"}}) {
        request.selection_.inputs_ = inputs;
        invalid.push_back(request);
    }
    request = {};
    for (const auto& weights : Vector_<Vector_<double>>{{}, {1.0}, {1.0, std::numeric_limits<double>::infinity()}}) {
        request.weights_ = weights;
        invalid.push_back(request);
    }
    request = {};
    request.selection_.numericPayloadBudgetBytes_ = 11 * sizeof(double) - 1;
    invalid.push_back(request);
    request = {};
    request.selection_.reportFactors_ = Vector_<>{1.0};
    invalid.push_back(request);
    for (const auto& malformed : invalid)
        ASSERT_THROW(static_cast<void>(Dal::Detail::PlanPortfolioWeightedRequest(data, malformed, true)), ScriptError_);
    ASSERT_THROW(static_cast<void>(Dal::Detail::PlanPortfolioWeightedRequest({}, {}, true)), ScriptError_);
    ASSERT_NO_THROW(static_cast<void>(Dal::Detail::PlanPortfolioWeightedRequest(data, {}, true)));
    ASSERT_EQ(history.historyCalls_, 0);
    ASSERT_EQ(history.fixingCalls_, 0);
    ASSERT_EQ(tasks.calls_, 0);
}

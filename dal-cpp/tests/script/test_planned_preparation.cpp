//
// Created by Codex on 2026/10/6.
//

#include <gtest/gtest.h>

#include <dal/model/blackscholes.hpp>
#include <dal/script/plannedpreparation.hpp>

#include "script_test_observers.hpp"

using namespace Dal;
using namespace Dal::Script;
using namespace Dal::Script::TestSupport;

namespace {
    ScriptProductData_ HistoricalProduct() {
        return {"", {Cell_(Date_(2026, 1, 1)), Cell_(Date_(2027, 1, 1))}, {"state = FIX(EQ[PLANNED_PAST])", "pay PAYS state + FIX(EQ[MODEL])"}};
    }

    ScriptValuationSettings_ Valuation() {
        ScriptValuationSettings_ result;
        result.evaluationDate_ = Date_(2026, 1, 2);
        return result;
    }

    Handle_<MarketFixingSnapshot_> History() {
        return Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_({{"EQ[PLANNED_PAST]", {{DateTime_(Date_(2026, 1, 1), 0.0), 80.0}}}}));
    }
} // namespace

TEST(PlannedPreparationTest, TestOwnedPlanDefersHistoryAndRetainsGlobalSource) {
    ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = Date_(2026, 1, 2);
    MonteCarloSettings_ simulation;
    simulation.compiled_ = true;
    const ScriptProductData_ product("", {Cell_("X"), Cell_(Date_(2026, 1, 1)), Cell_(Date_(2027, 1, 1))},
                                     {"5", "state = X * FIX(EQ[PLANNED_PAST])", "pay PAYS state + FIX(EQ[MODEL])"});
    auto planned = [&] {
        RejectFixingReads_ reads;
        RejectSubmissions_ tasks;
        const Dal::Detail::ScopedFixingReadObserver_ observeReads(&reads);
        const Dal::Script::Detail::ScopedSimulationObserver_ observeTasks(&tasks);
        return Dal::Script::Detail::PlanScript(product, std::make_unique<AAD::BlackScholes_<double>>(100.0, 0.2), valuation, simulation);
    }();
    ASSERT_TRUE(planned.View().Plan().KnownValues().empty());
    ASSERT_EQ(planned.View().TimeLine().size(), 1);
    ASSERT_EQ(planned.Model().SimDim(), 1);
    ASSERT_THROW(planned.View().RequireExecutable(), ScriptError_);
    const Handle_<MarketFixingSnapshot_> snapshot(new MarketFixingSnapshot_({{"EQ[PLANNED_PAST]", {{DateTime_(Date_(2026, 1, 1), 0.0), 80.0}}}}));
    NamedFixingReadCounter_ reads;
    const Dal::Detail::ScopedFixingReadObserver_ observeReads(&reads);
    const auto prepared = Dal::Script::Detail::CompleteScriptPreparation(std::move(planned), snapshot);
    prepared.RequireExecutable();
    ASSERT_STREQ(FixingSourceKind(prepared.Settings()), "GlobalSnapshot");
    ASSERT_TRUE(reads.histories_.empty());
    ASSERT_EQ(reads.fixings_, 1);
    AAD::Scenario_<double> path;
    AAD::AllocatePath(prepared.DefLine(), path);
    AAD::InitializePath(path);
    path[0].observations_[0] = 100.0;
    auto evaluator = prepared.BuildEvalState<double>();
    prepared.CompiledProgram().Evaluate(path, evaluator);
    ASSERT_DOUBLE_EQ(evaluator.VarVals()[prepared.PayOffIdx()], 500.0);
}

TEST(PlannedPreparationTest, TestInvalidAndUnsupportedPlansReadNoHistory) {
    RejectFixingReads_ reads;
    RejectSubmissions_ tasks;
    const Dal::Detail::ScopedFixingReadObserver_ observeReads(&reads);
    const Dal::Script::Detail::ScopedSimulationObserver_ observeTasks(&tasks);
    ASSERT_THROW(static_cast<void>(Dal::Script::Detail::PlanScript(HistoricalProduct(), {}, Valuation())), ScriptError_);
    for (const auto& [date, event] :
         Vector_<std::pair<Date_, String_>>{{Date_(2025, 1, 1), "pay PAYS FIX(EQ[PLANNED_PAST])"}, {Date_(2027, 1, 1), "EXERCISE 1"}}) {
        const ScriptProductData_ product("", {Cell_(date)}, {event});
        ASSERT_THROW(
            static_cast<void>(Dal::Script::Detail::PlanScript(product, std::make_unique<AAD::BlackScholes_<double>>(100.0, 0.2), Valuation())),
            ScriptError_);
    }
    ASSERT_EQ(reads.historyCalls_, 0);
    ASSERT_EQ(reads.fixingCalls_, 0);
    ASSERT_EQ(tasks.calls_, 0);
}

TEST(PlannedPreparationTest, TestNullAndSubstitutedSnapshotsFailBeforeReads) {
    auto valuation = Valuation();
    valuation.fixings_ = History();
    const auto fresh = [&] {
        return Dal::Script::Detail::PlanScript(HistoricalProduct(), std::make_unique<AAD::BlackScholes_<double>>(100.0, 0.2), valuation);
    };
    RejectFixingReads_ reads;
    const Dal::Detail::ScopedFixingReadObserver_ observeReads(&reads);
    auto withoutSnapshot = fresh();
    ASSERT_THROW(static_cast<void>(Dal::Script::Detail::CompleteScriptPreparation(std::move(withoutSnapshot), {})), ScriptError_);
    auto substituted = fresh();
    ASSERT_THROW(static_cast<void>(Dal::Script::Detail::CompleteScriptPreparation(std::move(substituted), History())), ScriptError_);
    ASSERT_EQ(reads.historyCalls_, 0);
    ASSERT_EQ(reads.fixingCalls_, 0);
}

TEST(PlannedPreparationTest, TestExplicitSnapshotAndOwnedModelSurviveCallerMutation) {
    auto valuation = Valuation();
    valuation.fixings_ = History();
    const auto snapshot = valuation.fixings_;
    auto model = std::make_unique<AAD::BlackScholes_<double>>(100.0, 0.2);
    auto planned = Dal::Script::Detail::PlanScript(HistoricalProduct(), std::move(model), valuation);
    ASSERT_FALSE(model);
    valuation.evaluationDate_ = Date_(2030, 1, 1);
    valuation.fixings_.reset();
    const auto prepared = Dal::Script::Detail::CompleteScriptPreparation(std::move(planned), snapshot);
    ASSERT_EQ(prepared.EvaluationDate(), Date_(2026, 1, 2));
    ASSERT_STREQ(FixingSourceKind(prepared.Settings()), "ExplicitSnapshot");
    ASSERT_DOUBLE_EQ(prepared.Plan().KnownValue(0), 80.0);
    ASSERT_THROW(static_cast<void>(planned.Model()), ScriptError_);
    ASSERT_THROW(static_cast<void>(Dal::Script::Detail::CompleteScriptPreparation(std::move(planned), snapshot)), ScriptError_);
}

TEST(PlannedPreparationTest, TestCompletionFailureLeavesPriorResultAndFreshPlanUsable) {
    const auto fresh = [&] {
        return Dal::Script::Detail::PlanScript(HistoricalProduct(), std::make_unique<AAD::BlackScholes_<double>>(100.0, 0.2), Valuation());
    };
    auto first = fresh();
    const auto prior = Dal::Script::Detail::CompleteScriptPreparation(std::move(first), History());
    auto failing = fresh();
    const Handle_<MarketFixingSnapshot_> empty(new MarketFixingSnapshot_);
    ASSERT_THROW(static_cast<void>(Dal::Script::Detail::CompleteScriptPreparation(std::move(failing), empty)), ScriptError_);
    auto following = fresh();
    const auto recovered = Dal::Script::Detail::CompleteScriptPreparation(std::move(following), History());
    ASSERT_DOUBLE_EQ(prior.Plan().KnownValue(0), 80.0);
    ASSERT_DOUBLE_EQ(recovered.Plan().KnownValue(0), 80.0);
    ASSERT_EQ(prior.TimeLine(), recovered.TimeLine());
}

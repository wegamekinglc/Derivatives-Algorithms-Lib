//
// Created by Codex on 2026/10/7.
//

#include <gtest/gtest.h>

#include <dal-public/src/portfolioplaninternal.hpp>
#include <dal-public/src/portfolioreplayinternal.hpp>
#include <dal-public/src/value.hpp>
#include <dal/concurrency/threadpool.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/platform/platform.hpp>

#include <script_test_observers.hpp>

using namespace Dal;
using namespace Dal::Script;

namespace {
    struct SinglePortfolioWorker_ {
        ThreadPool_* pool_ = ThreadPool_::GetInstance();
        size_t previous_ = pool_->NumThreads();
        SinglePortfolioWorker_() { pool_->Start(1, true); }
        ~SinglePortfolioWorker_() { pool_->Start(previous_, true); }
    };

    Handle_<ScriptPortfolioData_> HistoricalPortfolio(bool large) {
        const Handle_<ModelData_> model(new BSModelData_("", 100.0, 0.2));
        const String_ seed = large ? "v[4095] = X " : "v[1] = X ";
        const Handle_<ScriptProductData_> product(new ScriptProductData_(
            "", {Cell_("X"), Cell_(Date_(2025, 1, 1)), Cell_(Date_(2027, 1, 1))},
            {"5", seed + "h = FIX(EQ[PORTFOLIO_ADMISSION])", "pay PAYS SPOT() + h + SUM(v)"}, ScriptProductSettings_{"EQ[MODEL]", {}}));
        return Handle_<ScriptPortfolioData_>(new ScriptPortfolioData_("", {{"A", product, model}, {"B", product, model}}));
    }

    ScriptValuationSettings_ Valuation() {
        ScriptValuationSettings_ settings;
        settings.evaluationDate_ = Date_(2026, 1, 1);
        return settings;
    }

    void AssertCapacityFailureBeforeHistory(const Dal::Detail::PortfolioWeightedPlan_& plan,
                                            const MonteCarloSettings_& simulation,
                                            const Dal::Script::Detail::PortfolioCapacityLimits_& limits,
                                            const String_& capacity) {
        Script::TestSupport::RejectFixingReads_ reads;
        Script::TestSupport::RejectSubmissions_ tasks;
        const Dal::Detail::ScopedFixingReadObserver_ observeReads(&reads);
        const Dal::Script::Detail::ScopedSimulationObserver_ observeTasks(&tasks);
        try {
            static_cast<void>(Dal::Detail::PreparePortfolioWeightedReplay(plan, 257, Valuation(), simulation, limits));
            FAIL() << "startup capacity must reject before history";
        } catch (const Exception_& error) {
            ASSERT_NE(String_(error.what()).find(capacity + " capacity budget exceeded"), String_::npos) << error.what();
        }
        ASSERT_EQ(reads.historyCalls_, 0);
        ASSERT_EQ(reads.fixingCalls_, 0);
        ASSERT_EQ(tasks.calls_, 0);
    }
} // namespace

TEST(PortfolioAdmissionTest, TestKnownBudgetsRejectBeforeHistoricalReadsAndTasks) {
    RegisterAll_::Init();
    const auto data = HistoricalPortfolio(true);
    const auto plan = Dal::Detail::PlanPortfolioWeightedRequest(data, {}, true);
    for (const bool compiled : {false, true}) {
        MonteCarloSettings_ simulation;
        simulation.enableAad_ = true;
        simulation.compiled_ = compiled;
        Dal::Script::Detail::PortfolioCapacityLimits_ limits;
        limits.scratchBudgetBytes_ = 0;
        ASSERT_NO_FATAL_FAILURE(AssertCapacityFailureBeforeHistory(plan, simulation, limits, "Scratch buffer"));
        limits.scratchBudgetBytes_ = 32768;
        ASSERT_NO_FATAL_FAILURE(AssertCapacityFailureBeforeHistory(plan, simulation, limits, "Scratch buffer"));
        limits.scratchBudgetBytes_.reset();
        limits.tapeBudgetBytes_ = 0;
        ASSERT_NO_FATAL_FAILURE(AssertCapacityFailureBeforeHistory(plan, simulation, limits, "Tape"));
    }
}

TEST(PortfolioAdmissionTest, TestPassivePrivateHistoryShapesRejectBeforeReadsAndIgnoreTapeBudget) {
    RegisterAll_::Init();
    const SinglePortfolioWorker_ worker;
    const auto large = HistoricalPortfolio(true);
    const auto small = HistoricalPortfolio(false);
    const auto plan = Dal::Detail::PlanPortfolioWeightedRequest(large, {}, false);
    Script::TestSupport::StoreScriptTestFixing("EQ[PORTFOLIO_ADMISSION]", 80.0, DateTime_(Date_(2025, 1, 1), 0.0));
    for (const bool compiled : {false, true}) {
        MonteCarloSettings_ simulation;
        simulation.compiled_ = compiled;
        Dal::Script::Detail::PortfolioCapacityLimits_ limits{32768, 0};
        ASSERT_NO_FATAL_FAILURE(AssertCapacityFailureBeforeHistory(plan, simulation, limits, "Scratch buffer"));
        limits.scratchBudgetBytes_ = 64 * 1024 * 1024;
        const auto prepared = Dal::Detail::PreparePortfolioWeightedReplay(Dal::Detail::PlanPortfolioWeightedRequest(small, {}, false), 257,
                                                                          Valuation(), simulation, limits);
        Vector_<Dal::Script::Detail::PortfolioBatchOutput_> outputs;
        for (size_t trade = 0; trade < prepared.Trades().size(); ++trade) {
            auto coordinate = ScriptRiskOutputAxis(prepared.Trades()[trade].Product())[prepared.Trades()[trade].PayOffIdx()];
            coordinate.id_ = "trade:" + String_(std::to_string(trade)) + ":" + coordinate.id_;
            outputs.push_back({trade, coordinate, 1.0});
        }
        const auto result = Dal::Detail::EvaluatePortfolioWeightedReplay(prepared, outputs, {}, limits);
        ASSERT_EQ(result.gradient_.size(), 0);
        ASSERT_EQ(result.peakTapeBytes_, 0);
        ASSERT_LE(result.peakScratchBytes_, *limits.scratchBudgetBytes_);
        for (size_t trade = 0; trade < 2; ++trade) {
            const auto scalar = ValueByMonteCarloWithRisk(small->Products()[trade], small->Models()[0], 257, {}, Valuation(), simulation);
            ASSERT_NEAR(result.componentMeans_[trade], scalar.Values()[0], 1e-10);
        }
        ASSERT_EQ(result.groupCounters_[0].generatedScenarios_, 257);
        ASSERT_EQ(result.groupCounters_[0].evaluatorCalls_, 514);
    }
}

TEST(PortfolioAdmissionTest, TestFiniteStartupBudgetCompletesPrivateHistoryAndSharedReplay) {
    RegisterAll_::Init();
    Script::TestSupport::StoreScriptTestFixing("EQ[PORTFOLIO_ADMISSION]", 80.0, DateTime_(Date_(2025, 1, 1), 0.0));
    const auto data = HistoricalPortfolio(false);
    WeightedRiskRequest_ request;
    request.weights_ = Vector_<>{2.0, -1.0};
    request.selection_.inputs_ = Vector_<String_>{"model:0:parameter:0", "trade:0:constant:0", "trade:1:constant:0"};
    const auto plan = Dal::Detail::PlanPortfolioWeightedRequest(data, request, true);
    Dal::Script::Detail::PortfolioCapacityLimits_ limits;
    limits.scratchBudgetBytes_ = 64 * 1024 * 1024;
    limits.tapeBudgetBytes_ = 256 * 1024 * 1024;
    for (const bool compiled : {false, true}) {
        MonteCarloSettings_ simulation;
        simulation.enableAad_ = true;
        simulation.compiled_ = compiled;
        Script::TestSupport::FixingReadCounter_ reads;
        const Dal::Detail::ScopedFixingReadObserver_ observeReads(&reads);
        const auto prepared = Dal::Detail::PreparePortfolioWeightedReplay(plan, 257, Valuation(), simulation, limits);
        ASSERT_EQ(reads.histories_, 1);
        ASSERT_EQ(prepared.Groups().size(), 1);
        const auto result = Dal::Detail::EvaluatePortfolioWeightedReplay(prepared, plan.Outputs(), plan.InputPositions(), limits);
        ASSERT_EQ(result.componentMeans_.size(), 2);
        ASSERT_EQ(result.componentMeans_[0], result.componentMeans_[1]);
        ASSERT_DOUBLE_EQ(result.weightedValue_, result.componentMeans_[0]);
        ASSERT_DOUBLE_EQ(result.gradient_[0], (result.componentMeans_[0] - 85.0) / 100.0);
        ASSERT_DOUBLE_EQ(result.gradient_[1], 2.0);
        ASSERT_DOUBLE_EQ(result.gradient_[2], -1.0);
        ASSERT_EQ(result.groupCounters_[0].generatedScenarios_, 257);
        ASSERT_LE(result.peakScratchBytes_, *limits.scratchBudgetBytes_);
        ASSERT_LE(result.peakTapeBytes_, *limits.tapeBudgetBytes_);
    }
}

TEST(PortfolioAdmissionTest, TestAdmissionChargesEverySelectedPrivateVectorAndSkipsUnselectedState) {
    RegisterAll_::Init();
    const SinglePortfolioWorker_ worker;
    Script::TestSupport::StoreScriptTestFixing("EQ[PORTFOLIO_ADMISSION]", 80.0, DateTime_(Date_(2025, 1, 1), 0.0));
    const auto data = HistoricalPortfolio(true);
    WeightedRiskRequest_ request;
    request.selection_.inputs_ = Vector_<String_>{};
    const auto both = Dal::Detail::PlanPortfolioWeightedRequest(data, request, true);
    request.selection_.outputs_ = Vector_<String_>{"trade:0:payoff"};
    const auto one = Dal::Detail::PlanPortfolioWeightedRequest(data, request, true);
    Dal::Script::Detail::PortfolioCapacityLimits_ limits;
    limits.scratchBudgetBytes_ = 5 * 4096 * sizeof(AAD::Number_) + 32768;
    for (const bool compiled : {false, true}) {
        MonteCarloSettings_ simulation;
        simulation.enableAad_ = true;
        simulation.compiled_ = compiled;
        const auto prepared = Dal::Detail::PreparePortfolioWeightedReplay(one, 257, Valuation(), simulation, limits);
        const auto result = Dal::Detail::EvaluatePortfolioWeightedReplay(prepared, one.Outputs(), one.InputPositions(), limits);
        ASSERT_TRUE(result.gradient_.empty());
        ASSERT_EQ(result.groupCounters_[0].evaluatorCalls_, 257);
        ASSERT_NO_FATAL_FAILURE(AssertCapacityFailureBeforeHistory(both, simulation, limits, "Scratch buffer"));
        const auto recovered = Dal::Detail::PreparePortfolioWeightedReplay(one, 257, Valuation(), simulation, limits);
        const auto again = Dal::Detail::EvaluatePortfolioWeightedReplay(recovered, one.Outputs(), one.InputPositions(), limits);
        ASSERT_EQ(again.weightedValue_, result.weightedValue_);
    }
}

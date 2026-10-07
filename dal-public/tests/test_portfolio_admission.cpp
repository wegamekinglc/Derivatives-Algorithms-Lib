//
// Created by Codex on 2026/10/7.
//

#include <gtest/gtest.h>

#include <limits>

#include <dal-public/src/portfolioplaninternal.hpp>
#include <dal-public/src/portfolioreplayinternal.hpp>
#include <dal-public/src/value.hpp>
#include <dal/concurrency/threadpool.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/simulation.hpp>

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

    Handle_<ScriptPortfolioData_> WidePrivateInputPortfolio() {
        Vector_<Cell_> dates;
        Vector_<String_> events;
        String_ history = "h = 0";
        for (size_t input = 0; input < 1024; ++input) {
            const auto name = "C" + String_(std::to_string(input));
            dates.push_back(Cell_(name));
            events.push_back("1");
            history += " + " + name;
        }
        dates.push_back(Cell_(Date_(2025, 1, 1)));
        events.push_back(history);
        dates.push_back(Cell_(Date_(2027, 1, 1)));
        events.push_back("pay PAYS SPOT() + h");
        const Handle_<ScriptProductData_> product(new ScriptProductData_("", dates, events));
        const Handle_<ModelData_> model(new BSModelData_("", 100.0, 0.0));
        return Handle_<ScriptPortfolioData_>(new ScriptPortfolioData_("", {{"A", product, model}, {"B", product, model}}));
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

TEST(PortfolioAdmissionTest, TestSelectedPrivateGradientSlotsFitBelowFullExtractionCapacity) {
    RegisterAll_::Init();
    const SinglePortfolioWorker_ worker;
    const auto data = WidePrivateInputPortfolio();
    constexpr int PATHS = 1048577;
    const Script::BatchPlan_ batches(PATHS, 1);
    for (const bool compiled : {false, true}) {
        auto simulation = DefaultRiskMonteCarloSettings();
        simulation.compiled_ = compiled;
        PortfolioWeightedRiskRequest_ request;
        request.weights_ = Vector_<double>{2.0, -1.0};
        const auto full = ValuePortfolioByMonteCarloWithWeightedRisk(data, PATHS, request, Valuation(), simulation);
        ASSERT_EQ(full.CompleteInputAxis().size(), 2052);
        request.selection_.inputs_ = Vector_<String_>{"trade:1:constant:1023", "model:0:parameter:0", "trade:0:constant:0"};
        const size_t discardedSlots = batches.BatchCount() * (full.CompleteInputAxis().size() - request.selection_.inputs_->size()) * sizeof(double);
        ASSERT_GT(full.Execution().peakScratchBytes_, discardedSlots);
        request.scratchCapacityBudgetBytes_ = full.Execution().peakScratchBytes_ - discardedSlots / 2;
        request.recordingCapacityBudgetBytes_ = 64 * 1024 * 1024;
        const auto selected = ValuePortfolioByMonteCarloWithWeightedRisk(data, PATHS, request, Valuation(), simulation);
        ASSERT_EQ(selected.ComponentMeans(), full.ComponentMeans());
        ASSERT_DOUBLE_EQ(selected.WeightedValue(), full.WeightedValue());
        ASSERT_EQ(selected.Jacobian().Cols(), 3);
        ASSERT_DOUBLE_EQ(selected.Jacobian()(0, 0), -1.0);
        ASSERT_DOUBLE_EQ(selected.Jacobian()(0, 1), 1.0);
        ASSERT_DOUBLE_EQ(selected.Jacobian()(0, 2), 2.0);
        ASSERT_LE(selected.Execution().peakScratchBytes_, *request.scratchCapacityBudgetBytes_);
        ASSERT_EQ(selected.Execution().groups_[0].generatedScenarios_, PATHS);
        ASSERT_EQ(selected.Execution().groups_[0].evaluatorCalls_, 2 * PATHS);
        ASSERT_EQ(selected.Execution().groups_[0].suffixReversals_, PATHS);
        ASSERT_EQ(selected.Execution().groups_[0].prefixReversals_, batches.BatchCount());
    }
}

TEST(PortfolioAdmissionTest, TestSelectedJacobianGradientSlotsFitWithoutCapacityNarrowing) {
    RegisterAll_::Init();
    const SinglePortfolioWorker_ worker;
    const auto data = WidePrivateInputPortfolio();
    constexpr int PATHS = 1048577;
    const Script::BatchPlan_ batches(PATHS, 1);
    for (const bool compiled : {false, true}) {
        auto simulation = DefaultRiskMonteCarloSettings();
        simulation.compiled_ = compiled;
        PortfolioJacobianRiskRequest_ request;
        request.maxBlockWidth_ = 2;
        request.selection_.outputs_ = Vector_<String_>{"trade:1:payoff", "trade:0:output:0", "trade:0:payoff"};
        const auto full = ValuePortfolioByMonteCarloWithJacobianRisk(data, PATHS, request, Valuation(), simulation);
        request.selection_.inputs_ = Vector_<String_>{"trade:1:constant:1023", "model:0:parameter:0", "trade:0:constant:0"};
        const size_t discardedSlots =
            2 * batches.BatchCount() * (full.CompleteInputAxis().size() - request.selection_.inputs_->size()) * sizeof(double);
        ASSERT_GT(full.Execution().peakScratchBytes_, discardedSlots);
        request.scratchCapacityBudgetBytes_ = full.Execution().peakScratchBytes_ - discardedSlots / 2;
        request.recordingCapacityBudgetBytes_ = 256 * 1024 * 1024;
        const auto selected = ValuePortfolioByMonteCarloWithJacobianRisk(data, PATHS, request, Valuation(), simulation);
        ASSERT_EQ(selected.Values(), full.Values());
        ASSERT_EQ(selected.Execution().groups_[0].actualWidths_, (Vector_<size_t>{2, 2}));
        ASSERT_EQ(selected.Execution().groups_[0].actualWidths_, full.Execution().groups_[0].actualWidths_);
        ASSERT_EQ(selected.Jacobian().Rows(), 3);
        ASSERT_EQ(selected.Jacobian().Cols(), 3);
        ASSERT_DOUBLE_EQ(selected.Jacobian()(0, 0), 1.0);
        ASSERT_DOUBLE_EQ(selected.Jacobian()(0, 1), 1.0);
        ASSERT_DOUBLE_EQ(selected.Jacobian()(0, 2), 0.0);
        ASSERT_DOUBLE_EQ(selected.Jacobian()(1, 0), 0.0);
        ASSERT_DOUBLE_EQ(selected.Jacobian()(1, 1), 0.0);
        ASSERT_DOUBLE_EQ(selected.Jacobian()(1, 2), 1.0);
        ASSERT_DOUBLE_EQ(selected.Jacobian()(2, 0), 0.0);
        ASSERT_DOUBLE_EQ(selected.Jacobian()(2, 1), 1.0);
        ASSERT_DOUBLE_EQ(selected.Jacobian()(2, 2), 1.0);
        ASSERT_LE(selected.Execution().peakScratchBytes_, *request.scratchCapacityBudgetBytes_);
        ASSERT_EQ(selected.Execution().groups_[0].generatedScenarios_, 2 * PATHS);
        ASSERT_EQ(selected.Execution().groups_[0].evaluatorCalls_, 3 * PATHS);
        ASSERT_EQ(selected.Execution().groups_[0].prefixReversals_, 2 * batches.BatchCount());
    }
}

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

TEST(PortfolioAdmissionTest, TestJacobianPrivateHistoryShapesRejectBeforeReadsAndFiniteRisksRecover) {
    RegisterAll_::Init();
    const SinglePortfolioWorker_ worker;
    const auto data = HistoricalPortfolio(true);
    for (const bool compiled : {false, true}) {
        auto simulation = DefaultRiskMonteCarloSettings();
        simulation.compiled_ = compiled;
        PortfolioJacobianRiskRequest_ request;
        request.maxBlockWidth_ = 2;
        request.scratchCapacityBudgetBytes_ = 32768;
        {
            Script::TestSupport::RejectFixingReads_ history;
            Script::TestSupport::RejectSubmissions_ tasks;
            const Dal::Detail::ScopedFixingReadObserver_ observeHistory(&history);
            const Script::Detail::ScopedSimulationObserver_ observeTasks(&tasks);
            ASSERT_THROW(static_cast<void>(ValuePortfolioByMonteCarloWithJacobianRisk(data, 257, request, Valuation(), simulation)), Exception_);
            ASSERT_EQ(history.historyCalls_, 0);
            ASSERT_EQ(history.fixingCalls_, 0);
            ASSERT_EQ(tasks.calls_, 0);
        }
        auto valuation = Valuation();
        valuation.fixings_ =
            Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_({{"EQ[PORTFOLIO_ADMISSION]", {{DateTime_(Date_(2025, 1, 1), 0.0), 80.0}}}}));
        request.scratchCapacityBudgetBytes_ = 64 * 1024 * 1024;
        request.recordingCapacityBudgetBytes_ = 256 * 1024 * 1024;
        const auto result = ValuePortfolioByMonteCarloWithJacobianRisk(data, 257, request, valuation, simulation);
        const auto matrix = result.Jacobian();
        const auto reference = ValueByMonteCarloWithRisk(data->Products()[0], data->Models()[0], 257, {}, valuation, simulation);
        ASSERT_EQ(matrix.Rows(), 2);
        ASSERT_EQ(matrix.Cols(), 6);
        ASSERT_NEAR(result.Values()[0], reference.Values()[0], 1e-10);
        ASSERT_EQ(result.Values()[0], result.Values()[1]);
        for (int input = 0; input < 4; ++input) {
            ASSERT_NEAR(matrix(0, input), reference.Jacobian()(0, input), 1e-10);
            ASSERT_NEAR(matrix(1, input), reference.Jacobian()(0, input), 1e-10);
        }
        ASSERT_DOUBLE_EQ(matrix(0, 4), 1.0);
        ASSERT_DOUBLE_EQ(matrix(0, 5), 0.0);
        ASSERT_DOUBLE_EQ(matrix(1, 4), 0.0);
        ASSERT_DOUBLE_EQ(matrix(1, 5), 1.0);
        ASSERT_EQ(result.Execution().groups_[0].generatedScenarios_, 257);
        ASSERT_EQ(result.Execution().groups_[0].evaluatorCalls_, 514);
        ASSERT_LE(result.Execution().peakScratchBytes_, *request.scratchCapacityBudgetBytes_);
        ASSERT_LE(result.Execution().peakRecordingBytes_, *request.recordingCapacityBudgetBytes_);
    }
}

TEST(PortfolioAdmissionTest, TestJacobianNarrowingPreservesResultsUnderTheSameFiniteBudgets) {
    RegisterAll_::Init();
    const SinglePortfolioWorker_ worker;
    const auto data = HistoricalPortfolio(true);
    auto valuation = Valuation();
    valuation.fixings_ =
        Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_({{"EQ[PORTFOLIO_ADMISSION]", {{DateTime_(Date_(2025, 1, 1), 0.0), 80.0}}}}));
    for (const bool compiled : {false, true}) {
        auto simulation = DefaultRiskMonteCarloSettings();
        simulation.compiled_ = compiled;
        PortfolioJacobianRiskRequest_ request;
        request.maxBlockWidth_ = 2;
        request.selection_.inputs_ = Vector_<String_>{"model:0:parameter:0", "trade:0:constant:0", "trade:1:constant:0"};
        request.scratchCapacityBudgetBytes_ = 5 * 4096 * sizeof(AAD::Number_) + 32768;
        request.recordingCapacityBudgetBytes_ = 16 * 1024 * 1024;
        const auto narrowed = ValuePortfolioByMonteCarloWithJacobianRisk(data, 257, request, valuation, simulation);
        ASSERT_EQ(narrowed.Execution().requestedMaxBlockWidth_, 2);
        ASSERT_EQ(narrowed.Execution().groups_[0].actualWidths_, (Vector_<size_t>{1, 1}));
        ASSERT_EQ(narrowed.Execution().groups_[0].replayAttempts_, 2);
        ASSERT_EQ(narrowed.Execution().groups_[0].generatedScenarios_, 514);
        ASSERT_EQ(narrowed.Execution().groups_[0].evaluatorCalls_, 514);
        ASSERT_EQ(narrowed.Jacobian().Rows(), 2);
        ASSERT_EQ(narrowed.Jacobian().Cols(), 3);
        ASSERT_LE(narrowed.Execution().peakScratchBytes_, *request.scratchCapacityBudgetBytes_);
        ASSERT_LE(narrowed.Execution().peakRecordingBytes_, *request.recordingCapacityBudgetBytes_);
        request.maxBlockWidth_ = 1;
        const auto explicitWidth = ValuePortfolioByMonteCarloWithJacobianRisk(data, 257, request, valuation, simulation);
        const auto reference = ValueByMonteCarloWithRisk(data->Products()[0], data->Models()[0], 257, {}, valuation, simulation);
        ASSERT_EQ(narrowed.Values(), explicitWidth.Values());
        ASSERT_EQ(narrowed.Values(), Vector_<double>(2, reference.Values()[0]));
        const auto matrix = narrowed.Jacobian();
        const auto explicitMatrix = explicitWidth.Jacobian();
        for (int row = 0; row < 2; ++row) {
            ASSERT_NEAR(matrix(row, 0), reference.Jacobian()(0, 0), 1e-10);
            ASSERT_DOUBLE_EQ(matrix(row, 1 + row), reference.Jacobian()(0, 4));
            ASSERT_DOUBLE_EQ(matrix(row, 2 - row), 0.0);
            for (int column = 0; column < 3; ++column)
                ASSERT_DOUBLE_EQ(matrix(row, column), explicitMatrix(row, column));
        }
    }
}

TEST(PortfolioAdmissionTest, TestPassiveJacobianPrivateHistoryAdmissionAndZeroTapeBudget) {
    RegisterAll_::Init();
    const SinglePortfolioWorker_ worker;
    const auto data = HistoricalPortfolio(true);
    for (const bool compiled : {false, true}) {
        auto simulation = MonteCarloSettings_();
        simulation.enableAad_ = false;
        simulation.compiled_ = compiled;
        PortfolioJacobianRiskRequest_ request;
        request.maxBlockWidth_ = 2;
        request.scratchCapacityBudgetBytes_ = 32768;
        request.recordingCapacityBudgetBytes_ = 0;
        {
            Script::TestSupport::RejectFixingReads_ history;
            Script::TestSupport::RejectSubmissions_ tasks;
            const Dal::Detail::ScopedFixingReadObserver_ observeHistory(&history);
            const Script::Detail::ScopedSimulationObserver_ observeTasks(&tasks);
            ASSERT_THROW(static_cast<void>(ValuePortfolioByMonteCarloWithJacobianRisk(data, 257, request, Valuation(), simulation)), Exception_);
            ASSERT_EQ(history.historyCalls_, 0);
            ASSERT_EQ(history.fixingCalls_, 0);
            ASSERT_EQ(tasks.calls_, 0);
        }
        auto valuation = Valuation();
        valuation.fixings_ =
            Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_({{"EQ[PORTFOLIO_ADMISSION]", {{DateTime_(Date_(2025, 1, 1), 0.0), 80.0}}}}));
        request.scratchCapacityBudgetBytes_ = 64 * 1024 * 1024;
        const auto result = ValuePortfolioByMonteCarloWithJacobianRisk(data, 257, request, valuation, simulation);
        const auto reference = ValueByMonteCarloWithRisk(data->Products()[0], data->Models()[0], 257, {}, valuation, simulation);
        ASSERT_EQ(result.Values(), Vector_<double>(2, reference.Values()[0]));
        ASSERT_EQ(result.Jacobian().Rows(), 2);
        ASSERT_EQ(result.Jacobian().Cols(), 0);
        ASSERT_EQ(result.Execution().peakRecordingBytes_, 0);
        ASSERT_EQ(result.Execution().groups_[0].generatedScenarios_, 257);
        ASSERT_EQ(result.Execution().groups_[0].evaluatorCalls_, 514);
        ASSERT_TRUE(result.Execution().groups_[0].actualWidths_.empty());
        ASSERT_LE(result.Execution().peakScratchBytes_, *request.scratchCapacityBudgetBytes_);
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
        const double spotRisk = (result.componentMeans_[0] - 85.0) / 100.0;
        // The value and derivative use separate 257-path floating-point reductions.
        const double reductionError = 257 * std::numeric_limits<double>::epsilon() * std::max(1.0, std::abs(spotRisk));
        ASSERT_NEAR(result.gradient_[0], spotRisk, reductionError);
        ASSERT_DOUBLE_EQ(result.gradient_[1], 2.0);
        ASSERT_DOUBLE_EQ(result.gradient_[2], -1.0);
        ASSERT_EQ(result.groupCounters_[0].generatedScenarios_, 257);
        ASSERT_LE(result.peakScratchBytes_, *limits.scratchBudgetBytes_);
        ASSERT_LE(result.peakTapeBytes_, *limits.tapeBudgetBytes_);
        RiskRequest_ scalarRequest;
        scalarRequest.inputs_ = Vector_<String_>{"model:0", "constant:0"};
        const auto scalar = ValueByMonteCarloWithRisk(data->Products()[0], data->Models()[0], 257, scalarRequest, Valuation(), simulation);
        ASSERT_NEAR(result.gradient_[0], scalar.Jacobian()(0, 0), reductionError);
        ASSERT_DOUBLE_EQ(result.gradient_[1], 2.0 * scalar.Jacobian()(0, 1));
        ASSERT_DOUBLE_EQ(result.gradient_[2], -scalar.Jacobian()(0, 1));
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

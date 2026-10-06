//
// Created by Codex on 2026/10/6.
//

#include <gtest/gtest.h>

#include <limits>

#include <dal-public/src/models.hpp>
#include <dal-public/src/portfolioreplayinternal.hpp>
#include <dal-public/src/portfoliorisk.hpp>
#include <dal-public/src/value.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/simulation.hpp>

#include <script_test_observers.hpp>

using namespace Dal;
using namespace Dal::Script;

namespace {
    struct ScopedPortfolioThreads_ {
        ThreadPool_* pool_ = ThreadPool_::GetInstance();
        size_t threads_ = pool_->NumThreads();
        bool active_ = pool_->IsActive();
        explicit ScopedPortfolioThreads_(size_t threads) { pool_->Start(threads, true); }
        ~ScopedPortfolioThreads_() {
            pool_->Start(threads_, true);
            if (!active_)
                pool_->Stop();
        }
    };

    ScriptValuationSettings_ Valuation() {
        ScriptValuationSettings_ result;
        result.evaluationDate_ = Date_(2026, 1, 1);
        return result;
    }

    Handle_<ScriptProductData_> Trade(double constant, const String_& event, const Date_& date = Date_(2027, 1, 1)) {
        return Handle_<ScriptProductData_>(new ScriptProductData_("", {Cell_("X"), Cell_(date)}, {String_(std::to_string(constant)), event}));
    }

    Dal::Script::Detail::PortfolioBatchOutput_ Payoff(const Dal::Script::Detail::PreparedPortfolio_& portfolio, size_t trade, double weight) {
        auto output = ScriptRiskOutputAxis(portfolio.Trades()[trade].Product())[portfolio.Trades()[trade].PayOffIdx()];
        output.id_ = "trade:" + String_(std::to_string(trade)) + ":" + output.id_;
        return {trade, std::move(output), weight};
    }

    void AssertIndependentScalarRisk(const Handle_<ScriptPortfolioData_>& data,
                                     const PortfolioRiskAxes_& axes,
                                     const Vector_<size_t>& trades,
                                     const Vector_<double>& weights,
                                     const MonteCarloSettings_& simulation,
                                     const Dal::Detail::PortfolioWeightedReplayResult_& result) {
        double objective = 0.0;
        Vector_<double> gradient(axes.InputAxis().size(), 0.0);
        for (size_t row = 0; row < trades.size(); ++row) {
            const auto trade = trades[row];
            const auto reference =
                ValueByMonteCarloWithRisk(data->Products()[trade], data->Models()[data->ModelOwners()[trade]], 257, {}, Valuation(), simulation);
            ASSERT_NEAR(result.componentMeans_[row], reference.Values()[0], 1e-10);
            objective += weights[row] * reference.Values()[0];
            for (size_t local = 0; local < axes.TradeInputPositions()[trade].size(); ++local)
                gradient[axes.TradeInputPositions()[trade][local]] += weights[row] * reference.Jacobian()(0, static_cast<int>(local));
        }
        ASSERT_NEAR(result.weightedValue_, objective, 1e-10);
        for (size_t column = 0; column < gradient.size(); ++column)
            ASSERT_NEAR(result.gradient_[column], gradient[column], 1e-10) << axes.InputAxis()[column].id_;
    }
} // namespace

TEST(PortfolioReplayTest, TestSharedOwnershipScatterAndParallelReductionMatchOracle) {
    RegisterAll_::Init();
    const Handle_<ModelData_> model(new BSModelData_("", 100.0, 0.0));
    const Handle_<ScriptPortfolioData_> data(
        new ScriptPortfolioData_("", {{"A", Trade(5.0, "pay PAYS 2 * SPOT() + X"), model}, {"B", Trade(7.0, "pay PAYS 3 * SPOT() + X"), model}}));
    for (const size_t workers : {1, 4}) {
        const ScopedPortfolioThreads_ threads(workers);
        for (const bool compiled : {false, true}) {
            MonteCarloSettings_ simulation;
            simulation.enableAad_ = true;
            simulation.compiled_ = compiled;
            const auto prepared = Dal::Script::Detail::PrepareScriptPortfolio(data, 257, Valuation(), simulation);
            const auto result =
                Dal::Detail::EvaluatePortfolioWeightedReplay(prepared, {Payoff(prepared, 1, -1.0), Payoff(prepared, 0, 2.0)}, {5, 0, 4});
            ASSERT_DOUBLE_EQ(result.weightedValue_, 103.0);
            ASSERT_DOUBLE_EQ(result.componentMeans_[0], 307.0);
            ASSERT_DOUBLE_EQ(result.componentMeans_[1], 205.0);
            ASSERT_EQ(result.gradient_.size(), 3);
            ASSERT_DOUBLE_EQ(result.gradient_[0], -1.0);
            ASSERT_DOUBLE_EQ(result.gradient_[1], 1.0);
            ASSERT_DOUBLE_EQ(result.gradient_[2], 2.0);
            ASSERT_EQ(result.groupCounters_.size(), 1);
            ASSERT_EQ(result.groupCounters_[0].generatedScenarios_, 257);
            ASSERT_EQ(result.groupCounters_[0].evaluatorCalls_, 514);
            ASSERT_EQ(result.groupCounters_[0].suffixReversals_, 257);
            ASSERT_EQ(result.groupCounters_[0].prefixReversals_, workers);
        }
    }
}

TEST(PortfolioReplayTest, TestOriginalMeshesAndDistinctOwnersMatchIndependentScalarCalls) {
    RegisterAll_::Init();
    const Handle_<ModelData_> shared(new BSModelData_("same", 100.0, 0.23, 0.02, 0.01));
    const Handle_<ModelData_> distinct(new BSModelData_("same", 100.0, 0.23, 0.02, 0.01));
    const Handle_<ScriptPortfolioData_> data(new ScriptPortfolioData_("", {{"A", Trade(5.0, "pay PAYS MAX(SPOT() - 20 * X, 0)"), shared},
                                                                           {"B", Trade(7.0, "pay PAYS 3 * SPOT() + X", Date_(2027, 6, 3)), shared},
                                                                           {"C", Trade(11.0, "pay PAYS 2 * SPOT() + X"), shared},
                                                                           {"D", Trade(13.0, "pay PAYS SPOT() * X"), distinct}}));
    const auto axes = ScriptPortfolioRiskAxes(data);
    Vector_<size_t> inputs;
    for (size_t input = 0; input < axes.InputAxis().size(); ++input)
        inputs.push_back(input);
    const Vector_<size_t> trades{3, 2, 1, 0};
    const Vector_<double> weights{0.5, 2.0, -1.0, 3.0};
    for (const size_t workers : {1, 4}) {
        const ScopedPortfolioThreads_ threads(workers);
        for (const bool compiled : {false, true})
            for (const String_& rsg : Vector_<String_>{"sobol", "mrg32"}) {
                MonteCarloSettings_ simulation;
                simulation.enableAad_ = true;
                simulation.compiled_ = compiled;
                simulation.rsg_ = rsg;
                simulation.useBb_ = true;
                const auto prepared = Dal::Script::Detail::PrepareScriptPortfolio(data, 257, Valuation(), simulation);
                ASSERT_EQ(prepared.Groups().size(), 3);
                ASSERT_EQ(prepared.Groups()[0].tradePositions_, Vector_<size_t>({0, 2}));
                Vector_<Dal::Script::Detail::PortfolioBatchOutput_> outputs;
                for (size_t row = 0; row < trades.size(); ++row)
                    outputs.push_back(Payoff(prepared, trades[row], weights[row]));
                const auto result = Dal::Detail::EvaluatePortfolioWeightedReplay(prepared, outputs, inputs);
                ASSERT_NO_FATAL_FAILURE(AssertIndependentScalarRisk(data, axes, trades, weights, simulation, result));
                for (const auto& group : result.groupCounters_) {
                    ASSERT_EQ(group.generatedScenarios_, 257);
                    ASSERT_EQ(group.suffixReversals_, 257);
                    ASSERT_EQ(group.prefixReversals_, workers);
                }
                ASSERT_EQ(result.groupCounters_[0].evaluatorCalls_, 514);
                ASSERT_EQ(result.groupCounters_[1].evaluatorCalls_, 257);
                ASSERT_EQ(result.groupCounters_[2].evaluatorCalls_, 257);
            }
    }
}

TEST(PortfolioReplayTest, TestMalformedSelectionSubmitsNoTasks) {
    RegisterAll_::Init();
    const ScopedPortfolioThreads_ threads(4);
    const Handle_<ModelData_> model(new BSModelData_("", 100.0, 0.2));
    const Handle_<ScriptPortfolioData_> data(new ScriptPortfolioData_("", {{"A", Trade(5.0, "pay PAYS SPOT() + X"), model}}));
    MonteCarloSettings_ simulation;
    simulation.enableAad_ = true;
    const auto prepared = Dal::Script::Detail::PrepareScriptPortfolio(data, 257, Valuation(), simulation);
    const auto output = Payoff(prepared, 0, 1.0);
    auto wrongTrade = output;
    wrongTrade.tradePosition_ = std::numeric_limits<size_t>::max();
    auto wrongSlot = output;
    wrongSlot.coordinate_.slot_ = std::numeric_limits<size_t>::max();
    auto wrongId = output;
    wrongId.coordinate_.id_ = "trade:9:payoff";
    auto wrongLabel = output;
    wrongLabel.coordinate_.label_ = "changed";
    auto wrongWeight = output;
    wrongWeight.weight_ = std::numeric_limits<double>::infinity();
    Script::TestSupport::RejectSubmissions_ tasks;
    const Script::Detail::ScopedSimulationObserver_ observer(&tasks);
    for (const auto& outputs : Vector_<Vector_<Dal::Script::Detail::PortfolioBatchOutput_>>{
             {}, {output, output}, {wrongTrade}, {wrongSlot}, {wrongId}, {wrongLabel}, {wrongWeight}})
        ASSERT_THROW(static_cast<void>(Dal::Detail::EvaluatePortfolioWeightedReplay(prepared, outputs, {0})), ScriptError_);
    for (const auto& inputs : Vector_<Vector_<size_t>>{{0, 0}, {5}, {std::numeric_limits<size_t>::max()}})
        ASSERT_THROW(static_cast<void>(Dal::Detail::EvaluatePortfolioWeightedReplay(prepared, {output}, inputs)), ScriptError_);
    ASSERT_EQ(tasks.calls_, 0);
}

TEST(PortfolioReplayTest, TestUnselectedGroupIsSkippedAndEmptyInputsKeepNativePricing) {
    RegisterAll_::Init();
    const ScopedPortfolioThreads_ threads(4);
    const Handle_<ModelData_> model(new BSModelData_("", 100.0, 0.2));
    const Handle_<ModelData_> poison(new BSModelData_("", 1000.0, 0.0));
    const Handle_<ScriptPortfolioData_> data(
        new ScriptPortfolioData_("", {{"A", Trade(5.0, "pay PAYS SPOT() + X"), model}, {"B", Trade(7.0, "pay PAYS EXP(SPOT())"), poison}}));
    MonteCarloSettings_ simulation;
    simulation.enableAad_ = true;
    for (const bool compiled : {false, true}) {
        simulation.compiled_ = compiled;
        const auto prepared = Dal::Script::Detail::PrepareScriptPortfolio(data, 257, Valuation(), simulation);
        const auto risk = Dal::Detail::EvaluatePortfolioWeightedReplay(prepared, {Payoff(prepared, 0, 2.0)}, {0, 8});
        const auto price = Dal::Detail::EvaluatePortfolioWeightedReplay(prepared, {Payoff(prepared, 0, 2.0)}, {});
        ASSERT_EQ(price.componentMeans_, risk.componentMeans_);
        ASSERT_EQ(price.weightedValue_, risk.weightedValue_);
        ASSERT_TRUE(price.gradient_.empty());
        ASSERT_EQ(price.groupCounters_.size(), 2);
        ASSERT_EQ(price.groupCounters_[0].generatedScenarios_, 257);
        ASSERT_EQ(price.groupCounters_[0].evaluatorCalls_, 257);
        ASSERT_EQ(price.groupCounters_[1].generatedScenarios_, 0);
        ASSERT_EQ(price.groupCounters_[1].evaluatorCalls_, 0);
        ASSERT_EQ(price.groupCounters_[1].prefixReversals_, 0);
    }
}

TEST(PortfolioReplayTest, TestPartialSubmissionAndWorkerFailureDrainAndRecover) {
    RegisterAll_::Init();
    const ScopedPortfolioThreads_ threads(4);
    const Handle_<ModelData_> model(new BSModelData_("", 100.0, 0.2));
    const Handle_<ModelData_> poison(new BSModelData_("", 1000.0, 0.0));
    const Handle_<ScriptPortfolioData_> data(
        new ScriptPortfolioData_("", {{"A", Trade(5.0, "pay PAYS SPOT() + X"), model}, {"B", Trade(7.0, "pay PAYS EXP(SPOT())"), poison}}));
    MonteCarloSettings_ simulation;
    simulation.enableAad_ = true;
    for (const bool compiled : {false, true}) {
        simulation.compiled_ = compiled;
        const auto prepared = Dal::Script::Detail::PrepareScriptPortfolio(data, 257, Valuation(), simulation);
        const auto output = Payoff(prepared, 0, 1.0);
        const auto prior = Dal::Detail::EvaluatePortfolioWeightedReplay(prepared, {output}, {0, 8});
        {
            Script::TestSupport::RejectSubmissions_ tasks("injected portfolio submission failure");
            const Script::Detail::ScopedSimulationObserver_ observer(&tasks);
            ASSERT_THROW(static_cast<void>(Dal::Detail::EvaluatePortfolioWeightedReplay(prepared, {output}, {0, 8})), ScriptError_);
            ASSERT_EQ(tasks.calls_, 1);
        }
        ASSERT_THROW(static_cast<void>(Dal::Detail::EvaluatePortfolioWeightedReplay(prepared, {output, Payoff(prepared, 1, 0.0)}, {0, 8})),
                     ScriptError_);
        const auto recovered = Dal::Detail::EvaluatePortfolioWeightedReplay(prepared, {output}, {0, 8});
        ASSERT_EQ(recovered.weightedValue_, prior.weightedValue_);
        ASSERT_EQ(recovered.componentMeans_, prior.componentMeans_);
        ASSERT_EQ(recovered.gradient_, prior.gradient_);
    }
}

TEST(PortfolioReplayTest, TestOnlyRequiredDerivativesAreValidatedAndRecoveryPreservesResults) {
    RegisterAll_::Init();
    const ScopedPortfolioThreads_ threads(1);
    const Handle_<ModelData_> model(new BSModelData_("", 709.0, 0.0));
    const Handle_<ScriptPortfolioData_> data(new ScriptPortfolioData_("", {{"A", Trade(5.0, "pay PAYS EXP(SPOT())"), model}}));
    MonteCarloSettings_ simulation;
    simulation.enableAad_ = true;
    for (const bool compiled : {false, true}) {
        simulation.compiled_ = compiled;
        const auto prepared = Dal::Script::Detail::PrepareScriptPortfolio(data, 1, Valuation(), simulation);
        const auto output = Payoff(prepared, 0, 1.0);
        const auto prior = Dal::Detail::EvaluatePortfolioWeightedReplay(prepared, {output}, {});
        ASSERT_TRUE(std::isfinite(prior.weightedValue_));
        ASSERT_TRUE(prior.gradient_.empty());
        try {
            static_cast<void>(Dal::Detail::EvaluatePortfolioWeightedReplay(prepared, {output}, {2}));
            FAIL() << "the required rate derivative must reject overflow";
        } catch (const ScriptError_& error) {
            ASSERT_NE(String_(error.what()).find("group=0"), String_::npos);
            ASSERT_NE(String_(error.what()).find("model:0:parameter:2"), String_::npos);
        }
        const auto recovered = Dal::Detail::EvaluatePortfolioWeightedReplay(prepared, {output}, {});
        ASSERT_EQ(recovered.weightedValue_, prior.weightedValue_);
        ASSERT_EQ(recovered.componentMeans_, prior.componentMeans_);
    }
}

TEST(PortfolioReplayTest, TestSixModelFamiliesShareOriginalPathsAndMatchEveryIndependentRisk) {
    RegisterAll_::Init();
    const Date_ today(2026, 1, 1);
    HybridSettings_ hybridSettings;
    hybridSettings.domesticCurrency_ = "USD";
    hybridSettings.components_ = {NewHybridBSEquityData("A", "EQ[A]", "USD", "FA", 100.0, 0.2, 0.0),
                                  NewHybridDeterministicRateData("RATE", "USD", 0.03)};
    hybridSettings.correlation_ = NewHybridConstantCorrelationData("correlation", {"FA"}, Matrix_<>(1, 1, 1.0));
    const auto curve = NewGSRCurveData("curve", today, "USD", {today, today.AddDays(1095)}, {0.0, -0.09}, {}, Matrix_<>(0, 0));
    const auto gsr = NewGSRModelData("gsr", curve, NewGSRVolData("vol", {today}, {0.02}, {today}, {1.0}));
    MultiFactorGSRVolSettings_ multiSettings;
    multiSettings.factorNames_ = {"level"};
    multiSettings.gKnotDates_ = multiSettings.hKnotDates_ = {today};
    multiSettings.gValues_ = Matrix_<>(1, 1, 0.02);
    multiSettings.hValues_ = multiSettings.correlations_ = Matrix_<>(1, 1, 1.0);
    const auto multi = NewMultiFactorGSRModelData("multi", curve, NewMultiFactorGSRVolData("vol", multiSettings));
    GSRSLVSettings_ slvSettings;
    slvSettings.maxStep_ = 0.25;
    const auto slv = NewGSRSLVModelData("slv", multi, NewGSRLeverageData("leverage", {-0.02, 0.02}, {0.0}, Matrix_<>(2, 1, 1.0)), slvSettings);
    const Vector_<Handle_<ModelData_>> families{NewBSModelData("bs", 100.0, 0.2, 0.03, 0.0),
                                                NewCorrelatedBSModelData("correlated", {"EQ[A]"}, {100.0}, {0.2}, {0.0}, 0.03, Matrix_<>(1, 1, 1.0)),
                                                NewHybridModelData("hybrid", hybridSettings),
                                                gsr,
                                                multi,
                                                slv};
    const Vector_<String_> observations{
        "SPOT()", "FIX(EQ[A])", "FIX(EQ[A])", "FIX(IR[USD,DF,2028-01-01])", "FIX(IR[USD,DF,2028-01-01])", "FIX(IR[USD,DF,2028-01-01])"};
    for (size_t family = 0; family < families.size(); ++family) {
        SCOPED_TRACE(families[family]->Type());
        const auto expression = observations[family];
        const Handle_<ScriptPortfolioData_> data(
            new ScriptPortfolioData_("", {{"A", Trade(5.0, "pay PAYS MAX(" + expression + " - X, 0)"), families[family]},
                                          {"B", Trade(7.0, "pay PAYS 3 * " + expression + " + X"), families[family]}}));
        const auto axes = ScriptPortfolioRiskAxes(data);
        Vector_<size_t> inputs;
        for (size_t input = 0; input < axes.InputAxis().size(); ++input)
            inputs.push_back(input);
        for (const size_t workers : {1, 4}) {
            const ScopedPortfolioThreads_ threads(workers);
            for (const bool compiled : {false, true}) {
                MonteCarloSettings_ simulation;
                simulation.enableAad_ = true;
                simulation.compiled_ = compiled;
                const auto prepared = Dal::Script::Detail::PrepareScriptPortfolio(data, 257, Valuation(), simulation);
                ASSERT_EQ(prepared.Groups().size(), 1);
                const auto result =
                    Dal::Detail::EvaluatePortfolioWeightedReplay(prepared, {Payoff(prepared, 1, -1.0), Payoff(prepared, 0, 2.0)}, inputs);
                const auto first = ValueByMonteCarloWithRisk(data->Products()[0], data->Models()[0], 257, {}, Valuation(), simulation);
                const auto second = ValueByMonteCarloWithRisk(data->Products()[1], data->Models()[0], 257, {}, Valuation(), simulation);
                ASSERT_NEAR(result.componentMeans_[0], second.Values()[0], 1e-10);
                ASSERT_NEAR(result.componentMeans_[1], first.Values()[0], 1e-10);
                ASSERT_NEAR(result.weightedValue_, 2 * first.Values()[0] - second.Values()[0], 1e-10);
                Vector_<double> gradient(inputs.size(), 0.0);
                for (size_t input = 0; input < first.InputAxis().size(); ++input)
                    gradient[axes.TradeInputPositions()[0][input]] += 2 * first.Jacobian()(0, static_cast<int>(input));
                for (size_t input = 0; input < second.InputAxis().size(); ++input)
                    gradient[axes.TradeInputPositions()[1][input]] -= second.Jacobian()(0, static_cast<int>(input));
                for (size_t input = 0; input < inputs.size(); ++input)
                    ASSERT_NEAR(result.gradient_[input], gradient[input], 1e-10) << axes.InputAxis()[input].id_;
                ASSERT_EQ(result.groupCounters_[0].generatedScenarios_, 257);
                ASSERT_EQ(result.groupCounters_[0].evaluatorCalls_, 514);
            }
        }
    }
}

TEST(PortfolioReplayTest, TestFiniteAggregateBudgetsRetainRiskAndReportActualPeaks) {
    RegisterAll_::Init();
    const Handle_<ModelData_> model(new BSModelData_("", 100.0, 0.2));
    const Handle_<ScriptPortfolioData_> data(
        new ScriptPortfolioData_("", {{"A", Trade(5.0, "pay PAYS SPOT() + X"), model}, {"B", Trade(7.0, "pay PAYS 3 * SPOT() + X"), model}}));
    for (const size_t workers : {1, 4}) {
        const ScopedPortfolioThreads_ threads(workers);
        for (const bool compiled : {false, true}) {
            MonteCarloSettings_ simulation;
            simulation.enableAad_ = true;
            simulation.compiled_ = compiled;
            const auto prepared = Dal::Script::Detail::PrepareScriptPortfolio(data, 257, Valuation(), simulation);
            const Vector_<Dal::Script::Detail::PortfolioBatchOutput_> outputs{Payoff(prepared, 1, -1.0), Payoff(prepared, 0, 2.0)};
            const auto prior = Dal::Detail::EvaluatePortfolioWeightedReplay(prepared, outputs, {5, 0, 4});
            ASSERT_GT(prior.peakScratchBytes_, 0);
            ASSERT_GT(prior.peakTapeBytes_, 0);
            Dal::Script::Detail::PortfolioCapacityLimits_ limits;
            limits.scratchBudgetBytes_ = prior.peakScratchBytes_ + 2 * 1024 * 1024;
            limits.tapeBudgetBytes_ = prior.peakTapeBytes_ + 2 * workers * AAD::TapeCleanupCapacityBytes();
            const auto bounded = Dal::Detail::EvaluatePortfolioWeightedReplay(prepared, outputs, {5, 0, 4}, limits);
            ASSERT_EQ(bounded.componentMeans_, prior.componentMeans_);
            ASSERT_EQ(bounded.weightedValue_, prior.weightedValue_);
            ASSERT_EQ(bounded.gradient_, prior.gradient_);
            ASSERT_LE(bounded.peakScratchBytes_, *limits.scratchBudgetBytes_);
            ASSERT_LE(bounded.peakTapeBytes_, *limits.tapeBudgetBytes_);
        }
    }
}

TEST(PortfolioReplayTest, TestBudgetRejectionAndPrivateEvaluatorExhaustionDrainAndRecover) {
    RegisterAll_::Init();
    const ScopedPortfolioThreads_ threads(1);
    const Handle_<ModelData_> model(new BSModelData_("", 100.0, 0.2));
    const Handle_<ScriptPortfolioData_> data(new ScriptPortfolioData_("", {{"A", Trade(5.0, "pay PAYS SPOT() + X"), model}}));
    const Handle_<ScriptPortfolioData_> large(new ScriptPortfolioData_(
        "", {{"A", Trade(5.0, "v[4095] = X pay PAYS SPOT() + SUM(v)"), model}, {"B", Trade(7.0, "v[4095] = X pay PAYS SPOT() + SUM(v)"), model}}));
    for (const bool compiled : {false, true}) {
        MonteCarloSettings_ simulation;
        simulation.enableAad_ = true;
        simulation.compiled_ = compiled;
        const auto prepared = Dal::Script::Detail::PrepareScriptPortfolio(data, 257, Valuation(), simulation);
        const auto big = Dal::Script::Detail::PrepareScriptPortfolio(large, 257, Valuation(), simulation);
        const auto output = Payoff(prepared, 0, 1.0);
        const auto prior = Dal::Detail::EvaluatePortfolioWeightedReplay(prepared, {output}, {0, 4});
        Dal::Script::Detail::PortfolioCapacityLimits_ limits;
        {
            Script::TestSupport::RejectSubmissions_ tasks;
            const Script::Detail::ScopedSimulationObserver_ observer(&tasks);
            limits.scratchBudgetBytes_ = 0;
            ASSERT_THROW(static_cast<void>(Dal::Detail::EvaluatePortfolioWeightedReplay(prepared, {output}, {0, 4}, limits)), Exception_);
            limits.scratchBudgetBytes_.reset();
            limits.tapeBudgetBytes_ = 0;
            ASSERT_THROW(static_cast<void>(Dal::Detail::EvaluatePortfolioWeightedReplay(prepared, {output}, {0, 4}, limits)), Exception_);
            ASSERT_EQ(tasks.calls_, 0);
        }
        limits.tapeBudgetBytes_.reset();
        limits.scratchBudgetBytes_ = prior.peakScratchBytes_ + 32768;
        try {
            static_cast<void>(Dal::Detail::EvaluatePortfolioWeightedReplay(big, {Payoff(big, 0, 1.0), Payoff(big, 1, 1.0)}, {0}, limits));
            FAIL() << "private vector evaluator capacities must be admitted";
        } catch (const Exception_& error) {
            ASSERT_NE(String_(error.what()).find("Scratch buffer capacity budget exceeded"), String_::npos);
            ASSERT_NE(String_(error.what()).find("group=0"), String_::npos);
        }
        const auto recovered = Dal::Detail::EvaluatePortfolioWeightedReplay(prepared, {output}, {0, 4});
        ASSERT_EQ(recovered.weightedValue_, prior.weightedValue_);
        ASSERT_EQ(recovered.gradient_, prior.gradient_);
    }
}

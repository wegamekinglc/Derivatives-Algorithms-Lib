//
// Created by Codex on 2026/10/7.
//

#include <gtest/gtest.h>

#include <algorithm>
#include <limits>

#include <dal-public/src/value.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/platform/platform.hpp>
#include <dal/storage/globals.hpp>

#include <script_test_observers.hpp>

using namespace Dal;
using namespace Dal::Script;

namespace {
    ScriptValuationSettings_ Valuation() {
        ScriptValuationSettings_ settings;
        settings.evaluationDate_ = Date_(2026, 1, 1);
        return settings;
    }

    Handle_<ScriptPortfolioData_> Portfolio() {
        const Handle_<ModelData_> model(new BSModelData_("", 100.0, 0.0));
        const Handle_<ScriptProductData_> first(
            new ScriptProductData_("", {Cell_("X"), Cell_(Date_(2027, 1, 1))}, {"5", "a = X pay PAYS 2 * SPOT() + X"}));
        const Handle_<ScriptProductData_> second(
            new ScriptProductData_("", {Cell_("X"), Cell_(Date_(2027, 1, 1))}, {"7", "a = X pay PAYS 3 * SPOT() + X"}));
        return Handle_<ScriptPortfolioData_>(new ScriptPortfolioData_("", {{"A", first, model}, {"B", second, model}}));
    }

    Handle_<ScriptPortfolioData_> HistoricalPortfolio() {
        const Handle_<ModelData_> model(new BSModelData_("", 100.0, 0.0));
        const Handle_<ScriptProductData_> product(new ScriptProductData_("", {Cell_("X"), Cell_(Date_(2025, 1, 1)), Cell_(Date_(2027, 1, 1))},
                                                                         {"5", "h = FIX(EQ[PORTFOLIO_PUBLIC_HISTORY])", "pay PAYS SPOT() + h + X"},
                                                                         ScriptProductSettings_{"EQ[MODEL]", {}}));
        return Handle_<ScriptPortfolioData_>(new ScriptPortfolioData_("", {{"A", product, model}, {"B", product, model}}));
    }

    struct ChangeOriginalRequestAfterFreeze_ : Script::TestSupport::FixingReadCounter_ {
        PortfolioWeightedRiskRequest_* request_;
        ScriptValuationSettings_* valuation_;
        MonteCarloSettings_* simulation_;

        ChangeOriginalRequestAfterFreeze_(PortfolioWeightedRiskRequest_* request,
                                          ScriptValuationSettings_* valuation,
                                          MonteCarloSettings_* simulation)
            : request_(request), valuation_(valuation), simulation_(simulation) {}
        void BeforeFixing(const Index_&, const Environment_*, const DateTime_&) override {
            if (fixings_++ != 0)
                return;
            request_->selection_.inputs_->clear();
            request_->selection_.outputs_->clear();
            (*request_->weights_)[0] = 999.0;
            valuation_->evaluationDate_ = Date_(2030, 1, 1);
            simulation_->rsg_ = "changed-invalid-generator";
            simulation_->enableAad_ = false;
            Script::TestSupport::StoreScriptTestFixing("EQ[PORTFOLIO_PUBLIC_HISTORY]", 999.0, DateTime_(Date_(2025, 1, 1), 0.0));
        }
    };

    void AssertRejectedBeforeHistory(const Handle_<ScriptPortfolioData_>& data,
                                     const PortfolioWeightedRiskRequest_& request,
                                     int paths = 257,
                                     bool native = true) {
        Script::TestSupport::RejectFixingReads_ reads;
        Script::TestSupport::RejectSubmissions_ tasks;
        const Dal::Detail::ScopedFixingReadObserver_ observeReads(&reads);
        const Dal::Script::Detail::ScopedSimulationObserver_ observeTasks(&tasks);
        auto simulation = DefaultRiskMonteCarloSettings();
        simulation.enableAad_ = native;
        ASSERT_THROW(static_cast<void>(ValuePortfolioByMonteCarloWithWeightedRisk(data, paths, request, Valuation(), simulation)), Exception_);
        ASSERT_EQ(reads.historyCalls_, 0);
        ASSERT_EQ(reads.fixingCalls_, 0);
        ASSERT_EQ(tasks.calls_, 0);
    }
} // namespace

TEST(PortfolioRiskValueTest, TestNativeWeightedResultOwnsAxesReportsAndResolvedGroupProvenance) {
    RegisterAll_::Init();
    auto data = Portfolio();
    PortfolioWeightedRiskRequest_ request;
    request.selection_.outputs_ = Vector_<String_>{"trade:1:payoff", "trade:0:payoff"};
    request.selection_.inputs_ = Vector_<String_>{"trade:1:constant:0", "model:0:parameter:0", "trade:0:constant:0"};
    request.selection_.reportFactors_ = Vector_<>{2.0, 0.5, 3.0};
    request.weights_ = Vector_<>{-1.0, 2.0};
    request.selection_.numericPayloadBudgetBytes_ = 8 * sizeof(double);
    request.scratchCapacityBudgetBytes_ = 64 * 1024 * 1024;
    request.recordingCapacityBudgetBytes_ = 256 * 1024 * 1024;
    const auto result = ValuePortfolioByMonteCarloWithWeightedRisk(data, 257, request, Valuation());
    request.selection_.inputs_->clear();
    (*request.weights_)[0] = 999.0;
    data.reset();
    ASSERT_DOUBLE_EQ(result.WeightedValue(), 103.0);
    ASSERT_DOUBLE_EQ(result.ComponentMeans()[0], 307.0);
    ASSERT_DOUBLE_EQ(result.ComponentMeans()[1], 205.0);
    ASSERT_EQ(result.Weights(), Vector_<double>({-1.0, 2.0}));
    ASSERT_EQ(result.OutputAxis()[0].id_, "trade:1:payoff");
    ASSERT_EQ(result.InputAxis()[0].id_, "trade:1:constant:0");
    ASSERT_EQ(result.CompleteInputAxis().size(), 6);
    ASSERT_EQ(result.CompleteOutputAxis().size(), 4);
    ASSERT_DOUBLE_EQ(result.Jacobian()(0, 0), -1.0);
    ASSERT_DOUBLE_EQ(result.Jacobian()(0, 1), 1.0);
    ASSERT_DOUBLE_EQ(result.Jacobian()(0, 2), 2.0);
    const auto reported = result.ReportedJacobian();
    ASSERT_DOUBLE_EQ(reported(0, 0), -2.0);
    ASSERT_DOUBLE_EQ(reported(0, 1), 0.5);
    ASSERT_DOUBLE_EQ(reported(0, 2), 6.0);
    ASSERT_EQ(result.CompleteInputAxis()[0].reportScale_, 1.0);
    ASSERT_EQ(result.Provenance().tradeIds_, Vector_<String_>({"A", "B"}));
    ASSERT_EQ(result.Provenance().modelOwners_, Vector_<int>({0, 0}));
    ASSERT_EQ(result.Provenance().evaluationDate_, Date_(2026, 1, 1));
    ASSERT_EQ(result.Provenance().method_, "NativeAADWeightedPortfolio");
    ASSERT_EQ(result.Provenance().trades_.size(), 2);
    ASSERT_EQ(result.Provenance().trades_[0].execution_->productEvents_[1], "a = X pay PAYS 2 * SPOT() + X");
    ASSERT_EQ(result.Execution().groups_.size(), 1);
    const auto& group = result.Execution().groups_[0];
    ASSERT_EQ(group.modelOwner_, 0);
    ASSERT_EQ(group.tradePositions_, Vector_<size_t>({0, 1}));
    ASSERT_EQ(group.randomDimension_, 1);
    ASSERT_EQ(group.factors_, 1);
    ASSERT_EQ(group.sampleDates_, Vector_<Date_>({Date_(2027, 1, 1)}));
    ASSERT_EQ(group.sampleDefinitions_.size(), 1);
    ASSERT_EQ(group.generatedScenarios_, 257);
    ASSERT_EQ(group.evaluatorCalls_, 514);
    ASSERT_EQ(group.suffixReversals_, 257);
    ASSERT_GT(result.Execution().peakRecordingBytes_, 0);
    ASSERT_GT(result.Execution().peakScratchBytes_, 0);
    auto detached = result.Jacobian();
    detached(0, 0) = 999;
    ASSERT_DOUBLE_EQ(result.Jacobian()(0, 0), -1.0);
}

TEST(PortfolioRiskValueTest, TestEmptyNativeColumnsAndReportOverflowPreservePriorResults) {
    RegisterAll_::Init();
    const auto data = Portfolio();
    PortfolioWeightedRiskRequest_ empty;
    empty.selection_.inputs_ = Vector_<String_>{};
    empty.selection_.numericPayloadBudgetBytes_ = 5 * sizeof(double);
    const auto prior = ValuePortfolioByMonteCarloWithWeightedRisk(data, 257, empty, Valuation());
    ASSERT_EQ(prior.Jacobian().Rows(), 1);
    ASSERT_EQ(prior.Jacobian().Cols(), 0);
    ASSERT_EQ(prior.Provenance().engine_, "native");
    ASSERT_EQ(prior.Execution().groups_[0].suffixReversals_, 257);
    PortfolioWeightedRiskRequest_ overflow;
    overflow.selection_.outputs_ = Vector_<String_>{"trade:0:output:0"};
    overflow.selection_.inputs_ = Vector_<String_>{"trade:0:constant:0"};
    overflow.selection_.reportFactors_ = Vector_<>{std::numeric_limits<double>::max()};
    overflow.weights_ = Vector_<>{10.0};
    try {
        static_cast<void>(ValuePortfolioByMonteCarloWithWeightedRisk(data, 257, overflow, Valuation()));
        FAIL() << "report overflow must fail before publishing the result";
    } catch (const ScriptError_& error) {
        ASSERT_NE(String_(error.what()).find("trade:0:constant:0"), String_::npos);
        ASSERT_NE(String_(error.what()).find("report"), String_::npos);
        ASSERT_NE(String_(error.what()).find("trade=A"), String_::npos);
        ASSERT_NE(String_(error.what()).find("group=0"), String_::npos);
    }
    const auto recovered = ValuePortfolioByMonteCarloWithWeightedRisk(data, 257, empty, Valuation());
    ASSERT_EQ(recovered.ComponentMeans(), prior.ComponentMeans());
    ASSERT_EQ(recovered.WeightedValue(), prior.WeightedValue());
}

TEST(PortfolioRiskValueTest, TestPublicInvalidRequestsAndCapacitiesFailBeforeHistoryOrTasks) {
    RegisterAll_::Init();
    const auto data = HistoricalPortfolio();
    ASSERT_NO_FATAL_FAILURE(AssertRejectedBeforeHistory({}, {}));
    ASSERT_NO_FATAL_FAILURE(AssertRejectedBeforeHistory(data, {}, 0));
    ASSERT_NO_FATAL_FAILURE(AssertRejectedBeforeHistory(data, {}, -1));
    PortfolioWeightedRiskRequest_ request;
    request.selection_.outputs_ = Vector_<String_>{};
    ASSERT_NO_FATAL_FAILURE(AssertRejectedBeforeHistory(data, request));
    request.selection_.outputs_ = Vector_<String_>{"trade:0:payoff", "TRADE:0:PAYOFF"};
    ASSERT_NO_FATAL_FAILURE(AssertRejectedBeforeHistory(data, request));
    request.selection_.outputs_.reset();
    request.weights_ = Vector_<>{1.0};
    ASSERT_NO_FATAL_FAILURE(AssertRejectedBeforeHistory(data, request));
    request.weights_ = Vector_<>{0.0, std::numeric_limits<double>::infinity()};
    ASSERT_NO_FATAL_FAILURE(AssertRejectedBeforeHistory(data, request));
    request.weights_.reset();
    request.selection_.inputs_ = Vector_<String_>{"model:9:parameter:0"};
    ASSERT_NO_FATAL_FAILURE(AssertRejectedBeforeHistory(data, request));
    request.selection_.inputs_.reset();
    request.selection_.numericPayloadBudgetBytes_ = 0;
    ASSERT_NO_FATAL_FAILURE(AssertRejectedBeforeHistory(data, request));
    request.selection_.numericPayloadBudgetBytes_.reset();
    request.scratchCapacityBudgetBytes_ = 0;
    ASSERT_NO_FATAL_FAILURE(AssertRejectedBeforeHistory(data, request));
    request.scratchCapacityBudgetBytes_.reset();
    request.recordingCapacityBudgetBytes_ = 0;
    ASSERT_NO_FATAL_FAILURE(AssertRejectedBeforeHistory(data, request));
}

TEST(PortfolioRiskValueTest, TestFrozenHistorySettingsAndOriginalRequestOwnTheirProvenance) {
    RegisterAll_::Init();
    const auto data = HistoricalPortfolio();
    Script::TestSupport::StoreScriptTestFixing("EQ[PORTFOLIO_PUBLIC_HISTORY]", 80.0, DateTime_(Date_(2025, 1, 1), 0.0));
    PortfolioWeightedRiskRequest_ request;
    request.selection_.outputs_ = Vector_<String_>{"trade:0:payoff", "trade:1:payoff"};
    request.selection_.inputs_ = Vector_<String_>{"model:0:parameter:0", "trade:0:constant:0", "trade:1:constant:0"};
    request.weights_ = Vector_<>{2.0, -1.0};
    auto valuation = Valuation();
    auto simulation = DefaultRiskMonteCarloSettings();
    ChangeOriginalRequestAfterFreeze_ reads(&request, &valuation, &simulation);
    const Dal::Detail::ScopedFixingReadObserver_ observeReads(&reads);
    const auto result = ValuePortfolioByMonteCarloWithWeightedRisk(data, 257, request, valuation, simulation);
    ASSERT_EQ(reads.histories_, 1);
    ASSERT_DOUBLE_EQ(result.WeightedValue(), 185.0);
    ASSERT_DOUBLE_EQ(result.Jacobian()(0, 0), 1.0);
    ASSERT_DOUBLE_EQ(result.Jacobian()(0, 1), 2.0);
    ASSERT_DOUBLE_EQ(result.Jacobian()(0, 2), -1.0);
    ASSERT_EQ(result.Weights(), Vector_<double>({2.0, -1.0}));
    ASSERT_EQ(result.Provenance().evaluationDate_, Date_(2026, 1, 1));
    const auto& snapshot = *result.Provenance().trades_[0].execution_;
    ASSERT_EQ(snapshot.fixingSource_, "GlobalSnapshot");
    ASSERT_EQ(snapshot.simulation_.rsg_, "sobol");
    ASSERT_TRUE(snapshot.simulation_.enableAad_);
    const auto historical =
        std::find_if(snapshot.observations_.begin(), snapshot.observations_.end(), [](const auto& observation) { return observation.historical_; });
    ASSERT_NE(historical, snapshot.observations_.end());
    ASSERT_EQ(historical->index_, "EQ[PORTFOLIO_PUBLIC_HISTORY]");
    ASSERT_EQ(historical->value_, std::optional<double>(80.0));
    Script::TestSupport::RejectFixingReads_ forbidReads;
    Script::TestSupport::RejectSubmissions_ forbidTasks;
    const Dal::Detail::ScopedFixingReadObserver_ forbidHistory(&forbidReads);
    const Dal::Script::Detail::ScopedSimulationObserver_ forbidWork(&forbidTasks);
    ASSERT_EQ(result.ReportedJacobian().Cols(), 3);
    ASSERT_EQ(result.CompleteOutputAxis().size(), 4);
    ASSERT_EQ(result.Execution().groups_[0].simulation_.rsg_, "sobol");
    ASSERT_EQ(forbidReads.historyCalls_, 0);
    ASSERT_EQ(forbidTasks.calls_, 0);
}

TEST(PortfolioRiskValueTest, TestPassiveSharedPathsMatchSharpScalarPricesAndNativeEmptyKeepsSmoothing) {
    RegisterAll_::Init();
    const Handle_<ModelData_> model(new BSModelData_("", 100.0, 0.0));
    const Handle_<ScriptProductData_> product(
        new ScriptProductData_("", {Cell_(Date_(2027, 1, 1))}, {"IF SPOT() > 100 THEN pay PAYS 1 ELSE pay PAYS 0 END"}));
    const Handle_<ScriptPortfolioData_> data(new ScriptPortfolioData_("", {{"A", product, model}, {"B", product, model}}));
    PortfolioWeightedRiskRequest_ request;
    request.selection_.inputs_ = Vector_<String_>{};
    request.weights_ = Vector_<>{2.0, -1.0};
    request.selection_.numericPayloadBudgetBytes_ = 5 * sizeof(double);
    request.scratchCapacityBudgetBytes_ = 64 * 1024 * 1024;
    for (const bool compiled : {false, true}) {
        auto simulation = DefaultRiskMonteCarloSettings();
        simulation.compiled_ = compiled;
        const auto native = ValuePortfolioByMonteCarloWithWeightedRisk(data, 257, request, Valuation(), simulation);
        const auto nativeScalar = ValueByMonteCarloWithRisk(product, model, 257, {Vector_<String_>{}}, Valuation(), simulation);
        ASSERT_DOUBLE_EQ(native.WeightedValue(), nativeScalar.Values()[0]);
        ASSERT_EQ(native.Execution().groups_[0].suffixReversals_, 257);
        simulation.enableAad_ = false;
        request.recordingCapacityBudgetBytes_ = 0;
        const auto passive = ValuePortfolioByMonteCarloWithWeightedRisk(data, 257, request, Valuation(), simulation);
        const auto passiveScalar = ValueByMonteCarloWithRisk(product, model, 257, {}, Valuation(), simulation);
        ASSERT_DOUBLE_EQ(passive.WeightedValue(), passiveScalar.Values()[0]);
        ASSERT_DOUBLE_EQ(passive.ComponentMeans()[0], passiveScalar.Values()[0]);
        ASSERT_DOUBLE_EQ(passive.ComponentMeans()[1], passiveScalar.Values()[0]);
        ASSERT_NE(native.WeightedValue(), passive.WeightedValue());
        ASSERT_EQ(passive.Jacobian().Rows(), 1);
        ASSERT_EQ(passive.Jacobian().Cols(), 0);
        ASSERT_EQ(passive.CompleteInputAxis().size(), 4);
        ASSERT_EQ(passive.Provenance().method_, "PriceOnlyWeightedPortfolio");
        ASSERT_EQ(passive.Provenance().engine_, "passive");
        ASSERT_EQ(passive.Provenance().trades_[0].method_, "PriceOnly");
        ASSERT_EQ(passive.Provenance().trades_[0].engine_, "passive");
        ASSERT_EQ(passive.Execution().peakRecordingBytes_, 0);
        const auto& group = passive.Execution().groups_[0];
        ASSERT_EQ(group.generatedScenarios_, 257);
        ASSERT_EQ(group.evaluatorCalls_, 514);
        ASSERT_EQ(group.suffixReversals_, 0);
        ASSERT_EQ(group.prefixReversals_, 0);
        request.recordingCapacityBudgetBytes_.reset();
    }
}

TEST(PortfolioRiskValueTest, TestPassiveSelectionAndFailureRecoveryRetainOutputAndTradeContext) {
    RegisterAll_::Init();
    const auto historical = HistoricalPortfolio();
    PortfolioWeightedRiskRequest_ invalid;
    invalid.selection_.inputs_ = Vector_<String_>{"model:0:parameter:0"};
    ASSERT_NO_FATAL_FAILURE(AssertRejectedBeforeHistory(historical, invalid, 257, false));
    invalid.selection_.inputs_.reset();
    invalid.selection_.reportFactors_ = Vector_<>{2.0};
    ASSERT_NO_FATAL_FAILURE(AssertRejectedBeforeHistory(historical, invalid, 257, false));
    invalid.selection_.reportFactors_.reset();
    invalid.scratchCapacityBudgetBytes_ = 0;
    ASSERT_NO_FATAL_FAILURE(AssertRejectedBeforeHistory(historical, invalid, 257, false));
    const Handle_<ModelData_> model(new BSModelData_("", 100.0, 0.0));
    const Handle_<ScriptProductData_> good(new ScriptProductData_("", {Cell_(Date_(2027, 1, 1))}, {"pay PAYS SPOT()"}));
    const Handle_<ScriptProductData_> bad(new ScriptProductData_("", {Cell_(Date_(2027, 1, 1))}, {"pay PAYS LOG(-SPOT())"}));
    const Handle_<ScriptPortfolioData_> data(new ScriptPortfolioData_("", {{"Good", good, model}, {"Bad", bad, model}}));
    auto simulation = DefaultRiskMonteCarloSettings();
    simulation.enableAad_ = false;
    PortfolioWeightedRiskRequest_ selected;
    selected.selection_.outputs_ = Vector_<String_>{"trade:0:payoff"};
    PortfolioWeightedRiskRequest_ all;
    all.weights_ = Vector_<>{1.0, 0.0};
    for (const bool compiled : {false, true}) {
        simulation.compiled_ = compiled;
        const auto prior = ValuePortfolioByMonteCarloWithWeightedRisk(data, 257, selected, Valuation(), simulation);
        ASSERT_DOUBLE_EQ(prior.WeightedValue(), 100.0);
        ASSERT_EQ(prior.Execution().groups_[0].evaluatorCalls_, 257);
        try {
            static_cast<void>(ValuePortfolioByMonteCarloWithWeightedRisk(data, 257, all, Valuation(), simulation));
            FAIL() << "zero weight cannot hide a non-finite selected output";
        } catch (const ScriptError_& error) {
            const String_ message(error.what());
            ASSERT_NE(message.find("group=0"), String_::npos);
            ASSERT_NE(message.find("trade=Bad"), String_::npos);
            ASSERT_NE(message.find("trade:1:payoff"), String_::npos);
        }
        const auto recovered = ValuePortfolioByMonteCarloWithWeightedRisk(data, 257, selected, Valuation(), simulation);
        ASSERT_EQ(recovered.WeightedValue(), prior.WeightedValue());
        ASSERT_EQ(recovered.ComponentMeans(), prior.ComponentMeans());
    }
}

TEST(PortfolioRiskValueTest, TestWeightedOverflowRetainsSelectedOutputsAndTradeIdentityInBothModes) {
    RegisterAll_::Init();
    const Handle_<ModelData_> model(new BSModelData_("", 709.0, 0.0));
    const Handle_<ScriptProductData_> product(new ScriptProductData_("", {Cell_(Date_(2027, 1, 1))}, {"pay PAYS EXP(SPOT())"}));
    const Handle_<ScriptPortfolioData_> data(new ScriptPortfolioData_("", {{"OverflowTrade", product, model}}));
    PortfolioWeightedRiskRequest_ request;
    request.selection_.inputs_ = Vector_<String_>{};
    request.weights_ = Vector_<>{3.0};
    for (const bool native : {true, false}) {
        auto simulation = DefaultRiskMonteCarloSettings();
        simulation.enableAad_ = native;
        try {
            static_cast<void>(ValuePortfolioByMonteCarloWithWeightedRisk(data, 1, request, Valuation(), simulation));
            FAIL() << "weighted overflow must fail with the complete group selection context";
        } catch (const ScriptError_& error) {
            const String_ message(error.what());
            ASSERT_NE(message.find("group=0"), String_::npos);
            ASSERT_NE(message.find("OverflowTrade"), String_::npos);
            ASSERT_NE(message.find("trade:0:payoff"), String_::npos);
        }
    }
}

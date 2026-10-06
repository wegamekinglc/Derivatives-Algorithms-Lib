//
// Created by Codex on 2026/10/06.
//

#include <gtest/gtest.h>

#include <dal-public/src/global.hpp>
#include <dal-public/src/models.hpp>
#include <dal-public/src/script.hpp>
#include <dal-public/src/value.hpp>
#include <dal/script/jacobianrisk.hpp>

#include <script_test_observers.hpp>

using namespace Dal;

namespace {
    struct ScopedJacobianThreads_ {
        ThreadPool_* pool_ = ThreadPool_::GetInstance();
        size_t threads_ = pool_->NumThreads();
        bool active_ = pool_->IsActive();
        explicit ScopedJacobianThreads_(size_t threads) { pool_->Start(threads, true); }
        ~ScopedJacobianThreads_() {
            pool_->Start(threads_, true);
            if (!active_)
                pool_->Stop();
        }
    };

    ScriptValuationSettings_ JacobianValuation() {
        ScriptValuationSettings_ valuation;
        valuation.evaluationDate_ = Date_(2026, 1, 1);
        return valuation;
    }

    struct FreezeModelOnHistory_ : Detail::FixingReadObserver_ {
        BSModelData_* model_;
        Script::JacobianRiskRequest_* request_;
        size_t histories_ = 0;
        FreezeModelOnHistory_(BSModelData_* model, Script::JacobianRiskRequest_* request) : model_(model), request_(request) {}
        void BeforeHistory(const String_&) override {
            ++histories_;
            model_->spot_ = 999.0;
            request_->selection_.outputs_->clear();
            request_->maxBlockWidth_ = 0;
        }
        void BeforeFixing(const Index_&, const Environment_*, const DateTime_&) override {}
    };

    void AssertPublicScalarRows(const Handle_<ScriptProductData_>& product,
                                const Handle_<ModelData_>& model,
                                const Script::JacobianRiskRequest_& request,
                                const MonteCarloSettings_& simulation) {
        const auto result = ValueByMonteCarloWithJacobianRisk(product, model, 17, request, JacobianValuation(), simulation);
        Script::RiskRequest_ scalarRequest;
        scalarRequest.inputs_ = request.selection_.inputs_;
        for (size_t row = 0; row < result.OutputAxis().size(); ++row) {
            auto events = product->EventTexts();
            events.back() += " pay = " + result.OutputAxis()[row].label_;
            const auto scalarProduct = NewScriptProduct("scalar", product->Dates(), events, product->Settings());
            const auto scalar = ValueByMonteCarloWithRisk(scalarProduct, model, 17, scalarRequest, JacobianValuation(), simulation);
            ASSERT_NEAR(result.Values()[row], scalar.Values()[0], 1.0e-10);
            for (size_t column = 0; column < result.InputAxis().size(); ++column)
                ASSERT_NEAR(result.Jacobian()(static_cast<int>(row), static_cast<int>(column)), scalar.Jacobian()(0, static_cast<int>(column)),
                            1.0e-10);
        }
        Script::WeightedRiskRequest_ weightedRequest;
        weightedRequest.selection_ = request.selection_;
        weightedRequest.weights_.emplace();
        for (size_t row = 0; row < result.OutputAxis().size(); ++row)
            weightedRequest.weights_->push_back(row % 2 == 0 ? 0.5 : -0.25);
        const auto weighted = ValueByMonteCarloWithWeightedRisk(product, model, 17, weightedRequest, JacobianValuation(), simulation);
        for (size_t column = 0; column < result.InputAxis().size(); ++column) {
            double dot = 0.0;
            for (size_t row = 0; row < result.OutputAxis().size(); ++row)
                dot += (*weightedRequest.weights_)[row] * result.Jacobian()(static_cast<int>(row), static_cast<int>(column));
            ASSERT_NEAR(dot, weighted.Jacobian()(0, static_cast<int>(column)), 1.0e-10);
        }
    }
} // namespace

TEST(JacobianRiskValueTest, TestPreparedPrefixAliasesAndOrderedConstantColumns) {
    InitGlobalData(1);
    const auto product = NewScriptProduct("Jacobian", {Cell_("X"), Cell_("Y"), Cell_(Date_(2025, 1, 1)), Cell_(Date_(2027, 1, 1))},
                                          {"2", "3", "a = X * Y b = a direct = X literal = 5", "pay PAYS 0"});
    const auto model = NewBSModelData("model", 100.0, 0.2, 0.0, 0.0);
    Script::JacobianRiskRequest_ request;
    request.selection_.outputs_ = Vector_<String_>{"output:0", "output:1", "output:2", "output:3", "payoff"};
    request.selection_.inputs_ = Vector_<String_>{"constant:1", "constant:0"};
    request.selection_.reportFactors_ = Vector_<>{0.5, 2.0};
    request.maxBlockWidth_ = 2;
    for (const bool compiled : {false, true}) {
        auto simulation = DefaultRiskMonteCarloSettings();
        simulation.compiled_ = compiled;
        const auto result = ValueByMonteCarloWithJacobianRisk(product, model, 17, request, JacobianValuation(), simulation);
        ASSERT_EQ(result.Values(), Vector_<>({6.0, 6.0, 2.0, 5.0, 0.0}));
        ASSERT_EQ(result.Jacobian().Rows(), 5);
        ASSERT_EQ(result.Jacobian().Cols(), 2);
        ASSERT_DOUBLE_EQ(result.Jacobian()(0, 0), 2.0);
        ASSERT_DOUBLE_EQ(result.Jacobian()(0, 1), 3.0);
        ASSERT_DOUBLE_EQ(result.Jacobian()(1, 0), 2.0);
        ASSERT_DOUBLE_EQ(result.Jacobian()(1, 1), 3.0);
        ASSERT_DOUBLE_EQ(result.Jacobian()(2, 0), 0.0);
        ASSERT_DOUBLE_EQ(result.Jacobian()(2, 1), 1.0);
        ASSERT_DOUBLE_EQ(result.ReportedJacobian()(0, 0), 1.0);
        ASSERT_DOUBLE_EQ(result.ReportedJacobian()(0, 1), 6.0);
        ASSERT_EQ(result.InputAxis()[0].id_, "constant:1");
        ASSERT_EQ(result.InputAxis()[1].id_, "constant:0");
        ASSERT_EQ(result.CompleteInputAxis().size(), 6);
        ASSERT_EQ(result.Execution().actualWidths_, Vector_<size_t>({2, 2, 2}));
        ASSERT_EQ(result.Execution().replayAttempts_, 3);
        ASSERT_EQ(result.Execution().executedPaths_, 51);
        ASSERT_EQ(result.Provenance().execution_->pathsPerReplicate_, 17);
    }
}

TEST(JacobianRiskValueTest, TestInvalidRequestsAndKnownLimitsReadNoHistoryAndSubmitNoTasks) {
    InitGlobalData(1);
    const auto product =
        NewScriptProduct("history", {Cell_(Date_(2025, 1, 1)), Cell_(Date_(2027, 1, 1))}, {"a = FIX(EQ[JACOBIAN_REJECTION])", "pay PAYS a"});
    const auto model = NewBSModelData("model", 100.0, 0.2, 0.0, 0.0);
    Script::TestSupport::RejectFixingReads_ history;
    Script::TestSupport::RejectSubmissions_ workers;
    Detail::ScopedFixingReadObserver_ observeHistory(&history);
    Script::Detail::ScopedSimulationObserver_ observeWorkers(&workers);
    Script::JacobianRiskRequest_ request;
    request.maxBlockWidth_ = 0;
    ASSERT_THROW(static_cast<void>(ValueByMonteCarloWithJacobianRisk(product, model, 17, request, JacobianValuation())), ScriptError_);
    request.maxBlockWidth_ = 1;
    request.selection_.outputs_ = Vector_<String_>{"unknown"};
    ASSERT_THROW(static_cast<void>(ValueByMonteCarloWithJacobianRisk(product, model, 17, request, JacobianValuation())), ScriptError_);
    request.selection_.outputs_.reset();
    request.selection_.numericPayloadBudgetBytes_ = 0;
    ASSERT_THROW(static_cast<void>(ValueByMonteCarloWithJacobianRisk(product, model, 17, request, JacobianValuation())), ScriptError_);
    request.selection_.numericPayloadBudgetBytes_.reset();
    request.recordingCapacityBudgetBytes_ = 0;
    ASSERT_THROW(static_cast<void>(ValueByMonteCarloWithJacobianRisk(product, model, 17, request, JacobianValuation())), ScriptError_);
    request.recordingCapacityBudgetBytes_.reset();
    request.scratchCapacityBudgetBytes_ = 0;
    ASSERT_THROW(static_cast<void>(ValueByMonteCarloWithJacobianRisk(product, model, 17, request, JacobianValuation())), ScriptError_);
    ASSERT_EQ(history.historyCalls_, 0);
    ASSERT_EQ(history.fixingCalls_, 0);
    ASSERT_EQ(workers.calls_, 0);
}

TEST(JacobianRiskValueTest, TestModelDataAndRequestAreSealedBeforeHistory) {
    InitGlobalData(1);
    auto* mutableModel = new BSModelData_("model", 100.0, 0.0);
    const Handle_<ModelData_> model(mutableModel);
    ScriptProductSettings_ productSettings;
    productSettings.defaultIndex_ = "EQ[JACOBIAN_FREEZE]";
    const auto product = NewScriptProduct("freeze", {Cell_(Date_(2025, 1, 1)), Cell_(Date_(2027, 1, 1))},
                                          {"a = FIX(EQ[JACOBIAN_FREEZE])", "a = a + SPOT() pay PAYS a"}, productSettings);
    Script::TestSupport::StoreScriptTestFixing("EQ[JACOBIAN_FREEZE]", 10.0, DateTime_(Date_(2025, 1, 1), 0.0));
    Script::JacobianRiskRequest_ request;
    request.selection_.outputs_ = Vector_<String_>{"output:0", "payoff"};
    request.selection_.inputs_ = Vector_<String_>{"model:0"};
    request.maxBlockWidth_ = 1;
    FreezeModelOnHistory_ mutation(mutableModel, &request);
    Detail::ScopedFixingReadObserver_ observer(&mutation);
    const auto result = ValueByMonteCarloWithJacobianRisk(product, model, 17, request, JacobianValuation());
    ASSERT_EQ(mutation.histories_, 1);
    ASSERT_DOUBLE_EQ(mutableModel->spot_, 999.0);
    ASSERT_TRUE(request.selection_.outputs_->empty());
    ASSERT_NEAR(result.Values()[0], 110.0, 1.0e-10);
    ASSERT_NEAR(result.Values()[1], 110.0, 1.0e-10);
    ASSERT_NEAR(result.Jacobian()(0, 0), 1.0, 1.0e-10);
    ASSERT_NEAR(result.Jacobian()(1, 0), 1.0, 1.0e-10);
    ASSERT_DOUBLE_EQ(result.InputAxis()[0].value_, 100.0);
    ASSERT_EQ(result.Provenance().execution_->observations_.size(), 2);
    ASSERT_EQ(result.Execution().replayAttempts_, 2);
}

TEST(JacobianRiskValueTest, TestEmptyColumnsPreserveNativeAndPassiveEstimators) {
    InitGlobalData(1);
    const auto product = NewScriptProduct("empty", {Cell_(Date_(2026, 1, 1))}, {"IF SPOT() > 100 THEN a = 1 ELSE a = 0 END b = a pay PAYS 0"});
    const auto model = NewBSModelData("model", 100.0, 0.0, 0.0, 0.0);
    Script::JacobianRiskRequest_ request;
    request.selection_.outputs_ = Vector_<String_>{"output:1", "output:0"};
    request.selection_.inputs_ = Vector_<String_>{};
    request.maxBlockWidth_ = 2;
    for (const bool compiled : {false, true}) {
        auto simulation = DefaultRiskMonteCarloSettings();
        simulation.compiled_ = compiled;
        const auto native = ValueByMonteCarloWithJacobianRisk(product, model, 17, request, JacobianValuation(), simulation);
        ASSERT_EQ(native.Values(), Vector_<>({0.5, 0.5}));
        ASSERT_EQ(native.Jacobian().Rows(), 2);
        ASSERT_EQ(native.Jacobian().Cols(), 0);
        ASSERT_EQ(native.Execution().actualWidths_, Vector_<size_t>({2}));
        ASSERT_EQ(native.Provenance().method_, "NativeAAD");
        simulation.enableAad_ = false;
        request.recordingCapacityBudgetBytes_ = 0;
        const auto passive = ValueByMonteCarloWithJacobianRisk(product, model, 17, request, JacobianValuation(), simulation);
        ASSERT_EQ(passive.Values(), Vector_<>({0.0, 0.0}));
        ASSERT_EQ(passive.Jacobian().Rows(), 2);
        ASSERT_EQ(passive.Jacobian().Cols(), 0);
        ASSERT_TRUE(passive.Execution().actualWidths_.empty());
        ASSERT_EQ(passive.Execution().replayAttempts_, 1);
        ASSERT_EQ(passive.Execution().executedPaths_, 17);
        ASSERT_EQ(passive.Execution().peakRecordingBytes_, 0);
        ASSERT_EQ(passive.Provenance().method_, "PriceOnly");
        request.recordingCapacityBudgetBytes_.reset();
    }
}

TEST(JacobianRiskValueTest, TestPassiveValuesDoNotFormAnUnusedWeightedSum) {
    InitGlobalData(1);
    const auto product = NewScriptProduct("large", {Cell_("X"), Cell_(Date_(2027, 1, 1))}, {"1e308", "a = X b = X pay PAYS 0"});
    const auto model = NewBSModelData("model", 100.0, 0.0, 0.0, 0.0);
    Script::JacobianRiskRequest_ request;
    request.selection_.outputs_ = Vector_<String_>{"output:0", "output:1"};
    request.selection_.inputs_ = Vector_<String_>{};
    auto simulation = DefaultRiskMonteCarloSettings();
    simulation.enableAad_ = false;
    for (const bool compiled : {false, true}) {
        simulation.compiled_ = compiled;
        const auto result = ValueByMonteCarloWithJacobianRisk(product, model, 1, request, JacobianValuation(), simulation);
        ASSERT_EQ(result.Values(), Vector_<>({1e308, 1e308}));
    }
}

TEST(JacobianRiskValueTest, TestPublicScalarAndWeightedCommonPathOraclesForOneToSixtyFourRows) {
    InitGlobalData(1);
    String_ event;
    for (size_t row = 0; row < 63; ++row)
        event += "o" + String_(std::to_string(row)) + " = " + String_(std::to_string(row + 1)) + " * SPOT() ";
    event += "pay = 64 * SPOT() pay PAYS 0";
    const auto product = NewScriptProduct("rows", {Cell_(Date_(2027, 1, 1))}, {event});
    const auto model = NewBSModelData("model", 100.0, 0.2, 0.0, 0.0);
    auto parsed = product->Product();
    parsed.IndexVariables();
    const auto outputs = Script::ScriptRiskOutputAxis(parsed);
    for (const size_t threads : {1, 4}) {
        const ScopedJacobianThreads_ workers(threads);
        for (const bool compiled : {false, true}) {
            auto simulation = DefaultRiskMonteCarloSettings();
            simulation.compiled_ = compiled;
            for (const size_t rows : {1, 4, 16, 64}) {
                Script::JacobianRiskRequest_ request;
                request.selection_.outputs_.emplace();
                for (size_t row = rows; row > 0; --row)
                    request.selection_.outputs_->push_back(outputs[row - 1].id_);
                request.selection_.inputs_ = Vector_<String_>{"model:1", "model:0"};
                request.maxBlockWidth_ = 3;
                ASSERT_NO_FATAL_FAILURE(AssertPublicScalarRows(product, model, request, simulation));
            }
        }
    }
}

TEST(JacobianRiskValueTest, TestFrozenCentralDifferencesForOrderedConstantColumns) {
    InitGlobalData(1);
    const auto model = NewBSModelData("model", 100.0, 0.2, 0.0, 0.0);
    auto product = [](double x, double y) {
        return NewScriptProduct("difference", {Cell_("X"), Cell_("Y"), Cell_(Date_(2027, 1, 1))},
                                {String_(std::to_string(x)), String_(std::to_string(y)), "a = X * Y b = a direct = X literal = 5 pay PAYS 0"});
    };
    Script::JacobianRiskRequest_ request;
    request.selection_.outputs_ = Vector_<String_>{"output:1", "output:2", "output:0", "output:3"};
    request.selection_.inputs_ = Vector_<String_>{"constant:1", "constant:0"};
    request.maxBlockWidth_ = 3;
    constexpr double STEP = 0.1;
    for (const bool compiled : {false, true}) {
        auto simulation = DefaultRiskMonteCarloSettings();
        simulation.compiled_ = compiled;
        const auto result = ValueByMonteCarloWithJacobianRisk(product(2.0, 3.0), model, 17, request, JacobianValuation(), simulation);
        auto priceOnly = simulation;
        priceOnly.enableAad_ = false;
        auto valueRequest = request;
        valueRequest.selection_.inputs_ = Vector_<String_>{};
        for (size_t column = 0; column < 2; ++column) {
            const auto plus = ValueByMonteCarloWithJacobianRisk(product(2.0 + (column == 1 ? STEP : 0.0), 3.0 + (column == 0 ? STEP : 0.0)), model,
                                                                17, valueRequest, JacobianValuation(), priceOnly);
            const auto minus = ValueByMonteCarloWithJacobianRisk(product(2.0 - (column == 1 ? STEP : 0.0), 3.0 - (column == 0 ? STEP : 0.0)), model,
                                                                 17, valueRequest, JacobianValuation(), priceOnly);
            for (size_t row = 0; row < result.Values().size(); ++row)
                ASSERT_NEAR(result.Jacobian()(static_cast<int>(row), static_cast<int>(column)),
                            (plus.Values()[row] - minus.Values()[row]) / (2 * STEP), 1.0e-10);
        }
    }
}

TEST(JacobianRiskValueTest, TestPassiveKnownBudgetRejectsBeforeHistoryAndWorkerSubmission) {
    InitGlobalData(1);
    const auto product =
        NewScriptProduct("past", {Cell_(Date_(2025, 1, 1)), Cell_(Date_(2027, 1, 1))}, {"a = FIX(EQ[JACOBIAN_PASSIVE_REJECT])", "pay PAYS a"});
    const auto model = NewBSModelData("model", 100.0, 0.0, 0.0, 0.0);
    Script::JacobianRiskRequest_ request;
    request.selection_.inputs_ = Vector_<String_>{};
    request.scratchCapacityBudgetBytes_ = 0;
    request.recordingCapacityBudgetBytes_ = 0;
    auto simulation = DefaultRiskMonteCarloSettings();
    simulation.enableAad_ = false;
    Script::TestSupport::RejectFixingReads_ history;
    Script::TestSupport::RejectSubmissions_ submissions;
    const Detail::ScopedFixingReadObserver_ observeHistory(&history);
    const Script::Detail::ScopedSimulationObserver_ observeSubmissions(&submissions);
    ASSERT_THROW(static_cast<void>(ValueByMonteCarloWithJacobianRisk(product, model, 17, request, JacobianValuation(), simulation)), ScriptError_);
    ASSERT_EQ(history.historyCalls_, 0);
    ASSERT_EQ(history.fixingCalls_, 0);
    ASSERT_EQ(submissions.calls_, 0);
}

TEST(JacobianRiskValueTest, TestPassiveFailureDrainsAndEarlierResultsRemainUsable) {
    InitGlobalData(1);
    const ScopedJacobianThreads_ workers(4);
    const auto product = NewScriptProduct("recover", {Cell_(Date_(2027, 1, 1))}, {"a = SPOT() b = a pay PAYS 0"});
    const auto model = NewBSModelData("model", 100.0, 0.0, 0.0, 0.0);
    Script::JacobianRiskRequest_ request;
    request.selection_.outputs_ = Vector_<String_>{"output:1", "output:0"};
    request.selection_.inputs_ = Vector_<String_>{};
    auto simulation = DefaultRiskMonteCarloSettings();
    simulation.enableAad_ = false;
    const auto first = ValueByMonteCarloWithJacobianRisk(product, model, 17, request, JacobianValuation(), simulation);
    {
        Script::TestSupport::RejectSubmissions_ failure;
        const Script::Detail::ScopedSimulationObserver_ observe(&failure);
        ASSERT_THROW(static_cast<void>(ValueByMonteCarloWithJacobianRisk(product, model, 17, request, JacobianValuation(), simulation)),
                     ScriptError_);
        ASSERT_EQ(failure.calls_, 1);
    }
    const auto recovered = ValueByMonteCarloWithJacobianRisk(product, model, 17, request, JacobianValuation(), simulation);
    ASSERT_EQ(recovered.Values(), first.Values());
    ASSERT_EQ(first.Jacobian().Rows(), 2);
    ASSERT_EQ(first.Jacobian().Cols(), 0);
    const auto overflow = NewScriptProduct("overflow", {Cell_("X"), Cell_(Date_(2027, 1, 1))}, {"1e308", "a = 7 b = X pay PAYS 0"});
    request.selection_.outputs_ = Vector_<String_>{"output:1"};
    try {
        static_cast<void>(ValueByMonteCarloWithJacobianRisk(overflow, model, 16, request, JacobianValuation(), simulation));
        FAIL() << "overflowing selected value must fail";
    } catch (const ScriptError_& error) {
        ASSERT_NE(std::string(error.what()).find("output=output:1"), std::string::npos);
    }
    ASSERT_EQ(first.Values().size(), 2);
    ASSERT_NEAR(first.Values()[0], 100.0, 1.0e-10);
}

TEST(JacobianRiskValueTest, TestNestedHybridAndGsrSnapshotsMatchIndependentScalarRequests) {
    InitGlobalData(1);
    const ScopedJacobianThreads_ workers(4);
    HybridSettings_ settings;
    settings.domesticCurrency_ = "USD";
    settings.components_ = {Handle_<HybridComponentData_>(new HybridBSEquityData_("A", "EQ[A]", "USD", "FA", 100.0, 0.2, 0.0)),
                            Handle_<HybridComponentData_>(new HybridDeterministicRateData_("RATE", "USD", 0.05))};
    settings.correlation_ = Handle_<HybridCorrelationData_>(new HybridConstantCorrelationData_("correlation", {"FA"}, Matrix_<>(1, 1, 1.0)));
    const auto hybrid = NewHybridModelData("hybrid", settings);
    const Date_ today(2026, 1, 1);
    const auto curve = NewGSRCurveData("curve", today, "USD", {today, today.AddDays(1095)}, {0.0, -0.09}, {}, Matrix_<>(0, 0));
    const auto gsr = NewGSRModelData("gsr", curve, NewGSRVolData("vol", {today}, {0.02}, {today}, {1.0}));
    const Vector_<Handle_<ModelData_>> models{hybrid, gsr};
    const Vector_<String_> observations{"FIX(EQ[A])", "FIX(IR[USD,DF,2028-01-01])"};
    Script::JacobianRiskRequest_ request;
    request.selection_.outputs_ = Vector_<String_>{"payoff", "output:0"};
    request.selection_.inputs_ = Vector_<String_>{"model:0"};
    request.maxBlockWidth_ = 2;
    for (size_t index = 0; index < models.size(); ++index) {
        const auto product = NewScriptProduct("nested", {Cell_(Date_(2027, 1, 1))}, {"a = " + observations[index] + " pay PAYS a"});
        for (const bool compiled : {false, true}) {
            auto simulation = DefaultRiskMonteCarloSettings();
            simulation.compiled_ = compiled;
            ASSERT_NO_FATAL_FAILURE(AssertPublicScalarRows(product, models[index], request, simulation));
        }
    }
}

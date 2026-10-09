//
// Created by Codex on 2026-10-06.
//

#include <gtest/gtest.h>

#include <limits>

#include <dal-public/src/global.hpp>
#include <dal-public/src/models.hpp>
#include <dal-public/src/script.hpp>
#include <dal-public/src/value.hpp>
#include <dal/concurrency/threadpool.hpp>

#include <script_test_observers.hpp>

using namespace Dal;

namespace {
    struct ScopedThreads_ {
        ThreadPool_* pool_ = ThreadPool_::GetInstance();
        size_t threads_ = pool_->NumThreads();
        bool active_ = pool_->IsActive();
        explicit ScopedThreads_(size_t threads) { pool_->Start(threads, true); }
        ~ScopedThreads_() {
            pool_->Start(threads_, true);
            if (!active_)
                pool_->Stop();
        }
    };

    ScriptValuationSettings_ WeightedValuation() {
        ScriptValuationSettings_ valuation;
        valuation.evaluationDate_ = Date_(2026, 1, 1);
        return valuation;
    }
} // namespace

TEST(WeightedRiskValueTest, TestAnalyticObjectiveOwnsComponentsAndMeanGradient) {
    const auto product =
        NewScriptProduct("weighted", {Cell_("X"), Cell_("Y"), Cell_(Date_(2027, 1, 1))}, {"2", "3", "a = X * Y b = X + Y pay PAYS 5"});
    const auto model = NewBSModelData("model", 100.0, 0.2, 0.0, 0.0);
    ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = Date_(2026, 1, 1);
    Script::WeightedRiskRequest_ request;
    request.selection_.outputs_ = Vector_<String_>{"output:0", "output:1", "payoff"};
    request.selection_.inputs_ = Vector_<String_>{"constant:0", "constant:1"};
    request.selection_.reportFactors_ = Vector_<>{0.5, 2.0};
    request.weights_ = Vector_<>{2.0, -1.0, 0.5};
    request.selection_.numericPayloadBudgetBytes_ = 9 * sizeof(double);
    for (const bool compiled : {false, true}) {
        auto simulation = DefaultRiskMonteCarloSettings();
        simulation.compiled_ = compiled;
        const auto result = ValueByMonteCarloWithWeightedRisk(product, model, 257, request, valuation, simulation);
        ASSERT_DOUBLE_EQ(result.WeightedValue(), 9.5);
        ASSERT_EQ(result.ComponentMeans(), Vector_<>({6.0, 5.0, 5.0}));
        ASSERT_EQ(result.Weights(), Vector_<>({2.0, -1.0, 0.5}));
        ASSERT_EQ(result.OutputAxis()[0].label_, "a");
        ASSERT_EQ(result.Jacobian().Rows(), 1);
        ASSERT_EQ(result.Jacobian().Cols(), 2);
        ASSERT_NEAR(result.Jacobian()(0, 0), 5.0, 1.0e-10);
        ASSERT_NEAR(result.Jacobian()(0, 1), 3.0, 1.0e-10);
        ASSERT_NEAR(result.ReportedJacobian()(0, 0), 2.5, 1.0e-10);
        ASSERT_NEAR(result.ReportedJacobian()(0, 1), 6.0, 1.0e-10);
        ASSERT_EQ(result.Provenance().method_, "NativeAAD");
        ASSERT_EQ(result.Provenance().execution_->pathsPerReplicate_, 257);
    }
}

TEST(WeightedRiskValueTest, TestIRNPositionedBatchesPreserveWeightedComponentsAndAllInputs) {
    const ScopedThreads_ restore(1);
    const auto product = NewScriptProduct("irn", {Cell_(Date_(2027, 1, 1))}, {"a = SPOT() b = 2 * a pay PAYS 3 * a"});
    const auto model = NewBSModelData("model", 100.0, 0.2, 0.03, 0.01);
    Script::WeightedRiskRequest_ request;
    request.selection_.outputs_ = Vector_<String_>{"output:0", "output:1", "payoff"};
    request.weights_ = Vector_<>{2.0, -1.0, 0.5};
    for (const bool aad : {false, true}) {
        auto simulation = DefaultRiskMonteCarloSettings();
        simulation.rsg_ = "irn";
        simulation.compiled_ = true;
        simulation.enableAad_ = aad;
        const auto serial = ValueByMonteCarloWithWeightedRisk(product, model, 65553, request, WeightedValuation(), simulation);
        restore.pool_->Start(4, true);
        const auto parallel = ValueByMonteCarloWithWeightedRisk(product, model, 65553, request, WeightedValuation(), simulation);
        ASSERT_NEAR(serial.WeightedValue(), parallel.WeightedValue(), 1e-10);
        ASSERT_EQ(serial.ComponentMeans().size(), parallel.ComponentMeans().size());
        for (size_t i = 0; i < serial.ComponentMeans().size(); ++i)
            ASSERT_NEAR(serial.ComponentMeans()[i], parallel.ComponentMeans()[i], 1e-10);
        ASSERT_EQ(serial.Jacobian().Cols(), parallel.Jacobian().Cols());
        for (int i = 0; i < serial.Jacobian().Cols(); ++i)
            ASSERT_NEAR(serial.Jacobian()(0, i), parallel.Jacobian()(0, i), 1e-10);
        restore.pool_->Start(1, true);
    }
}

TEST(WeightedRiskValueTest, TestHistoricalAliasesDirectInputsAndConsecutiveWeights) {
    const auto product = NewScriptProduct("prefix", {Cell_("X"), Cell_("Y"), Cell_(Date_(2025, 1, 1)), Cell_(Date_(2027, 1, 1))},
                                          {"2", "3", "a = X * Y b = a direct = X literal = 5", "pay PAYS 0"});
    const auto model = NewBSModelData("model", 100.0, 0.2, 0.0, 0.0);
    const Vector_<Vector_<double>> weights = {{2.0, 3.0, 0.0, 0.0}, {-1.0, -1.0, 0.0, 0.0}, {0.0, 0.0, 4.0, -2.0}, {0.0, 0.0, 0.0, 0.0}};
    const Vector_<double> values = {30.0, -12.0, -2.0, 0.0};
    const Vector_<double> dx = {15.0, -6.0, 4.0, 0.0};
    const Vector_<double> dy = {10.0, -4.0, 0.0, 0.0};
    for (const size_t threads : {1, 4}) {
        const ScopedThreads_ scoped(threads);
        for (const bool compiled : {false, true}) {
            auto simulation = DefaultRiskMonteCarloSettings();
            simulation.compiled_ = compiled;
            for (size_t index = 0; index < weights.size(); ++index) {
                Script::WeightedRiskRequest_ request;
                request.selection_.outputs_ = Vector_<String_>{"output:0", "output:1", "output:2", "output:3"};
                request.selection_.inputs_ = Vector_<String_>{"constant:1", "constant:0"};
                request.weights_ = weights[index];
                const auto result = ValueByMonteCarloWithWeightedRisk(product, model, 257, request, WeightedValuation(), simulation);
                request.weights_->clear();
                request.selection_.outputs_->clear();
                ASSERT_DOUBLE_EQ(result.WeightedValue(), values[index]);
                ASSERT_EQ(result.ComponentMeans(), Vector_<>({6.0, 6.0, 2.0, 5.0}));
                ASSERT_EQ(result.Weights(), weights[index]);
                ASSERT_NEAR(result.Jacobian()(0, 0), dy[index], 1.0e-10);
                ASSERT_NEAR(result.Jacobian()(0, 1), dx[index], 1.0e-10);
                ASSERT_EQ(result.OutputAxis().size(), 4);
                ASSERT_EQ(result.CompleteInputAxis().size(), 6);
                ASSERT_EQ(result.Provenance().execution_->productEvents_, product->EventTexts());
            }
        }
    }
}

TEST(WeightedRiskValueTest, TestPriceOnlyAndEmptyNativeColumnsKeepDifferentEstimators) {
    const auto product = NewScriptProduct("smooth", {Cell_(Date_(2026, 1, 1))}, {"IF SPOT() > 100 THEN pay PAYS 1 ELSE pay PAYS 0 END"});
    const auto model = NewBSModelData("model", 100.0, 0.0, 0.0, 0.0);
    Script::WeightedRiskRequest_ request;
    request.selection_.inputs_ = Vector_<String_>{};
    for (const bool compiled : {false, true}) {
        auto simulation = DefaultRiskMonteCarloSettings();
        simulation.compiled_ = compiled;
        const auto native = ValueByMonteCarloWithWeightedRisk(product, model, 17, request, WeightedValuation(), simulation);
        ASSERT_NEAR(native.WeightedValue(), 0.5, 1.0e-10);
        ASSERT_EQ(native.Jacobian().Rows(), 1);
        ASSERT_EQ(native.Jacobian().Cols(), 0);
        ASSERT_EQ(native.Provenance().method_, "NativeAAD");
        simulation.enableAad_ = false;
        const auto passive = ValueByMonteCarloWithWeightedRisk(product, model, 17, request, WeightedValuation(), simulation);
        ASSERT_DOUBLE_EQ(passive.WeightedValue(), 0.0);
        ASSERT_EQ(passive.ComponentMeans(), Vector_<>({0.0}));
        ASSERT_EQ(passive.Jacobian().Rows(), 1);
        ASSERT_EQ(passive.Jacobian().Cols(), 0);
        ASSERT_EQ(passive.Provenance().method_, "PriceOnly");
    }
}

TEST(WeightedRiskValueTest, TestInvalidRequestsRejectBeforeHistoryAndWorkers) {
    const auto product =
        NewScriptProduct("history", {Cell_(Date_(2025, 1, 1)), Cell_(Date_(2027, 1, 1))}, {"a = FIX(EQ[WEIGHTED_REJECTION])", "pay PAYS a"});
    const auto model = NewBSModelData("model", 100.0, 0.2, 0.0, 0.0);
    Script::TestSupport::RejectFixingReads_ history;
    Script::TestSupport::RejectSubmissions_ workers;
    const Detail::ScopedFixingReadObserver_ observeHistory(&history);
    const Script::Detail::ScopedSimulationObserver_ observeWorkers(&workers);
    const auto run = [&](const Script::WeightedRiskRequest_& request) {
        return ValueByMonteCarloWithWeightedRisk(product, model, 17, request, WeightedValuation());
    };
    for (const Vector_<String_>& outputs : {Vector_<String_>{}, Vector_<String_>{"unknown"}, Vector_<String_>{"payoff", "PAYOFF"}}) {
        Script::WeightedRiskRequest_ request;
        request.selection_.outputs_ = outputs;
        ASSERT_THROW(static_cast<void>(run(request)), ScriptError_);
    }
    for (const Vector_<double>& weights : {Vector_<double>{}, Vector_<double>{1.0, 2.0}, Vector_<double>{std::numeric_limits<double>::infinity()}}) {
        Script::WeightedRiskRequest_ request;
        request.weights_ = weights;
        ASSERT_THROW(static_cast<void>(run(request)), ScriptError_);
    }
    Script::WeightedRiskRequest_ request;
    request.selection_.numericPayloadBudgetBytes_ = 7 * sizeof(double) - 1;
    ASSERT_THROW(static_cast<void>(run(request)), ScriptError_);
    request.selection_.numericPayloadBudgetBytes_.reset();
    request.selection_.inputs_ = Vector_<String_>{"model:99"};
    ASSERT_THROW(static_cast<void>(run(request)), ScriptError_);
    ASSERT_EQ(history.historyCalls_, 0);
    ASSERT_EQ(history.fixingCalls_, 0);
    ASSERT_EQ(workers.calls_, 0);
}

TEST(WeightedRiskValueTest, TestZeroWeightNonfiniteComponentDrainsAndNextRequestRecovers) {
    const ScopedThreads_ scoped(4);
    const auto invalid = NewScriptProduct("invalid", {Cell_(Date_(2027, 1, 1))}, {"a = EXP(1000) pay PAYS 1"});
    const auto valid = NewScriptProduct("valid", {Cell_(Date_(2027, 1, 1))}, {"a = 100 pay PAYS 1"});
    const auto model = NewBSModelData("model", 100.0, 0.0, 0.0, 0.0);
    Script::WeightedRiskRequest_ request;
    request.selection_.outputs_ = Vector_<String_>{"output:0", "payoff"};
    request.weights_ = Vector_<>{0.0, 1.0};
    for (const bool aad : {false, true}) {
        for (const bool compiled : {false, true}) {
            auto simulation = DefaultRiskMonteCarloSettings();
            simulation.enableAad_ = aad;
            simulation.compiled_ = compiled;
            ASSERT_THROW(static_cast<void>(ValueByMonteCarloWithWeightedRisk(invalid, model, 257, request, WeightedValuation(), simulation)),
                         ScriptError_);
            const auto result = ValueByMonteCarloWithWeightedRisk(valid, model, 257, request, WeightedValuation(), simulation);
            ASSERT_DOUBLE_EQ(result.WeightedValue(), 1.0);
            ASSERT_EQ(result.ComponentMeans(), Vector_<>({100.0, 1.0}));
            const auto scalar = ValueByMonteCarloWithRisk(valid, model, 17, {}, WeightedValuation(), simulation);
            ASSERT_DOUBLE_EQ(scalar.Values()[0], 1.0);
        }
    }
}

TEST(WeightedRiskValueTest, TestCommonPathComponentPricesGradientAndPassiveWeightedOrder) {
    const ScopedThreads_ scoped(1);
    const auto product = NewScriptProduct("weighted_spot", {Cell_(Date_(2027, 1, 1))}, {"a = SPOT() b = 2 * SPOT() pay PAYS 3 * SPOT()"});
    const auto model = NewBSModelData("model", 100.0, 0.2, 0.0, 0.0);
    Script::WeightedRiskRequest_ request;
    request.selection_.outputs_ = Vector_<String_>{"payoff", "output:0", "output:1"};
    request.selection_.inputs_ = Vector_<String_>{"model:0", "model:1"};
    request.weights_ = Vector_<>{0.5, 2.0, -1.0};
    for (const bool compiled : {false, true}) {
        auto simulation = DefaultRiskMonteCarloSettings();
        simulation.compiled_ = compiled;
        const auto result = ValueByMonteCarloWithWeightedRisk(product, model, 257, request, WeightedValuation(), simulation);
        Vector_<double> means;
        Vector_<double> gradients(2, 0.0);
        double objective = 0.0;
        const Vector_<String_> expressions = {"3 * SPOT()", "SPOT()", "2 * SPOT()"};
        for (size_t component = 0; component < expressions.size(); ++component) {
            const auto scalarProduct = NewScriptProduct("component", {Cell_(Date_(2027, 1, 1))}, {"pay PAYS " + expressions[component]});
            const auto scalar = ValueByMonteCarlo(scalarProduct, model, 257, WeightedValuation(), simulation);
            means.push_back(scalar.at("PV"));
            objective += (*request.weights_)[component] * scalar.at("PV");
            gradients[0] += (*request.weights_)[component] * scalar.at("d_spot");
            gradients[1] += (*request.weights_)[component] * scalar.at("d_vol");
        }
        for (size_t component = 0; component < means.size(); ++component)
            ASSERT_NEAR(result.ComponentMeans()[component], means[component], 1.0e-10);
        ASSERT_NEAR(result.WeightedValue(), objective, 1.0e-10);
        ASSERT_NEAR(result.Jacobian()(0, 0), gradients[0], 1.0e-10);
        ASSERT_NEAR(result.Jacobian()(0, 1), gradients[1], 1.0e-10);
        ASSERT_NEAR(result.Jacobian()(0, 0), result.WeightedValue() / 100.0, 1.0e-10);
        auto passiveRequest = request;
        passiveRequest.selection_.inputs_.reset();
        simulation.enableAad_ = false;
        const auto passive = ValueByMonteCarloWithWeightedRisk(product, model, 257, passiveRequest, WeightedValuation(), simulation);
        ASSERT_NEAR(passive.WeightedValue(), objective, 1.0e-10);
        ASSERT_EQ(passive.Jacobian().Cols(), 0);
        for (size_t component = 0; component < means.size(); ++component)
            ASSERT_NEAR(passive.ComponentMeans()[component], means[component], 1.0e-10);
    }
}

TEST(WeightedRiskValueTest, TestRequestAndProvenanceOwnSnapshotsBeforeHistoryCallback) {
    InitGlobalData(1);
    Script::TestSupport::StoreScriptTestFixing("EQ[WEIGHTED_SNAPSHOT]", 80.0, DateTime_(Date_(2025, 1, 1), 0.0));
    auto product = NewScriptProduct("snapshot", {Cell_("SCALE"), Cell_(Date_(2025, 1, 1)), Cell_(Date_(2027, 1, 1))},
                                    {"2", "a = SCALE * FIX(EQ[WEIGHTED_SNAPSHOT])", "pay PAYS a"});
    auto model = NewBSModelData("original", 100.0, 0.0, 0.0, 0.0);
    auto simulation = DefaultRiskMonteCarloSettings();
    auto valuation = WeightedValuation();
    Script::WeightedRiskRequest_ request;
    request.selection_.outputs_ = Vector_<String_>{"output:0", "payoff"};
    request.selection_.inputs_ = Vector_<String_>{"constant:0"};
    request.weights_ = Vector_<>{2.0, -1.0};
    struct MutateCaller_ : Detail::FixingReadObserver_ {
        Handle_<ScriptProductData_>* product_;
        Handle_<ModelData_>* model_;
        MonteCarloSettings_* simulation_;
        ScriptValuationSettings_* valuation_;
        Script::WeightedRiskRequest_* request_;
        void BeforeHistory(const String_&) override {
            *product_ = NewScriptProduct("changed", {Cell_(Date_(2027, 1, 1))}, {"pay PAYS 1000"});
            *model_ = NewBSModelData("changed", 500.0, 0.0, 0.0, 0.0);
            simulation_->enableAad_ = false;
            valuation_->evaluationDate_ = Date_(2030, 1, 1);
            request_->weights_->clear();
            request_->selection_.outputs_->clear();
        }
    } observer;
    observer.product_ = &product;
    observer.model_ = &model;
    observer.simulation_ = &simulation;
    observer.valuation_ = &valuation;
    observer.request_ = &request;
    const auto result = [&] {
        const Detail::ScopedFixingReadObserver_ observe(&observer);
        return ValueByMonteCarloWithWeightedRisk(product, model, 32, request, valuation, simulation);
    }();
    ASSERT_DOUBLE_EQ(result.WeightedValue(), 160.0);
    ASSERT_EQ(result.Weights(), Vector_<>({2.0, -1.0}));
    ASSERT_EQ(result.ComponentMeans(), Vector_<>({160.0, 160.0}));
    ASSERT_NEAR(result.Jacobian()(0, 0), 80.0, 1.0e-10);
    ASSERT_EQ(result.Provenance().method_, "NativeAAD");
    ASSERT_EQ(result.Provenance().evaluationDate_, WeightedValuation().evaluationDate_);
    ASSERT_EQ(result.Provenance().execution_->productEvents_[2], "pay PAYS a");
    ASSERT_TRUE(result.Provenance().execution_->simulation_.enableAad_);
    ASSERT_EQ(result.Provenance().execution_->observations_[0].value_, std::optional<double>(80.0));
    Script::TestSupport::StoreScriptTestFixing("EQ[WEIGHTED_SNAPSHOT]", 90.0, DateTime_(Date_(2025, 1, 1), 0.0));
    ASSERT_DOUBLE_EQ(result.WeightedValue(), 160.0);
    ASSERT_EQ(result.Provenance().execution_->observations_[0].value_, std::optional<double>(80.0));
}

TEST(WeightedRiskValueTest, TestPartialSubmissionFailureDrainsAndRecovers) {
    const ScopedThreads_ scoped(4);
    const auto product = NewScriptProduct("submission", {Cell_(Date_(2027, 1, 1))}, {"a = 2 pay PAYS 3"});
    const auto model = NewBSModelData("model", 100.0, 0.0, 0.0, 0.0);
    Script::WeightedRiskRequest_ request;
    request.selection_.outputs_ = Vector_<String_>{"output:0", "payoff"};
    request.weights_ = Vector_<>{2.0, -1.0};
    struct FailSecondSubmission_ : Script::Detail::SimulationObserver_ {
        size_t submissions_ = 0;
        void AfterSubmission() override {
            if (++submissions_ == 2)
                THROW("weighted submission failure");
        }
    };
    for (const bool aad : {false, true}) {
        auto simulation = DefaultRiskMonteCarloSettings();
        simulation.enableAad_ = aad;
        FailSecondSubmission_ failure;
        {
            const Script::Detail::ScopedSimulationObserver_ observe(&failure);
            ASSERT_THROW(static_cast<void>(ValueByMonteCarloWithWeightedRisk(product, model, 257, request, WeightedValuation(), simulation)),
                         Exception_);
        }
        ASSERT_EQ(failure.submissions_, 2);
        const auto result = ValueByMonteCarloWithWeightedRisk(product, model, 257, request, WeightedValuation(), simulation);
        ASSERT_DOUBLE_EQ(result.WeightedValue(), 1.0);
        ASSERT_EQ(result.ComponentMeans(), Vector_<>({2.0, 3.0}));
    }
}

#if defined(DAL_ENABLE_AAD_PROFILING)
TEST(WeightedRiskValueTest, TestOneForwardAndReverseSuffixPerPathAndPrefixPerBatch) {
    const ScopedThreads_ scoped(4);
    const auto product = NewScriptProduct("profile", {Cell_(Date_(2027, 1, 1))}, {"a = SPOT() b = a pay PAYS a + b"});
    const auto model = NewBSModelData("model", 100.0, 0.2, 0.0, 0.0);
    Script::WeightedRiskRequest_ request;
    request.selection_.outputs_ = Vector_<String_>{"output:0", "output:1", "payoff"};
    request.weights_ = Vector_<>{2.0, -1.0, 0.5};
    for (const bool compiled : {false, true}) {
        auto simulation = DefaultRiskMonteCarloSettings();
        simulation.compiled_ = compiled;
        AAD::ProfilingData_ data;
        const auto result = [&] {
            AAD::ProfilingScope_ profile(&data);
            return ValueByMonteCarloWithWeightedRisk(product, model, 257, request, WeightedValuation(), simulation);
        }();
        ASSERT_TRUE(data.complete_);
        ASSERT_FALSE(data.invalidMeasurement_);
        ASSERT_GT(result.WeightedValue(), 0.0);
        ASSERT_EQ(data.taskGroups_.size(), 1);
        const Script::BatchPlan_ batches(257, 4);
        ASSERT_EQ(data.taskGroups_.front().size(), batches.BatchCount());
        std::uint64_t forward = 0, payoff = 0, suffix = 0, prefix = 0;
        for (const auto& task : data.taskGroups_.front()) {
            ASSERT_TRUE(task.complete_);
            forward += task.Phase(AAD::AADProfilingPhase_::Value_::PATH_FORWARD).calls_;
            payoff += task.Phase(AAD::AADProfilingPhase_::Value_::PAYOFF).calls_;
            suffix += task.Phase(AAD::AADProfilingPhase_::Value_::REVERSE_SUFFIX).calls_;
            prefix += task.Phase(AAD::AADProfilingPhase_::Value_::REVERSE_PREFIX).calls_;
            ASSERT_GT(task.memory_.workspaceArrays_.liveBytes_, 3 * sizeof(AAD::Number_));
        }
        ASSERT_EQ(forward, 257);
        ASSERT_EQ(payoff, 257);
        ASSERT_EQ(suffix, 257);
        ASSERT_EQ(prefix, batches.BatchCount());
        ASSERT_GE(data.memory_.resultArrays_.liveBytes_, (1 + 4) * (4 + 3) * sizeof(double));
    }
}
#endif

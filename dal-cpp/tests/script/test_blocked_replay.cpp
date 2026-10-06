//
// Created by Codex on 2026/10/06.
//

#include <gtest/gtest.h>

#include <dal/platform/platform.hpp>
#include <dal/script/blockedreplay.hpp>

#include <script_test_observers.hpp>

using namespace Dal;
using namespace Dal::Script;

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

    PreparedScript_
    ReplayProduct(const Handle_<ModelData_>& model, const String_& event, bool compiled = false, const Date_& eventDate = Date_(2027, 1, 1)) {
        const ScriptProductData_ data("replay", {Cell_(eventDate)}, {event});
        ScriptValuationSettings_ valuation;
        valuation.evaluationDate_ = Date_(2026, 1, 1);
        MonteCarloSettings_ simulation;
        simulation.enableAad_ = true;
        simulation.compiled_ = compiled;
        auto metadata = CreateModel<double>(model);
        return PrepareScript(data, metadata.get(), valuation, simulation);
    }

    Script::Detail::AADBatchSettings_ ReplayBatchSettings(const PreparedScript_& product) {
        return {product.Simulation().rsg_, product.Simulation().useBb_, -1, product.Simulation().smooth_, 17, 4, 0, product.PayOffIdx()};
    }

    struct MutateReplaySelection_ : Script::Detail::SimulationObserver_ {
        Vector_<RiskOutputCoordinate_>* outputs_;
        Vector_<size_t>* inputs_;
        Script::Detail::AADBatchSettings_* settings_;
        String_* method_ = nullptr;
        MutateReplaySelection_(Vector_<RiskOutputCoordinate_>* outputs, Vector_<size_t>* inputs, Script::Detail::AADBatchSettings_* settings)
            : outputs_(outputs), inputs_(inputs), settings_(settings) {}
        void AfterSubmission() override {
            outputs_->clear();
            inputs_->clear();
            settings_->nPaths_ = 1;
            if (method_)
                *method_ = "invalid-after-submission";
        }
    };

    void AssertReplayRows(const PreparedScript_& product,
                          const Handle_<ModelData_>& model,
                          const Script::Detail::AADBatchSettings_& settings,
                          const Vector_<RiskOutputCoordinate_>& outputs) {
        Script::Detail::AADBlockReplaySettings_ limits;
        limits.maxWidth_ = 3;
        limits.scratchBudgetBytes_ = 16 * 1024 * 1024;
        limits.tapeBudgetBytes_ = 128 * 1024 * 1024;
        const auto result = Script::Detail::EvaluateAADBlockReplay(product, model, settings, outputs, {1, 0}, limits);
        ASSERT_EQ(result.values_.size(), outputs.size());
        ASSERT_EQ(result.jacobian_.Rows(), outputs.size());
        ASSERT_EQ(result.jacobian_.Cols(), 2);
        ASSERT_EQ(result.replayAttempts_, (outputs.size() + 2) / 3);
        ASSERT_EQ(result.executedPaths_, settings.nPaths_ * result.replayAttempts_);
        ASSERT_GT(result.peakTapeBytes_, 0);
        ASSERT_LE(result.peakTapeBytes_, *limits.tapeBudgetBytes_);
        ASSERT_GT(result.peakScratchBytes_, 0);
        ASSERT_LE(result.peakScratchBytes_, *limits.scratchBudgetBytes_);
        const auto program = product.Simulation().compiled_.value_or(false) ? std::optional<ScriptCompiled_>(product.Compile(true)) : std::nullopt;
        for (size_t row = 0; row < outputs.size(); ++row) {
            auto mode = AAD::SetNumResultsForAAD(false, 1);
            auto scalarSettings = settings;
            scalarSettings.payoffIndex_ = outputs[row].slot_;
            SimResults_ scalar({"spot", "vol", "rate", "div"});
            Script::Detail::EvaluateAADBatch(product, model, scalarSettings, program, {0, settings.nPaths_}, &scalar);
            ASSERT_NEAR(result.values_[row], scalar.aggregated_ / static_cast<double>(settings.nPaths_), 1.0e-10);
            ASSERT_NEAR(result.jacobian_(static_cast<int>(row), 0), scalar.risks_[1], 1.0e-10);
            ASSERT_NEAR(result.jacobian_(static_cast<int>(row), 1), scalar.risks_[0], 1.0e-10);
        }
        const auto metadata = CreateModel<double>(model);
        Vector_<RiskCoordinate_> inputAxis;
        for (size_t input = 0; input < metadata->Parameters().size(); ++input)
            inputAxis.push_back({"model:" + String_(std::to_string(input)), metadata->ParameterLabels()[input], "model", input,
                                 *metadata->Parameters()[input], "model-coordinate", std::nullopt, 1.0});
        WeightedRiskRequest_ request;
        request.selection_.outputs_.emplace();
        request.weights_.emplace();
        for (size_t row = 0; row < outputs.size(); ++row) {
            request.selection_.outputs_->push_back(outputs[row].id_);
            request.weights_->push_back(row % 2 == 0 ? 0.5 : -0.25);
        }
        const auto weightedPlan = PlanWeightedRiskRequest(product.Product(), inputAxis, product.EvaluationDate(), request, true);
        const Script::Detail::WeightedSimulationObjective_ objective(weightedPlan);
        const auto weighted = MCAADSimulationWithObjective(product, model, settings.nPaths_, settings.rsg_, settings.useBb_,
                                                           product.Simulation().compiled_, -1, settings.eps_, objective);
        for (size_t column = 0; column < 2; ++column) {
            double dot = 0.0;
            for (size_t row = 0; row < outputs.size(); ++row)
                dot += (*request.weights_)[row] * result.jacobian_(static_cast<int>(row), static_cast<int>(column));
            ASSERT_NEAR(dot, weighted.risks_[1 - column], 1.0e-10);
        }
    }
} // namespace

TEST(BlockedReplayTest, TestCompleteCommonPathsAndOrderedRowsForOneAndFourWorkers) {
    String_ event;
    for (size_t row = 0; row < 63; ++row)
        event += "o" + String_(std::to_string(row)) + " = " + String_(std::to_string(row + 1)) + " * SPOT() ";
    event += "pay = 64 * SPOT() pay PAYS 0";
    const ScriptProductData_ data("replay", {Cell_(Date_(2027, 1, 1))}, {event});
    const Handle_<ModelData_> model(new BSModelData_("model", 100.0, 0.2));
    ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = Date_(2026, 1, 1);
    for (const size_t workers : {1, 4}) {
        ScopedThreads_ threads(workers);
        for (const bool compiled : {false, true}) {
            MonteCarloSettings_ simulation;
            simulation.enableAad_ = true;
            simulation.compiled_ = compiled;
            auto metadata = CreateModel<double>(model);
            const auto prepared = PrepareScript(data, metadata.get(), valuation, simulation);
            const auto axis = ScriptRiskOutputAxis(prepared.Product());
            const Script::Detail::AADBatchSettings_ settings{simulation.rsg_,     simulation.useBb_, -1, simulation.smooth_, 17, 4, 0,
                                                             prepared.PayOffIdx()};
            for (const size_t count : {1, 4, 16, 64}) {
                Vector_<RiskOutputCoordinate_> outputs(axis.begin(), axis.begin() + count);
                std::reverse(outputs.begin(), outputs.end());
                ASSERT_NO_FATAL_FAILURE(AssertReplayRows(prepared, model, settings, outputs));
            }
        }
    }
}

TEST(BlockedReplayTest, TestCallerSelectionsAreFrozenBeforeSubmission) {
    ScopedThreads_ threads(1);
    const Handle_<ModelData_> model(new BSModelData_("model", 100.0, 0.0));
    const auto product = ReplayProduct(model, "a = SPOT() pay PAYS 2 * a");
    auto outputs = ScriptRiskOutputAxis(product.Product());
    Vector_<size_t> inputs{0};
    String_ method = "sobol";
    Script::Detail::AADBatchSettings_ settings{method, true, -1, product.Simulation().smooth_, 17, 4, 0, product.PayOffIdx()};
    MutateReplaySelection_ mutation(&outputs, &inputs, &settings);
    mutation.method_ = &method;
    Script::Detail::ScopedSimulationObserver_ observer(&mutation);
    const auto result = Script::Detail::EvaluateAADBlockReplay(product, model, settings, outputs, inputs);
    ASSERT_TRUE(outputs.empty());
    ASSERT_TRUE(inputs.empty());
    ASSERT_EQ(settings.nPaths_, 1);
    ASSERT_EQ(method, "invalid-after-submission");
    ASSERT_EQ(result.values_.size(), 2);
    ASSERT_EQ(result.jacobian_.Cols(), 1);
    ASSERT_DOUBLE_EQ(result.values_[0], 100.0);
    ASSERT_DOUBLE_EQ(result.values_[1], 200.0);
    ASSERT_DOUBLE_EQ(result.jacobian_(0, 0), 1.0);
    ASSERT_DOUBLE_EQ(result.jacobian_(1, 0), 2.0);
    ASSERT_EQ(result.executedPaths_, 34);
}

TEST(BlockedReplayTest, TestActiveWaitIsolatesOtherRequestsAndUnbudgetedTasksDuringDrain) {
    ScopedThreads_ threads(1);
    const Handle_<ModelData_> model(new BSModelData_("model", 100.0, 0.0));
    const auto product = ReplayProduct(model, "a = SPOT() pay PAYS 2 * a", false, Date_(2026, 1, 1));
    const auto outputs = ScriptRiskOutputAxis(product.Product());
    const auto settings = ReplayBatchSettings(product);
    Script::Detail::AADBlockReplaySettings_ limits;
    limits.scratchBudgetBytes_ = 1024 * 1024;
    struct FailFirstSubmission_ : Script::Detail::SimulationObserver_ {
        bool fail_;
        size_t calls_ = 0;
        explicit FailFirstSubmission_(bool fail) : fail_(fail) {}
        void AfterSubmission() override {
            if (++calls_ == 1 && fail_)
                THROW("controlled first submission failure");
        }
    };
    for (const bool fail : {false, true}) {
        std::optional<Script::Detail::AADBlockReplayResult_> otherResult;
        auto otherRequest = ThreadPool_::GetInstance()->SpawnTask([&] {
            otherResult.emplace(Script::Detail::EvaluateAADBlockReplay(product, model, settings, outputs, {0}, limits));
            return true;
        });
        auto unbudgeted = ThreadPool_::GetInstance()->SpawnTask([] {
            Vector_<> values(2 * 1024 * 1024 / sizeof(double), 7.0);
            return values.front() == 7.0;
        });
        FailFirstSubmission_ observer(fail);
        {
            const Script::Detail::ScopedSimulationObserver_ observe(&observer);
            if (fail)
                ASSERT_THROW(Script::Detail::EvaluateAADBlockReplay(product, model, settings, outputs, {0}, limits), ScriptError_);
            else {
                const auto result = Script::Detail::EvaluateAADBlockReplay(product, model, settings, outputs, {0}, limits);
                ASSERT_EQ(result.values_, Vector_<>({100.0, 200.0}));
            }
        }
        ASSERT_TRUE(otherRequest.get());
        ASSERT_TRUE(unbudgeted.get());
        ASSERT_TRUE(otherResult.has_value());
        ASSERT_EQ(otherResult->values_, Vector_<>({100.0, 200.0}));
        ASSERT_LE(otherResult->peakScratchBytes_, *limits.scratchBudgetBytes_);
    }
}

TEST(BlockedReplayTest, TestKnownBudgetsAndInvalidAxesRejectBeforeSubmission) {
    ScopedThreads_ threads(4);
    const Handle_<ModelData_> model(new BSModelData_("model", 100.0, 0.2));
    const auto product = ReplayProduct(model, "a = SPOT() pay PAYS a");
    const auto outputs = ScriptRiskOutputAxis(product.Product());
    const auto settings = ReplayBatchSettings(product);
    TestSupport::SubmissionCounter_ counter;
    Script::Detail::ScopedSimulationObserver_ observer(&counter);
    Script::Detail::AADBlockReplaySettings_ limits;
    limits.tapeBudgetBytes_ = 0;
    ASSERT_THROW(Script::Detail::EvaluateAADBlockReplay(product, model, settings, outputs, {0}, limits), ScriptError_);
    limits.tapeBudgetBytes_.reset();
    limits.scratchBudgetBytes_ = 0;
    ASSERT_THROW(Script::Detail::EvaluateAADBlockReplay(product, model, settings, outputs, {0}, limits), ScriptError_);
    limits.scratchBudgetBytes_.reset();
    limits.numericResultBudgetBytes_ = 0;
    ASSERT_THROW(Script::Detail::EvaluateAADBlockReplay(product, model, settings, outputs, {0}, limits), Exception_);
    limits.numericResultBudgetBytes_.reset();
    ASSERT_THROW(Script::Detail::EvaluateAADBlockReplay(product, model, settings, {}, {0}, limits), Exception_);
    ASSERT_THROW(Script::Detail::EvaluateAADBlockReplay(product, model, settings, {outputs[0], outputs[0]}, {0}, limits), ScriptError_);
    ASSERT_THROW(Script::Detail::EvaluateAADBlockReplay(product, model, settings, outputs, {0, 0}, limits), ScriptError_);
    ASSERT_THROW(Script::Detail::EvaluateAADBlockReplay(product, model, settings, outputs, {4}, limits), ScriptError_);
    ASSERT_EQ(counter.submissions_, 0);
}

TEST(BlockedReplayTest, TestRuntimeCapacityAndSubmissionFailuresDrainAndPreserveResults) {
    ScopedThreads_ threads(1);
    const Handle_<ModelData_> model(new BSModelData_("model", 100.0, 0.2));
    const auto product = ReplayProduct(model, "a = SPOT() pay PAYS a");
    const auto outputs = ScriptRiskOutputAxis(product.Product());
    const auto settings = ReplayBatchSettings(product);
    const auto retained = Script::Detail::EvaluateAADBlockReplay(product, model, settings, outputs, {0, 1});
    const auto before = retained.values_;
    {
        TestSupport::SubmissionCounter_ counter;
        Script::Detail::ScopedSimulationObserver_ observer(&counter);
        Script::Detail::AADBlockReplaySettings_ limits;
        limits.scratchBudgetBytes_ = retained.peakScratchBytes_ - 1;
        try {
            static_cast<void>(Script::Detail::EvaluateAADBlockReplay(product, model, settings, outputs, {0, 1}, limits));
            FAIL() << "Expected blocked replay scratch exhaustion";
        } catch (const Exception_& error) {
            const std::string message = error.what();
            ASSERT_NE(message.find("blockFirst=0"), std::string::npos);
            ASSERT_NE(message.find("output=output:0"), std::string::npos);
            ASSERT_NE(message.find("Scratch buffer"), std::string::npos);
        }
        ASSERT_GT(counter.submissions_, 0);
    }
    {
        TestSupport::RejectSubmissions_ rejection("injected blocked replay submission failure");
        Script::Detail::ScopedSimulationObserver_ observer(&rejection);
        ASSERT_THROW(Script::Detail::EvaluateAADBlockReplay(product, model, settings, outputs, {0, 1}), Exception_);
        ASSERT_EQ(rejection.calls_, 1);
    }
    const auto recovered = Script::Detail::EvaluateAADBlockReplay(product, model, settings, outputs, {0, 1});
    ASSERT_EQ(retained.values_, before);
    ASSERT_EQ(recovered.values_, retained.values_);
    ASSERT_EQ(recovered.jacobian_.Rows(), retained.jacobian_.Rows());
    ASSERT_EQ(recovered.jacobian_.Cols(), retained.jacobian_.Cols());
    for (int row = 0; row < retained.jacobian_.Rows(); ++row)
        for (int column = 0; column < retained.jacobian_.Cols(); ++column)
            ASSERT_DOUBLE_EQ(recovered.jacobian_(row, column), retained.jacobian_(row, column));
}

TEST(BlockedReplayTest, TestEmptyInputColumnsKeepTheNativeFuzzyEstimator) {
    ScopedThreads_ threads(1);
    const Handle_<ModelData_> model(new BSModelData_("model", 100.0, 0.0));
    for (const bool compiled : {false, true}) {
        const auto product = ReplayProduct(model, "IF SPOT() > 100 THEN pay PAYS 1 ELSE pay PAYS 0 END", compiled, Date_(2026, 1, 1));
        const auto settings = ReplayBatchSettings(product);
        const auto result = Script::Detail::EvaluateAADBlockReplay(product, model, settings, ScriptRiskOutputAxis(product.Product()), {});
        ASSERT_EQ(result.values_.size(), 1);
        ASSERT_EQ(result.jacobian_.Rows(), 1);
        ASSERT_EQ(result.jacobian_.Cols(), 0);
        ASSERT_DOUBLE_EQ(result.values_[0], 0.5);
    }
}

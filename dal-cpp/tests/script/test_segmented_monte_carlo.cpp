//
// Created by Codex on 2026/10/09.
//

#include <gtest/gtest.h>

#include <cmath>
#include <future>
#include <limits>
#include <memory>

#include <dal/model/blackscholes.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/segmentedmontecarlo.hpp>
#include <dal/script/simulation.hpp>

#include <script_test_observers.hpp>

using Dal::Cell_;
using Dal::Date_;
using Dal::Vector_;
namespace AAD = Dal::AAD;
namespace Script = Dal::Script;

namespace {
    struct ScopedThreads_ {
        Dal::ThreadPool_* pool_ = Dal::ThreadPool_::GetInstance();
        size_t threads_ = pool_->NumThreads();
        bool active_ = pool_->IsActive();
        explicit ScopedThreads_(size_t threads) { pool_->Start(threads, true); }
        ~ScopedThreads_() {
            pool_->Start(threads_, true);
            if (!active_)
                pool_->Stop();
        }
    };

    std::shared_ptr<const Script::BlackScholesSegmentedPreparation_>
    PrepareMonteCarloProduct(const Script::ScriptProductData_& product, const Dal::Handle_<Dal::MarketFixingSnapshot_>& history = {}) {
        Script::ScriptValuationSettings_ valuation;
        valuation.evaluationDate_ = Date_(2026, 10, 1);
        return std::make_shared<const Script::BlackScholesSegmentedPreparation_>(
            Script::PrepareBlackScholesSegmentedScript(product, valuation, history));
    }

    std::shared_ptr<const Script::BlackScholesSegmentedPreparation_> PrepareMonteCarlo() {
        const Script::ScriptProductData_ product("", {Cell_("SCALE"), Cell_(Date_(2026, 10, 2)), Cell_(Date_(2026, 11, 3)), Cell_(Date_(2027, 1, 7))},
                                                 {"2", "x = FIX(EQ[DAL196_TEST])", "x = x", "pay PAYS SCALE * FIX(EQ[DAL196_TEST]) ON 2027-04-01"});
        return PrepareMonteCarloProduct(product);
    }

    Script::SimResults_ FullMonteCarlo(const Script::PreparedScript_& prepared,
                                       size_t paths,
                                       const Script::SegmentedMonteCarloSettings_& request,
                                       const Vector_<>& modelPoint = {100.0, 0.2, 0.03, 0.01}) {
        const auto mode = AAD::SetNumResultsForAAD(false, 1);
        const Dal::Handle_<Dal::ModelData_> model(new Dal::BSModelData_("", modelPoint[0], modelPoint[1], modelPoint[2], modelPoint[3]));
        auto labels = AAD::BlackScholes_<>(100.0, 0.2).ParameterLabels();
        labels.Append(prepared.ConstVarNames());
        Script::SimResults_ result(labels);
        const Script::Detail::AADBatchSettings_ settings{request.rsg_,
                                                         request.useBb_,
                                                         static_cast<int>(prepared.MaxNestedIfs()),
                                                         prepared.Simulation().smooth_,
                                                         paths,
                                                         4,
                                                         prepared.ConstVarNames().size(),
                                                         prepared.PayOffIdx()};
        const std::optional<Script::ScriptCompiled_> program(prepared.CompiledProgram(true));
        Script::Detail::EvaluateAADBatch(prepared, model, settings, program, {request.firstPath_, paths}, &result);
        result.aggregated_ /= static_cast<double>(paths);
        return result;
    }
    Dal::Handle_<Dal::MarketFixingSnapshot_> MonteCarloHistory() {
        return Dal::Handle_<Dal::MarketFixingSnapshot_>(
            new Dal::MarketFixingSnapshot_({{"EQ[DAL196_TEST]", {{Dal::DateTime_(Date_(2026, 9, 30), 0.0), 80.0}}}}));
    }

    struct LinearMean_ {
        double value_ = 0.0;
        Vector_<> risks_ = Vector_<>(5, 0.0);
    };

    LinearMean_ AnalyticLinearMean(const Script::PreparedScript_& prepared,
                                   const Vector_<>& parameters,
                                   size_t paths,
                                   const Script::SegmentedMonteCarloSettings_& settings) {
        const auto& times = prepared.TimeLine();
        auto random = Script::CreateRNG(settings.rsg_, times.size(), settings.useBb_, settings.scrambleKey_);
        random->SkipNormalTo(settings.firstPath_);
        Vector_<> gaussian(times.size());
        const double maturity = static_cast<double>(Date_(2027, 4, 1) - prepared.EvaluationDate()) / 365.0;
        LinearMean_ result;
        for (size_t path = 0; path < paths; ++path) {
            random->FillNormal(&gaussian);
            double brownian = 0.0;
            for (size_t i = 0; i < times.size(); ++i)
                brownian += std::sqrt(times[i] - (i == 0 ? 0.0 : times[i - 1])) * gaussian[i];
            const double time = times.back();
            const double unit = parameters[0] * std::exp((parameters[2] - parameters[3] - 0.5 * parameters[1] * parameters[1]) * time +
                                                         parameters[1] * brownian - parameters[2] * maturity);
            const double value = parameters[4] * unit;
            result.value_ += value / static_cast<double>(paths);
            const Vector_<> risks{value / parameters[0], value * (brownian - parameters[1] * time), value * (time - maturity), -time * value, unit};
            for (size_t i = 0; i < risks.size(); ++i)
                result.risks_[i] += risks[i] / static_cast<double>(paths);
        }
        return result;
    }

    struct MutateMonteCarloInputs_ : Script::Detail::SimulationObserver_ {
        Vector_<>* parameters_;
        Script::SegmentedMonteCarloSettings_* settings_;
        MutateMonteCarloInputs_(Vector_<>* parameters, Script::SegmentedMonteCarloSettings_* settings)
            : parameters_(parameters), settings_(settings) {}
        void AfterSubmission() override {
            *parameters_ = {1.0};
            settings_->rsg_ = "invalid-after-submission";
            settings_->firstPath_ = std::numeric_limits<size_t>::max();
            settings_->path_.segmentSteps_ = 0;
        }
    };
} // namespace

TEST(SegmentedMonteCarloTest, TestNonzeroOffsetAndTailAgainstCompleteNativeBatch) {
    const auto prepared = PrepareMonteCarlo();
    const Script::BlackScholesSegmentedPath_ kernel(prepared);
    Script::SegmentedMonteCarloSettings_ settings;
    settings.firstPath_ = 7;
    settings.path_.segmentSteps_ = 2;
    constexpr size_t PATHS = 259;
    const auto full = FullMonteCarlo(prepared->Prepared(), PATHS, settings);
    const auto result = Script::EvaluateBlackScholesSegmentedMonteCarlo(kernel, {100.0, 0.2, 0.03, 0.01, 2.0}, PATHS, settings);
    ASSERT_NEAR(result.MeanValue(), full.aggregated_, 1e-10);
    ASSERT_EQ(result.ParameterLabels(), full.names_);
    ASSERT_EQ(result.MeanGradient().size(), full.risks_.size());
    for (size_t i = 0; i < full.risks_.size(); ++i)
        ASSERT_NEAR(result.MeanGradient()[i], full.risks_[i], 1e-9);
    ASSERT_EQ(result.Execution().pathCount_, PATHS);
    ASSERT_EQ(result.Execution().firstPath_, 7);
    ASSERT_EQ(result.Execution().batchSize_, 32);
    ASSERT_EQ(result.Execution().batches_, 9);
    ASSERT_GT(result.Execution().maxPathTapeBytes_, 0);
    ASSERT_GT(result.Execution().maxPathCheckpointBytes_, 0);
    ASSERT_LE(result.Execution().maxPathCleanupReserveBytes_, result.Execution().maxPathTapeBytes_);
}

TEST(SegmentedMonteCarloTest, TestFuzzyVectorBranchesAndTimeZeroAgainstCompleteNativeBatch) {
    const auto prepared = PrepareMonteCarloProduct(
        Script::ScriptProductData_("", {Cell_("K"), Cell_("SCALE"), Cell_(Date_(2026, 10, 1)), Cell_(Date_(2026, 10, 2)), Cell_(Date_(2026, 10, 4))},
                                   {"100", "2", "APPEND(v, FIX(EQ[DAL196_TEST]))",
                                    "IF FIX(EQ[DAL196_TEST], 2026-10-01) = K:2 THEN APPEND(v, SCALE * FIX(EQ[DAL196_TEST])) ELSE v[3] = K END",
                                    "pay PAYS MAX(v) + MIN(v) + AVERAGE(v) + SUM(v)"}));
    const Script::BlackScholesSegmentedPath_ kernel(prepared);
    const Vector_<> parameters{100.0, 0.0, 0.0, 0.0, 100.0, 2.0};
    Script::SegmentedMonteCarloSettings_ settings;
    settings.firstPath_ = 7;
    settings.useBb_ = true;
    settings.path_.segmentSteps_ = 1;
    const auto full = FullMonteCarlo(prepared->Prepared(), 35, settings, parameters);
    const auto result = Script::EvaluateBlackScholesSegmentedMonteCarlo(kernel, parameters, 35, settings);
    ASSERT_NEAR(result.MeanValue(), full.aggregated_, 1e-10);
    ASSERT_EQ(result.ParameterLabels(), full.names_);
    for (size_t i = 0; i < full.risks_.size(); ++i)
        ASSERT_NEAR(result.MeanGradient()[i], full.risks_[i], 1e-9);
}

TEST(SegmentedMonteCarloTest, TestAllGeneratorsAndBridgeAgainstCompleteNativeBatch) {
    const auto prepared = PrepareMonteCarlo();
    const Script::BlackScholesSegmentedPath_ kernel(prepared);
    for (const Dal::String_ method : {"sobol", "mrg32", "irn"}) {
        for (bool bridge : {false, true}) {
            Script::SegmentedMonteCarloSettings_ settings;
            settings.rsg_ = method;
            settings.useBb_ = bridge;
            settings.firstPath_ = 7;
            settings.path_.segmentSteps_ = 2;
            const auto full = FullMonteCarlo(prepared->Prepared(), 259, settings);
            const auto result = Script::EvaluateBlackScholesSegmentedMonteCarlo(kernel, {100.0, 0.2, 0.03, 0.01, 2.0}, 259, settings);
            ASSERT_NEAR(result.MeanValue(), full.aggregated_, 1e-10);
            ASSERT_EQ(result.ParameterLabels(), full.names_);
            for (size_t i = 0; i < full.risks_.size(); ++i)
                ASSERT_NEAR(result.MeanGradient()[i], full.risks_[i], 1e-9);
            ASSERT_EQ(result.Execution().rsg_, method);
            ASSERT_EQ(result.Execution().useBb_, bridge);
        }
    }
}

TEST(SegmentedMonteCarloTest, TestTaskSubmissionAndWorkerCountInvariantReduction) {
    const auto prepared = PrepareMonteCarlo();
    const Script::BlackScholesSegmentedPath_ kernel(prepared);
    Script::SegmentedMonteCarloSettings_ settings;
    settings.firstPath_ = 7;
    settings.rsg_ = "irn";
    std::optional<Script::SegmentedMonteCarloResult_> reference;
    for (size_t threads : {1, 4}) {
        const ScopedThreads_ pool(threads);
        Script::TestSupport::SubmissionCounter_ submissions;
        const Script::Detail::ScopedSimulationObserver_ observer(&submissions);
        auto result = Script::EvaluateBlackScholesSegmentedMonteCarlo(kernel, {100.0, 0.2, 0.03, 0.01, 2.0}, 259, settings);
        ASSERT_EQ(submissions.submissions_, 9);
        ASSERT_EQ(result.Execution().lanes_, std::min(size_t(9), Dal::ThreadPool_::GetInstance()->NumThreads()));
        if (reference) {
            ASSERT_EQ(result.MeanValue(), reference->MeanValue());
            ASSERT_EQ(result.MeanGradient(), reference->MeanGradient());
        } else {
            reference = std::move(result);
        }
    }
}

TEST(SegmentedMonteCarloTest, TestFreshPointsSegmentLengthsAndDigitalShiftAgainstAnalyticRisks) {
    const auto prepared = PrepareMonteCarlo();
    const Script::BlackScholesSegmentedPath_ kernel(prepared);
    Script::SegmentedMonteCarloSettings_ settings;
    settings.firstPath_ = 13;
    settings.useBb_ = true;
    settings.scrambleKey_ = 12345;
    for (const Vector_<>& parameters : Vector_<Vector_<>>{{120.0, 0.0, -0.02, 0.03, 3.0}, {85.0, 0.35, 0.08, -0.01, -1.5}}) {
        const auto analytic = AnalyticLinearMean(prepared->Prepared(), parameters, 35, settings);
        for (size_t length : {1, 2, 8}) {
            settings.path_.segmentSteps_ = length;
            const auto result = Script::EvaluateBlackScholesSegmentedMonteCarlo(kernel, parameters, 35, settings);
            ASSERT_NEAR(result.MeanValue(), analytic.value_, 1e-10);
            for (size_t i = 0; i < parameters.size(); ++i)
                ASSERT_NEAR(result.MeanGradient()[i], analytic.risks_[i], 1e-9);
            ASSERT_EQ(result.Execution().scrambleKey_, settings.scrambleKey_);
            ASSERT_EQ(result.Execution().segmentSteps_, length);
        }
    }
}

TEST(SegmentedMonteCarloTest, TestHistoricalVectorsOldFixingsAndReadablePayments) {
    const auto prepared = PrepareMonteCarloProduct(
        Script::ScriptProductData_(
            "", {Cell_("SCALE"), Cell_(Date_(2026, 9, 30)), Cell_(Date_(2026, 10, 2)), Cell_(Date_(2026, 10, 4)), Cell_(Date_(2026, 10, 6))},
            {"2", "x = SCALE * FIX(EQ[DAL196_TEST]) APPEND(v, x)", "APPEND(v, FIX(EQ[DAL196_TEST])) z = 0 pay PAYS x ON 2026-12-01",
             "z = pay APPEND(v, FIX(EQ[DAL196_TEST], 2026-10-02))", "pay PAYS SUM(v) + z + FIX(EQ[DAL196_TEST], 2026-10-02)"}),
        MonteCarloHistory());
    const Script::BlackScholesSegmentedPath_ kernel(prepared);
    Script::SegmentedMonteCarloSettings_ settings;
    settings.rsg_ = "mrg32";
    settings.useBb_ = true;
    settings.firstPath_ = 13;
    const auto full = FullMonteCarlo(prepared->Prepared(), 35, settings);
    for (size_t length : {1, 2, 8}) {
        settings.path_.segmentSteps_ = length;
        const auto result = Script::EvaluateBlackScholesSegmentedMonteCarlo(kernel, {100.0, 0.2, 0.03, 0.01, 2.0}, 35, settings);
        ASSERT_NEAR(result.MeanValue(), full.aggregated_, 1e-10);
        for (size_t i = 0; i < full.risks_.size(); ++i)
            ASSERT_NEAR(result.MeanGradient()[i], full.risks_[i], 1e-9);
    }
}

TEST(SegmentedMonteCarloTest, TestTimeZeroAndHistoricalOnlyWithoutDrivers) {
    const ScopedThreads_ threads(1);
    Script::SegmentedMonteCarloSettings_ settings;
    settings.rsg_ = "irn";
    settings.useBb_ = true;
    settings.firstPath_ = 7;
    const auto today = PrepareMonteCarloProduct(Script::ScriptProductData_("", {Cell_(Date_(2026, 10, 1))}, {"pay PAYS FIX(EQ[DAL196_TEST])"}));
    const Script::BlackScholesSegmentedPath_ todayKernel(today);
    ASSERT_EQ(todayKernel.SimDim(), 0);
    const auto now = Script::EvaluateBlackScholesSegmentedMonteCarlo(todayKernel, {100.0, 0.2, 0.03, 0.01}, 35, settings);
    ASSERT_EQ(now.MeanValue(), 100.0);
    ASSERT_EQ(now.MeanGradient(), (Vector_<>{1.0, 0.0, 0.0, 0.0}));
    const auto historical =
        PrepareMonteCarloProduct(Script::ScriptProductData_("", {Cell_("SCALE"), Cell_(Date_(2026, 9, 30))},
                                                            {"2", "APPEND(v, SCALE * FIX(EQ[DAL196_TEST])) pay PAYS 0 pay = SUM(v)"}),
                                 MonteCarloHistory());
    const Script::BlackScholesSegmentedPath_ historyKernel(historical);
    ASSERT_EQ(historyKernel.SimDim(), 0);
    Script::TestSupport::RejectFixingReads_ reads;
    const Dal::Detail::ScopedFixingReadObserver_ observer(&reads);
    const auto past = Script::EvaluateBlackScholesSegmentedMonteCarlo(historyKernel, {100.0, 0.2, 0.03, 0.01, -3.0}, 35, settings);
    ASSERT_EQ(past.MeanValue(), -240.0);
    ASSERT_EQ(past.MeanGradient(), (Vector_<>{0.0, 0.0, 0.0, 0.0, 80.0}));
    if constexpr (std::numeric_limits<size_t>::max() > std::numeric_limits<std::uint32_t>::max()) {
        settings.firstPath_ = std::numeric_limits<std::uint32_t>::max();
        for (const Dal::String_ method : {"sobol", "mrg32", "irn"}) {
            settings.rsg_ = method;
            const auto distantNow = Script::EvaluateBlackScholesSegmentedMonteCarlo(todayKernel, {100.0, 0.2, 0.03, 0.01}, 35, settings);
            const auto distantPast = Script::EvaluateBlackScholesSegmentedMonteCarlo(historyKernel, {100.0, 0.2, 0.03, 0.01, -3.0}, 35, settings);
            ASSERT_EQ(distantNow.MeanValue(), now.MeanValue());
            ASSERT_EQ(distantNow.MeanGradient(), now.MeanGradient());
            ASSERT_EQ(distantPast.MeanValue(), past.MeanValue());
            ASSERT_EQ(distantPast.MeanGradient(), past.MeanGradient());
            ASSERT_EQ(distantPast.Execution().firstPath_, settings.firstPath_);
        }
    }
    settings.firstPath_ = std::numeric_limits<size_t>::max();
    for (const Dal::String_ method : {"sobol", "mrg32", "irn"}) {
        settings.rsg_ = method;
        const auto finalNow = Script::EvaluateBlackScholesSegmentedMonteCarlo(todayKernel, {100.0, 0.2, 0.03, 0.01}, 1, settings);
        const auto finalPast = Script::EvaluateBlackScholesSegmentedMonteCarlo(historyKernel, {100.0, 0.2, 0.03, 0.01, -3.0}, 1, settings);
        ASSERT_EQ(finalNow.MeanValue(), now.MeanValue());
        ASSERT_EQ(finalNow.MeanGradient(), now.MeanGradient());
        ASSERT_EQ(finalPast.MeanValue(), past.MeanValue());
        ASSERT_EQ(finalPast.MeanGradient(), past.MeanGradient());
        ASSERT_EQ(finalPast.Execution().firstPath_, settings.firstPath_);
        ASSERT_EQ(finalPast.Execution().pathCount_, 1);
        ASSERT_THROW((void)Script::EvaluateBlackScholesSegmentedMonteCarlo(todayKernel, {100.0, 0.2, 0.03, 0.01}, 2, settings), Dal::Exception_);
        ASSERT_THROW((void)Script::EvaluateBlackScholesSegmentedMonteCarlo(historyKernel, {100.0, 0.2, 0.03, 0.01, -3.0}, 2, settings),
                     Dal::Exception_);
    }
}

TEST(SegmentedMonteCarloTest, TestInvalidAdmissionHasNoSubmissionsAndPreservesOuterRecording) {
    const auto prepared = PrepareMonteCarlo();
    const Script::BlackScholesSegmentedPath_ kernel(prepared);
    const Vector_<> parameters{100.0, 0.2, 0.03, 0.01, 2.0};
    Script::TestSupport::SubmissionCounter_ submissions;
    const Script::Detail::ScopedSimulationObserver_ observer(&submissions);
    ASSERT_THROW((void)Script::EvaluateBlackScholesSegmentedMonteCarlo(kernel, parameters, 0), Dal::Exception_);
    ASSERT_THROW((void)Script::EvaluateBlackScholesSegmentedMonteCarlo(kernel, {}, 1), Dal::Exception_);
    for (size_t column : {0, 1, 4}) {
        auto invalid = parameters;
        invalid[column] = std::numeric_limits<double>::quiet_NaN();
        ASSERT_THROW((void)Script::EvaluateBlackScholesSegmentedMonteCarlo(kernel, invalid, 1), Dal::Exception_);
    }
    Script::SegmentedMonteCarloSettings_ settings;
    settings.rsg_ = "invalid";
    ASSERT_THROW((void)Script::EvaluateBlackScholesSegmentedMonteCarlo(kernel, parameters, 1, settings), Dal::Exception_);
    settings.rsg_ = "irn";
    settings.scrambleKey_ = 1;
    ASSERT_THROW((void)Script::EvaluateBlackScholesSegmentedMonteCarlo(kernel, parameters, 1, settings), Dal::Exception_);
    settings.scrambleKey_.reset();
    settings.firstPath_ = std::numeric_limits<size_t>::max();
    ASSERT_THROW((void)Script::EvaluateBlackScholesSegmentedMonteCarlo(kernel, parameters, 1, settings), Dal::Exception_);
    settings.firstPath_ = std::numeric_limits<size_t>::max() / kernel.SimDim();
    ASSERT_THROW((void)Script::EvaluateBlackScholesSegmentedMonteCarlo(kernel, parameters, 1, settings), Dal::Exception_);
    settings.rsg_ = "sobol";
    settings.firstPath_ = std::numeric_limits<std::uint32_t>::max();
    ASSERT_THROW((void)Script::EvaluateBlackScholesSegmentedMonteCarlo(kernel, parameters, 1, settings), Dal::Exception_);
    settings.firstPath_ = 0;
    settings.path_.segmentSteps_ = 0;
    ASSERT_THROW((void)Script::EvaluateBlackScholesSegmentedMonteCarlo(kernel, parameters, 1, settings), Dal::Exception_);
    settings.path_.segmentSteps_ = 1;
    settings.path_.checkpointCapacityBudgetBytes_ = 0;
    ASSERT_THROW((void)Script::EvaluateBlackScholesSegmentedMonteCarlo(kernel, parameters, 1, settings), Dal::Exception_);
    settings.path_.checkpointCapacityBudgetBytes_.reset();
    const auto mode = AAD::SetNumResultsForAAD(true, 4);
    AAD::RecordingScope_ outer;
    AAD::Number_ input;
    outer.RegisterInput(input, 3.0);
    outer.StartRecording();
    AAD::Number_ root = input * input;
    ASSERT_THROW((void)Script::EvaluateBlackScholesSegmentedMonteCarlo(kernel, parameters, 1, settings), Dal::Exception_);
    outer.FinishRecording();
    AAD::NativeOperations_::SetSeed(root, 1.0, 2);
    outer.Reverse();
    ASSERT_EQ(AAD::NativeOperations_::ReadAdjoint(input, 2), 6.0);
    outer.Close();
    ASSERT_TRUE(AAD::Tape()->multi_);
    ASSERT_EQ(AAD::Tape()->numAdj_, 4);
    ASSERT_EQ(submissions.submissions_, 0);
}

TEST(SegmentedMonteCarloTest, TestSubmissionWorkerBudgetAndAggregateFailuresRecover) {
    const ScopedThreads_ threads(4);
    const auto prepared = PrepareMonteCarlo();
    const Script::BlackScholesSegmentedPath_ kernel(prepared);
    const Vector_<> parameters{100.0, 0.2, 0.03, 0.01, 2.0};
    const auto expected = Script::EvaluateBlackScholesSegmentedMonteCarlo(kernel, parameters, 65);
    const auto mode = AAD::SetNumResultsForAAD(true, 4);
    {
        Script::TestSupport::RejectSubmissions_ reject;
        const Script::Detail::ScopedSimulationObserver_ observer(&reject);
        ASSERT_THROW((void)Script::EvaluateBlackScholesSegmentedMonteCarlo(kernel, parameters, 65), Dal::Exception_);
        ASSERT_EQ(reject.calls_, 1);
    }
    Script::SegmentedMonteCarloSettings_ settings;
    settings.path_.recordingCapacityBudgetBytes_ = 0;
    ASSERT_THROW((void)Script::EvaluateBlackScholesSegmentedMonteCarlo(kernel, parameters, 65, settings), Dal::Exception_);
    ASSERT_THROW((void)Script::EvaluateBlackScholesSegmentedMonteCarlo(kernel, {100.0, 0.2, 1e308, 0.01, 2.0}, 65), Dal::Exception_);
    ASSERT_THROW((void)Script::EvaluateBlackScholesSegmentedMonteCarlo(kernel, {4e306, 0.0, 0.0, 0.0, 1.0}, 65), Dal::Exception_);
    const auto recovered = Script::EvaluateBlackScholesSegmentedMonteCarlo(kernel, parameters, 65);
    ASSERT_EQ(recovered.MeanValue(), expected.MeanValue());
    ASSERT_EQ(recovered.MeanGradient(), expected.MeanGradient());
    ASSERT_TRUE(AAD::Tape()->multi_);
    ASSERT_EQ(AAD::Tape()->numAdj_, 4);
}

TEST(SegmentedMonteCarloTest, TestCallerInputMutationCannotChangeAdmittedRequest) {
    const ScopedThreads_ threads(1);
    const auto prepared = PrepareMonteCarlo();
    const Script::BlackScholesSegmentedPath_ kernel(prepared);
    Vector_<> parameters{100.0, 0.2, 0.03, 0.01, 2.0};
    Script::SegmentedMonteCarloSettings_ settings;
    settings.firstPath_ = 7;
    const auto expected = Script::EvaluateBlackScholesSegmentedMonteCarlo(kernel, parameters, 65, settings);
    MutateMonteCarloInputs_ mutate(&parameters, &settings);
    const Script::Detail::ScopedSimulationObserver_ observer(&mutate);
    const auto result = Script::EvaluateBlackScholesSegmentedMonteCarlo(kernel, parameters, 65, settings);
    ASSERT_EQ(parameters.size(), 1);
    ASSERT_EQ(result.MeanValue(), expected.MeanValue());
    ASSERT_EQ(result.MeanGradient(), expected.MeanGradient());
    ASSERT_EQ(result.Execution().firstPath_, 7);
    ASSERT_EQ(result.Execution().rsg_, "sobol");
}

TEST(SegmentedMonteCarloTest, TestConcurrentCallersAndDetachedResults) {
    const ScopedThreads_ threads(4);
    const auto prepared = PrepareMonteCarlo();
    const Script::BlackScholesSegmentedPath_ kernel(prepared);
    const auto evaluate = [&]() { return Script::EvaluateBlackScholesSegmentedMonteCarlo(kernel, {100.0, 0.2, 0.03, 0.01, 2.0}, 65); };
    const auto expected = evaluate();
    auto first = std::async(std::launch::async, evaluate);
    auto second = std::async(std::launch::async, evaluate);
    const auto one = first.get();
    const auto two = second.get();
    for (const auto* result : {&one, &two}) {
        ASSERT_EQ(result->MeanValue(), expected.MeanValue());
        ASSERT_EQ(result->MeanGradient(), expected.MeanGradient());
    }
    const auto detached = []() {
        const auto owner = PrepareMonteCarlo();
        const Script::BlackScholesSegmentedPath_ local(owner);
        return Script::EvaluateBlackScholesSegmentedMonteCarlo(local, {100.0, 0.2, 0.03, 0.01, 2.0}, 65);
    }();
    ASSERT_EQ(detached.MeanValue(), expected.MeanValue());
    ASSERT_EQ(detached.MeanGradient(), expected.MeanGradient());
    ASSERT_EQ(detached.ParameterLabels(), expected.ParameterLabels());
}

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <limits>
#include <memory>

#include <dal/math/aad/profiling.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/detail/traininggaussians.hpp>
#include <dal/script/lsmccurvature.hpp>
#include <dal/script/simulation.hpp>

using Dal::Cell_;
using Dal::Date_;
using Dal::Vector_;
namespace AAD = Dal::AAD;
namespace Script = Dal::Script;

namespace {
    struct ReplayThreads_ {
        Dal::ThreadPool_* pool_ = Dal::ThreadPool_::GetInstance();
        size_t threads_ = pool_->NumThreads();
        bool active_ = pool_->IsActive();
        explicit ReplayThreads_(size_t threads) { pool_->Start(threads, true); }
        ~ReplayThreads_() {
            pool_->Start(threads_, true);
            if (!active_)
                pool_->Stop();
        }
    };

    struct ReplaySubmissions_ final : Script::Detail::SimulationObserver_ {
        std::atomic<size_t> calls_{0};
        void AfterSubmission() override { ++calls_; }
    };

    template <class F_> size_t CountReplaySubmissions(const F_& work) {
        ReplaySubmissions_ observer;
        const Script::Detail::ScopedSimulationObserver_ scope(&observer);
        static_cast<void>(work());
        return observer.calls_.load();
    }

    Script::ScriptProductData_ ReplayProduct() {
        return Script::ScriptProductData_("", {Cell_("K"), Cell_(Date_(2026, 10, 9)), Cell_(Date_(2027, 4, 10)), Cell_(Date_(2027, 10, 10))},
                                          {"55", "x = 2 * K", "EXERCISE x - spot()", "EXERCISE x - spot()"});
    }

    Dal::Handle_<Dal::ModelData_> ReplayModel() { return Dal::Handle_<Dal::ModelData_>(new Dal::BSModelData_("", 100.0, 0.2, 0.05, 0.0)); }

    Script::ScriptValuationSettings_ ReplayValuation() {
        Script::ScriptValuationSettings_ valuation;
        valuation.evaluationDate_ = Date_(2026, 10, 10);
        return valuation;
    }

    Script::MonteCarloSettings_ ReplaySettings(bool compiled, size_t validation = 0) {
        Script::MonteCarloSettings_ settings;
        settings.enableAad_ = true;
        settings.compiled_ = compiled;
        settings.smooth_ = 2.0;
        settings.lsmcBasisDegree_ = 1;
        settings.lsmcTrainingPaths_ = 128;
        if (validation > 0)
            settings.lsmcValidationPaths_ = validation;
        return settings;
    }

    std::shared_ptr<const Script::PreparedScript_> ReplayPreparation(bool compiled, size_t validation = 0) {
        auto model = Dal::CreateModel<double>(ReplayModel());
        return std::make_shared<const Script::PreparedScript_>(
            Script::PrepareScript(ReplayProduct(), model.get(), ReplayValuation(), ReplaySettings(compiled, validation)));
    }

    size_t HardReplayControlSubmissions(bool compiled) {
        auto settings = ReplaySettings(compiled);
        settings.enableAad_ = false;
        return CountReplaySubmissions([&] { return Script::MCSimulation<double>(ReplayProduct(), ReplayModel(), 1, ReplayValuation(), settings); });
    }

    AAD::BumpOverAADRequest_ ReplaySpotDirection(bool empty = false) {
        AAD::BumpOverAADRequest_ request;
        request.directions_ = Dal::Matrix_<>(empty ? 0 : 1, 5, 0.0);
        if (!empty) {
            request.directions_(0, 0) = 1.0;
            request.steps_ = {0.1};
        }
        return request;
    }
} // namespace

TEST(ScriptExerciseLSMCTest, TestLsmcReplaySubmitsPersistentWorkersWithoutChangingTraining) {
    constexpr size_t PATHS = 3 * 8192 + 1;
    for (size_t threads : {1, 3, 8}) {
        const ReplayThreads_ pool(threads);
        const Script::BatchPlan_ batches(PATHS, 1);
        const size_t workers = std::min(pool.pool_->NumThreads(), batches.BatchCount());
        for (bool compiled : {false, true}) {
            SCOPED_TRACE(::testing::Message() << "threads=" << threads << "; compiled=" << compiled);
            const size_t control = HardReplayControlSubmissions(compiled);
            ASSERT_GT(control, 0u);
            const auto settings = ReplaySettings(compiled);
            const size_t actual = CountReplaySubmissions(
                [&] { return Script::MCSimulation<AAD::Number_>(ReplayProduct(), ReplayModel(), PATHS, ReplayValuation(), settings); });
            ASSERT_EQ(actual, control - 1 + workers);
        }
    }
}

TEST(ScriptExerciseLSMCTest, TestLsmcCurvatureSubmitsPersistentWorkersPerGradient) {
    const ReplayThreads_ pool(1);
    constexpr size_t PATHS = 3 * 8192 + 1;
    for (bool compiled : {false, true}) {
        SCOPED_TRACE(compiled);
        const size_t control = HardReplayControlSubmissions(compiled);
        ASSERT_GT(control, 0u);
        const auto prepared = ReplayPreparation(compiled);
        const auto request = ReplaySpotDirection();
        const size_t actual = CountReplaySubmissions(
            [&] { return Script::EvaluateBlackScholesLsmcCurvature(prepared, {100.0, 0.2, 0.05, 0.0, 55.0}, PATHS, request); });
        ASSERT_EQ(actual, control - 1 + 3);
    }
}

TEST(ScriptExerciseLSMCTest, TestLsmcPrefixMatchesIndependentFreshSegments) {
    const ReplayThreads_ pool(1);
    constexpr size_t PATHS = 2 * 8192 + 1;
    for (bool compiled : {false, true}) {
        SCOPED_TRACE(compiled);
        const auto total = Script::MCSimulation<AAD::Number_>(ReplayProduct(), ReplayModel(), PATHS, ReplayValuation(), ReplaySettings(compiled));
        double sum = 0.0;
        Vector_<> risks(total.risks_.size(), 0.0);
        Vector_<Script::ExerciseRegression_> referencePolicy;
        for (size_t segment = 0; segment < 3; ++segment) {
            const size_t count = segment == 2 ? 1 : 8192;
            const auto prepared = ReplayPreparation(compiled, segment * 8192);
            const auto policy = Script::EvaluateBlackScholesLsmcCurvature(prepared, {100.0, 0.2, 0.05, 0.0, 55.0}, 1, ReplaySpotDirection(true));
            if (segment == 0)
                referencePolicy = policy.BasePolicy();
            ASSERT_EQ(policy.BasePolicy().size(), referencePolicy.size());
            for (size_t date = 0; date < referencePolicy.size(); ++date) {
                ASSERT_EQ(policy.BasePolicy()[date].coefficients_, referencePolicy[date].coefficients_);
                ASSERT_EQ(policy.BasePolicy()[date].means_, referencePolicy[date].means_);
                ASSERT_EQ(policy.BasePolicy()[date].sigmas_, referencePolicy[date].sigmas_);
                ASSERT_EQ(policy.BasePolicy()[date].mean_, referencePolicy[date].mean_);
                ASSERT_EQ(policy.BasePolicy()[date].sigma_, referencePolicy[date].sigma_);
                ASSERT_EQ(policy.BasePolicy()[date].basisDegree_, referencePolicy[date].basisDegree_);
            }
            const auto fresh = Script::MCLsmcAadSimulation(*prepared, ReplayModel(), count);
            ASSERT_EQ(fresh.names_, total.names_);
            sum += fresh.aggregated_;
            for (size_t input = 0; input < risks.size(); ++input)
                risks[input] += fresh.risks_[input] * static_cast<double>(count);
        }
        ASSERT_EQ(sum, total.aggregated_);
        for (size_t input = 0; input < risks.size(); ++input)
            ASSERT_EQ(risks[input] / static_cast<double>(PATHS), total.risks_[input]);
        ASSERT_GT(total["K"], 0.0);
    }
}

#if defined(DAL_ENABLE_AAD_PROFILING)
TEST(ScriptExerciseLSMCTest, TestLsmcReplayProfilesWorkersAndBatchReversals) {
    const ReplayThreads_ pool(1);
    constexpr size_t PATHS = 3 * 8192 + 1;
    const auto settings = ReplaySettings(true);
    AAD::ProfilingData_ data;
    {
        AAD::ProfilingScope_ profile(&data);
        static_cast<void>(Script::MCSimulation<AAD::Number_>(ReplayProduct(), ReplayModel(), PATHS, ReplayValuation(), settings));
    }
    ASSERT_TRUE(data.complete_);
    ASSERT_FALSE(data.taskGroups_.empty());
    const auto& replay = data.taskGroups_.back();
    ASSERT_EQ(replay.size(), 1);
    std::uint64_t forward = 0, suffix = 0, prefix = 0, samples = 0;
    for (const auto& worker : replay) {
        ASSERT_TRUE(worker.complete_);
        ASSERT_EQ(worker.Phase(AAD::AADProfilingPhase_::Value_::WORKER_INIT).calls_, 1);
        forward += worker.Phase(AAD::AADProfilingPhase_::Value_::PATH_FORWARD).calls_;
        suffix += worker.Phase(AAD::AADProfilingPhase_::Value_::REVERSE_SUFFIX).calls_;
        prefix += worker.Phase(AAD::AADProfilingPhase_::Value_::REVERSE_PREFIX).calls_;
        samples += worker.tapeSamples_;
    }
    ASSERT_EQ(forward, PATHS);
    ASSERT_EQ(suffix, PATHS);
    ASSERT_EQ(prefix, Script::BatchPlan_(PATHS, 1).BatchCount());
    ASSERT_EQ(samples, PATHS + 1);
}
#endif

namespace {
    void AssertReplayEqual(const Script::SimResults_& actual, const Script::SimResults_& expected) {
        ASSERT_EQ(actual.aggregated_, expected.aggregated_);
        ASSERT_EQ(actual.names_, expected.names_);
        ASSERT_EQ(actual.risks_, expected.risks_);
    }

    struct LiveReplayWorker_ {
        std::atomic<size_t>* live_;
        explicit LiveReplayWorker_(std::atomic<size_t>* live) : live_(live) { ++*live_; }
        ~LiveReplayWorker_() { --*live_; }
    };

    struct LegacyReplayResult_ {
        double sum_;
        double paths_;
        size_t batches_;
        size_t initialized_;
    };

    LegacyReplayResult_ RunOneSlotLegacyReplay(bool fail, std::atomic<size_t>* live) {
        const Script::BatchPlan_ plan(3 * 8192 + 1, 1);
        Script::Detail::OrderedChunkResults_<Script::Detail::AADBatchValues_> partials(
            Script::Detail::MakeSimulationPartials(1, [] { return Script::Detail::AADBatchValues_(1); }));
        const double values[] = {1e16, 1.0, -1e16, 3.0};
        std::atomic<size_t> initialized{0};
        LegacyReplayResult_ result{0.0, 0.0, 0, 0};
        Script::Detail::RunChunkWorkers(
            Dal::ThreadPool_::GetInstance(), plan, &partials, nullptr,
            [&](const auto& run) {
                const LiveReplayWorker_ lifetime(live);
                ++initialized;
                run([&](const Script::PathBatch_& batch, auto* partial, std::unique_ptr<Dal::Random_>) {
                    partial->aggregated_ += values[batch.firstPath_ / 8192];
                    partial->risks_[0] += static_cast<double>(batch.pathCount_);
                });
            },
            [&](const auto& partial) {
                if (fail && result.batches_ == 1)
                    THROW("legacy replay reducer failure");
                result.sum_ += partial.aggregated_;
                result.paths_ += partial.risks_[0];
                ++result.batches_;
            });
        result.initialized_ = initialized.load();
        return result;
    }
} // namespace

TEST(ScriptExerciseLSMCTest, TestLsmcPersistentReplayIsExactAcrossWorkersAndReplicates) {
    for (bool compiled : {false, true}) {
        auto settings = ReplaySettings(compiled, 128);
        settings.lsmcRqmcReplicates_ = 2;
        settings.lsmcTrainingSeed_ = 17;
        settings.lsmcPricingSeed_ = 29;
        settings.normalPrecision_ = "Precise";
        settings.useBb_ = true;
        Script::SimResults_ reference(Vector_<Dal::String_>{});
        for (size_t threads : {1, 3, 8}) {
            const ReplayThreads_ pool(threads);
            for (size_t repeat = 0; repeat < 3; ++repeat) {
                SCOPED_TRACE(::testing::Message() << "compiled=" << compiled << "; threads=" << threads << "; repeat=" << repeat);
                const auto result = Script::MCSimulation<AAD::Number_>(ReplayProduct(), ReplayModel(), 2 * 8192 + 1, ReplayValuation(), settings);
                if (threads == 1 && repeat == 0)
                    reference = result;
                AssertReplayEqual(result, reference);
            }
        }
    }
}

TEST(ScriptExerciseLSMCTest, TestLsmcRetrainedValueWorkersAreExactAcrossMultipleBatches) {
    for (bool compiled : {false, true}) {
        auto settings = ReplaySettings(compiled, 128);
        settings.lsmcRqmcReplicates_ = 2;
        settings.lsmcTrainingSeed_ = 7;
        settings.lsmcPricingSeed_ = 11;
        settings.lsmcPolicyRiskMode_ = "RetrainedBump";
        Script::SimResults_ reference(Vector_<Dal::String_>{});
        for (size_t threads : {1, 3, 8}) {
            const ReplayThreads_ pool(threads);
            SCOPED_TRACE(::testing::Message() << "compiled=" << compiled << "; threads=" << threads);
            const auto result = Script::MCSimulation<AAD::Number_>(ReplayProduct(), ReplayModel(), 3 * 8192 + 1, ReplayValuation(), settings);
            if (threads == 1)
                reference = result;
            AssertReplayEqual(result, reference);
        }
    }
}

TEST(ScriptExerciseLSMCTest, TestRetrainedTrainingGaussiansMatchUncachedCurvatureCaller) {
    const ReplayThreads_ pool(3);
    for (bool compiled : {false, true}) {
        for (const Dal::String_ precision : {"Fast", "Precise"}) {
            for (bool bridge : {false, true}) {
                SCOPED_TRACE(::testing::Message() << compiled << "; " << precision << "; bridge=" << bridge);
                auto settings = ReplaySettings(compiled, 33);
                settings.useBb_ = bridge;
                settings.normalPrecision_ = precision;
                settings.lsmcTrainingPaths_ = 8193;
                settings.lsmcPolicyRiskMode_ = "RetrainedBump";
                settings.lsmcRqmcReplicates_ = 2;
                settings.lsmcTrainingSeed_ = 17;
                settings.lsmcPricingSeed_ = 29;
                auto model = Dal::CreateModel<double>(ReplayModel());
                const auto prepared =
                    std::make_shared<const Script::PreparedScript_>(Script::PrepareScript(ReplayProduct(), model.get(), ReplayValuation(), settings));
                const auto cached = Script::MCLsmcAadSimulation(*prepared, ReplayModel(), 129);
                const auto uncached =
                    Script::EvaluateBlackScholesLsmcCurvature(prepared, {100.0, 0.2, 0.05, 0.0, 55.0}, 129, ReplaySpotDirection(true));
                ASSERT_EQ(cached.aggregated_ / 129.0, uncached.Value());
                ASSERT_EQ(cached.risks_, uncached.Gradient());
            }
        }
    }
}

TEST(ScriptExerciseLSMCTest, TestRetrainedTrainingGaussianStorageBoundsAndIdentity) {
    using Script::Detail::TrainingGaussians_;
    constexpr size_t DIMENSIONS = 4;
    constexpr size_t PATHS = TrainingGaussians_::MAX_BYTES / sizeof(double) / DIMENSIONS;
    const TrainingGaussians_ full(DIMENSIONS, 2, PATHS, 17);
    ASSERT_EQ(full.Bytes(), TrainingGaussians_::MAX_BYTES);
    ASSERT_TRUE(full.Matches(DIMENSIONS, 2, 17));
    ASSERT_FALSE(full.Matches(DIMENSIONS + 1, 2, 17));
    ASSERT_FALSE(full.Matches(DIMENSIONS, 1, 17));
    ASSERT_FALSE(full.Matches(DIMENSIONS, 2, 29));
    ASSERT_FALSE(full.Matches(DIMENSIONS, 2, std::nullopt));
    for (const auto shape : {std::pair<size_t, size_t>{DIMENSIONS, PATHS + 1},
                             {0, PATHS},
                             {std::numeric_limits<size_t>::max(), 2},
                             {2, std::numeric_limits<size_t>::max()}}) {
        const TrainingGaussians_ oversized(shape.first, 1, shape.second, std::nullopt);
        ASSERT_EQ(oversized.Bytes(), 0u);
        ASSERT_FALSE(oversized.Matches(shape.first, 1, std::nullopt));
    }
    TrainingGaussians_ cache(DIMENSIONS, 1, 3, 17);
    auto generator = Script::CreateRNG("sobol", DIMENSIONS, true, 17, "Precise");
    Vector_<Vector_<>> expected(3, Vector_<>(DIMENSIONS));
    for (size_t path = 0; path < expected.size(); ++path)
        cache.FillNormal(generator.get(), path, &expected[path]);
    cache.Complete();
    for (size_t path : {2, 0, 1}) {
        Vector_<> actual(DIMENSIONS);
        cache.FillNormal(nullptr, path, &actual);
        ASSERT_EQ(actual, expected[path]);
    }
    Vector_<> invalid(DIMENSIONS - 1);
    ASSERT_THROW(cache.FillNormal(nullptr, 0, &invalid), Dal::Exception_);
    Vector_<> valid(DIMENSIONS);
    ASSERT_THROW(cache.FillNormal(nullptr, 3, &valid), Dal::Exception_);
}

TEST(ScriptExerciseLSMCTest, TestRetrainedTrainingGaussianFailuresDrainAndRecover) {
    for (size_t threads : {1, 3}) {
        const ReplayThreads_ pool(threads);
        for (bool compiled : {false, true}) {
            SCOPED_TRACE(::testing::Message() << threads << "; " << compiled);
            auto settings = ReplaySettings(compiled, 33);
            settings.lsmcTrainingPaths_ = 8193;
            const size_t frozenSubmissions = CountReplaySubmissions(
                [&] { return Script::MCSimulation<AAD::Number_>(ReplayProduct(), ReplayModel(), 129, ReplayValuation(), settings); });
            settings.lsmcPolicyRiskMode_ = "RetrainedBump";
            const auto expected = Script::MCSimulation<AAD::Number_>(ReplayProduct(), ReplayModel(), 129, ReplayValuation(), settings);
            for (size_t offset : {1, 3, 6}) {
                struct FailTraining_ final : Script::Detail::SimulationObserver_ {
                    size_t calls_ = 0;
                    size_t failAt_;
                    explicit FailTraining_(size_t failAt) : failAt_(failAt) {}
                    void AfterSubmission() override {
                        if (++calls_ == failAt_)
                            THROW("retrained training submission failure");
                    }
                } observer(frozenSubmissions + offset);
                {
                    const Script::Detail::ScopedSimulationObserver_ scope(&observer);
                    ASSERT_THROW(
                        static_cast<void>(Script::MCSimulation<AAD::Number_>(ReplayProduct(), ReplayModel(), 129, ReplayValuation(), settings)),
                        Dal::Exception_);
                }
                ASSERT_EQ(observer.calls_, frozenSubmissions + offset);
                AssertReplayEqual(Script::MCSimulation<AAD::Number_>(ReplayProduct(), ReplayModel(), 129, ReplayValuation(), settings), expected);
            }
            for (const Dal::String_ condition : {"spot() > 100.05", "K > 55.02"}) {
                SCOPED_TRACE(condition);
                const Script::ScriptProductData_ failing("", {Cell_("K"), Cell_(Date_(2027, 10, 10))},
                                                         {"55", "IF " + condition + " THEN divisor = 0 ELSE divisor = 1 END EXERCISE 1 / divisor"});
                const Dal::Handle_<Dal::ModelData_> deterministic(new Dal::BSModelData_("", 100.0, 0.0, 0.0, 0.0));
                auto frozen = settings;
                frozen.lsmcPolicyRiskMode_ = "Frozen";
                ASSERT_NO_THROW(static_cast<void>(Script::MCSimulation<AAD::Number_>(failing, deterministic, 129, ReplayValuation(), frozen)));
                ASSERT_THROW(static_cast<void>(Script::MCSimulation<AAD::Number_>(failing, deterministic, 129, ReplayValuation(), settings)),
                             Dal::Exception_);
                AssertReplayEqual(Script::MCSimulation<AAD::Number_>(ReplayProduct(), ReplayModel(), 129, ReplayValuation(), settings), expected);
            }
        }
    }
}

TEST(ScriptExerciseLSMCTest, TestRetrainedTrainingGaussianCallerPreservesNestedRecording) {
    const ReplayThreads_ pool(1);
    auto settings = ReplaySettings(true, 33);
    settings.lsmcPolicyRiskMode_ = "RetrainedBump";
    auto model = Dal::CreateModel<double>(ReplayModel());
    const auto prepared = Script::PrepareScript(ReplayProduct(), model.get(), ReplayValuation(), settings);
    AAD::RecordingScope_ recording;
    AAD::Number_ input;
    recording.RegisterInput(input, 2.0);
    recording.StartRecording();
    AAD::Number_ output = input * input;
    ASSERT_THROW(static_cast<void>(Script::MCLsmcAadSimulation(prepared, ReplayModel(), 129)), Dal::Exception_);
    recording.FinishRecording();
    AAD::Adjoint(output) = 1.0;
    recording.Reverse();
    ASSERT_DOUBLE_EQ(AAD::Adjoint(input), 4.0);
    recording.Close();
    const auto recovered = Script::MCLsmcAadSimulation(prepared, ReplayModel(), 129);
    AssertReplayEqual(Script::MCLsmcAadSimulation(prepared, ReplayModel(), 129), recovered);
}

#if defined(DAL_ENABLE_AAD_PROFILING)
TEST(ScriptExerciseLSMCTest, TestRetrainedTrainingGaussianProfileAccountsCapacity) {
    const ReplayThreads_ pool(1);
    auto settings = ReplaySettings(true, 33);
    auto model = Dal::CreateModel<double>(ReplayModel());
    static_cast<void>(Script::PrepareScript(ReplayProduct(), model.get(), ReplayValuation(), settings));
    const size_t cacheBytes = (128 + 33) * model->SimDim() * sizeof(double);
    AAD::ProfilingData_ frozen, retrained;
    {
        AAD::ProfilingScope_ profile(&frozen);
        static_cast<void>(Script::MCSimulation<AAD::Number_>(ReplayProduct(), ReplayModel(), 129, ReplayValuation(), settings));
    }
    settings.lsmcPolicyRiskMode_ = "RetrainedBump";
    {
        AAD::ProfilingScope_ profile(&retrained);
        static_cast<void>(Script::MCSimulation<AAD::Number_>(ReplayProduct(), ReplayModel(), 129, ReplayValuation(), settings));
    }
    ASSERT_TRUE(frozen.complete_);
    ASSERT_TRUE(retrained.complete_);
    ASSERT_TRUE(retrained.memory_.regressionArrays_.valid_);
    ASSERT_EQ(retrained.memory_.regressionArrays_.capacityBytes_, frozen.memory_.regressionArrays_.capacityBytes_ + cacheBytes);
}
#endif

TEST(ScriptExerciseLSMCTest, TestLsmcLegacyPlanOneSlotPreservesOrderCancelsAndRecovers) {
    for (size_t threads : {1, 3, 8}) {
        const ReplayThreads_ pool(threads);
        SCOPED_TRACE(threads);
        std::atomic<size_t> live{0};
        ASSERT_THROW(static_cast<void>(RunOneSlotLegacyReplay(true, &live)), Dal::Exception_);
        ASSERT_EQ(live.load(), 0u);
        for (size_t repeat = 0; repeat < 3; ++repeat) {
            const auto result = RunOneSlotLegacyReplay(false, &live);
            ASSERT_EQ(result.sum_, 3.0);
            ASSERT_EQ(result.paths_, 3 * 8192 + 1);
            ASSERT_EQ(result.batches_, 4u);
            ASSERT_EQ(result.initialized_, std::min(pool.pool_->NumThreads(), size_t{4}));
            ASSERT_EQ(live.load(), 0u);
        }
    }
}

TEST(ScriptExerciseLSMCTest, TestLsmcReplayLateSubmissionFailureDrainsAndRecovers) {
    for (size_t threads : {1, 3}) {
        const ReplayThreads_ pool(threads);
        for (bool compiled : {false, true}) {
            const size_t firstReplay = HardReplayControlSubmissions(compiled);
            ASSERT_GT(firstReplay, 0u);
            const auto settings = ReplaySettings(compiled);
            const auto expected = Script::MCSimulation<AAD::Number_>(ReplayProduct(), ReplayModel(), 3 * 8192 + 1, ReplayValuation(), settings);
            struct FailReplay_ final : Script::Detail::SimulationObserver_ {
                size_t calls_ = 0;
                size_t failAt_;
                explicit FailReplay_(size_t failAt) : failAt_(failAt) {}
                void AfterSubmission() override {
                    if (++calls_ == failAt_)
                        THROW("LSMC replay submission failure");
                }
            } observer(firstReplay);
            {
                const Script::Detail::ScopedSimulationObserver_ scope(&observer);
                ASSERT_THROW(
                    static_cast<void>(Script::MCSimulation<AAD::Number_>(ReplayProduct(), ReplayModel(), 3 * 8192 + 1, ReplayValuation(), settings)),
                    Dal::Exception_);
            }
            ASSERT_EQ(observer.calls_, firstReplay);
            const auto recovered = Script::MCSimulation<AAD::Number_>(ReplayProduct(), ReplayModel(), 3 * 8192 + 1, ReplayValuation(), settings);
            AssertReplayEqual(recovered, expected);
        }
    }
}

#if defined(DAL_ENABLE_AAD_PROFILING)
TEST(ScriptExerciseLSMCTest, TestLsmcReplayBoundsNumericSlotsAcrossLegacyWindow) {
    const ReplayThreads_ pool(3);
    constexpr size_t PATHS = 256 * 8192 + 1;
    AAD::ProfilingData_ data;
    Script::SimResults_ actual(Vector_<Dal::String_>{});
    {
        AAD::ProfilingScope_ profile(&data);
        actual = Script::MCSimulation<AAD::Number_>(ReplayProduct(), ReplayModel(), PATHS, ReplayValuation(), ReplaySettings(true));
    }
    ASSERT_TRUE(data.complete_);
    const size_t risks = actual.risks_.size();
    const size_t maximum = 2 * risks * sizeof(double) + 256 * (sizeof(Script::Detail::AADBatchValues_) + risks * sizeof(double));
    ASSERT_LE(data.memory_.resultArrays_.liveBytes_, maximum);
    RecordProperty("replay_result_live_bytes", static_cast<int>(data.memory_.resultArrays_.liveBytes_));
    RecordProperty("replay_result_limit_bytes", static_cast<int>(maximum));
    pool.pool_->Start(1, true);
    const auto serial = Script::MCSimulation<AAD::Number_>(ReplayProduct(), ReplayModel(), PATHS, ReplayValuation(), ReplaySettings(true));
    AssertReplayEqual(actual, serial);
}
#endif

TEST(ScriptExerciseLSMCTest, TestLsmcConcurrentPoolRequestsSharePersistentReplayHelpers) {
    const ReplayThreads_ pool(4);
    auto settings = ReplaySettings(true, 128);
    settings.lsmcRqmcReplicates_ = 2;
    settings.lsmcTrainingSeed_ = 13;
    settings.lsmcPricingSeed_ = 17;
    constexpr size_t PATHS = 2 * 8192 + 1;
    const auto expected = Script::MCSimulation<AAD::Number_>(ReplayProduct(), ReplayModel(), PATHS, ReplayValuation(), settings);
    for (size_t repeat = 0; repeat < 2; ++repeat) {
        Vector_<Script::SimResults_> results(3, Script::SimResults_(Vector_<Dal::String_>{}));
        Script::SimulationTaskGroup_ tasks(pool.pool_, results.size());
        for (size_t request = 0; request < results.size(); ++request)
            tasks.Spawn([&, request] {
                results[request] = Script::MCSimulation<AAD::Number_>(ReplayProduct(), ReplayModel(), PATHS, ReplayValuation(), settings);
                return true;
            });
        tasks.Complete();
        for (const auto& result : results)
            AssertReplayEqual(result, expected);
    }
}

TEST(ScriptExerciseLSMCTest, TestLsmcCurvaturePersistentWorkersPreserveCapacityAndProducts) {
    const auto prepared = ReplayPreparation(true);
    const Vector_<> point{100.0, 0.2, 0.05, 0.0, 55.0};
    auto request = ReplaySpotDirection();
    constexpr size_t PATHS = 3 * 8192 + 1;
    const ReplayThreads_ serialPool(1);
    const auto expected = Script::EvaluateBlackScholesLsmcCurvature(prepared, point, PATHS, request);
    const size_t capacity = expected.Execution().maxBatchTapeBytes_ + expected.Execution().maxBatchCleanupReserveBytes_;
    ASSERT_GT(capacity, 0u);
    request.recordingCapacityBudgetBytes_ = capacity;
    for (size_t threads : {3, 8}) {
        serialPool.pool_->Start(threads, true);
        SCOPED_TRACE(threads);
        const auto actual = Script::EvaluateBlackScholesLsmcCurvature(prepared, point, PATHS, request);
        ASSERT_EQ(actual.Value(), expected.Value());
        ASSERT_EQ(actual.Gradient(), expected.Gradient());
        ASSERT_EQ(actual.Point(), expected.Point());
        ASSERT_EQ(actual.Execution().gradientEvaluations_, 3u);
        ASSERT_LE(actual.Execution().maxBatchTapeBytes_ + actual.Execution().maxBatchCleanupReserveBytes_, capacity);
        for (size_t input = 0; input < point.size(); ++input)
            ASSERT_EQ(actual.HessianProducts()(0, input), expected.HessianProducts()(0, input));
        auto invalid = request;
        invalid.recordingCapacityBudgetBytes_ = 0;
        ASSERT_THROW(static_cast<void>(Script::EvaluateBlackScholesLsmcCurvature(prepared, point, PATHS, invalid)), Dal::Exception_);
        const auto recovered = Script::EvaluateBlackScholesLsmcCurvature(prepared, point, PATHS, request);
        ASSERT_EQ(recovered.Value(), expected.Value());
        ASSERT_EQ(recovered.Gradient(), expected.Gradient());
    }
}

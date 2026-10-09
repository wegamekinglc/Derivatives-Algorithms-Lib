//
// Created by wegamekinglc on 2026/7/11.
//

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <functional>
#include <future>
#include <limits>
#include <memory>
#include <stdexcept>
#include <thread>
#include <tuple>

#include <dal/curve/tapeguard.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/simulation.hpp>
#include <dal/storage/globals.hpp>
#include <dal/utilities/exceptions.hpp>
#include <script_test_observers.hpp>

using namespace Dal;

namespace {
    struct CapturedLifetime_ {
        std::atomic<bool>* destroyed_;
        std::atomic<bool>* taskFinished_;
        std::atomic<bool>* taskFinishedBeforeDestruction_;

        ~CapturedLifetime_() {
            taskFinishedBeforeDestruction_->store(taskFinished_->load());
            destroyed_->store(true);
        }
    };

    struct ReleaseOnDestruction_ {
        std::promise<void>* release_;
        bool released_ = false;

        void Release() {
            if (!released_) {
                release_->set_value();
                released_ = true;
            }
        }

        ~ReleaseOnDestruction_() { Release(); }
    };

    struct PoolRestore_ {
        ThreadPool_* pool_ = ThreadPool_::GetInstance();
        size_t threads_ = pool_->NumThreads();
        ~PoolRestore_() {
            pool_->Start(threads_, true);
            pool_->Stop();
        }
    };

    struct PositioningAudit_ {
        std::atomic<size_t> live_{0}, peak_{0}, clones_{0}, runs_{0};
        size_t replayed_ = 0;
        size_t failSeek_ = std::numeric_limits<size_t>::max();
        size_t failClone_ = std::numeric_limits<size_t>::max();
        std::function<void(size_t)> beforeSeek_;
    };

    class AuditedIRN_ : public Random_ {
        PositioningAudit_* audit_;
        std::unique_ptr<Random_> inner_;
        size_t path_ = 0;
        bool clone_ = false;

    public:
        explicit AuditedIRN_(PositioningAudit_* audit) : audit_(audit), inner_(Script::CreateRNG("irn", 3, false)) {}
        AuditedIRN_(PositioningAudit_* audit, std::unique_ptr<Random_> inner, size_t path)
            : audit_(audit), inner_(std::move(inner)), path_(path), clone_(true) {
            const size_t live = ++audit_->live_;
            size_t peak = audit_->peak_.load();
            while (peak < live && !audit_->peak_.compare_exchange_weak(peak, live)) {
            }
        }
        ~AuditedIRN_() override {
            if (clone_)
                --audit_->live_;
        }
        void FillUniform(Vector_<>* values) override { inner_->FillUniform(values); }
        void FillNormal(Vector_<>* values) override {
            inner_->FillNormal(values);
            ++path_;
        }
        void SkipTo(size_t path) override { SkipNormalTo(path); }
        void SkipNormalTo(size_t path) override {
            if (audit_->beforeSeek_)
                audit_->beforeSeek_(path);
            REQUIRE(path != audit_->failSeek_, "injected seek failure");
            REQUIRE(path >= path_, "positioner sought backwards");
            audit_->replayed_ += (path - path_) * NDim();
            inner_->SkipNormalTo(path);
            path_ = path;
        }
        std::unique_ptr<Random_> Clone() const override {
            REQUIRE(audit_->clones_.load() != audit_->failClone_, "injected clone failure");
            ++audit_->clones_;
            return std::make_unique<AuditedIRN_>(audit_, inner_->Clone(), path_);
        }
        size_t NDim() const override { return inner_->NDim(); }
    };

    void ConfigurePositioningFailure(const String_& source, PositioningAudit_* audit) {
        if (source == "seek")
            audit->failSeek_ = Script::BATCH_SIZE;
        if (source == "clone")
            audit->failClone_ = 1;
    }

    template <class Real_> void AssertIntMaxBatchBoundary() {
        const size_t nPaths = static_cast<size_t>(std::numeric_limits<int>::max());
        const Script::BatchPlan_ plan(nPaths, 1);

        ASSERT_EQ(plan.BatchSize(), Script::BATCH_SIZE);
        ASSERT_EQ(plan.BatchCount(), 262144u);

        const auto first = plan.BatchAt(0);
        const auto penultimate = plan.BatchAt(plan.BatchCount() - 2);
        const auto last = plan.BatchAt(plan.BatchCount() - 1);
        ASSERT_EQ(first.firstPath_, 0u);
        ASSERT_EQ(first.pathCount_, Script::BATCH_SIZE);
        ASSERT_EQ(penultimate.pathCount_, Script::BATCH_SIZE);
        ASSERT_EQ(last.firstPath_, 262143u * Script::BATCH_SIZE);
        ASSERT_EQ(last.pathCount_, 8191u);
        ASSERT_EQ((plan.BatchCount() - 1) * plan.BatchSize() + last.pathCount_, nPaths);
    }
} // namespace

TEST(ScriptTest, TestBatchPlanHandlesIntMaxForDoubleAndAad) {
    AssertIntMaxBatchBoundary<double>();
    AssertIntMaxBatchBoundary<AAD::Number_>();
}

TEST(ScriptTest, TestBatchPlanPreservesSmallRunParallelism) {
    const Script::BatchPlan_ plan(10, 4);

    ASSERT_EQ(plan.BatchSize(), 3u);
    ASSERT_EQ(plan.BatchCount(), 4u);
    ASSERT_EQ(plan.BatchAt(0).firstPath_, 0u);
    ASSERT_EQ(plan.BatchAt(1).firstPath_, 3u);
    ASSERT_EQ(plan.BatchAt(2).pathCount_, 3u);
    ASSERT_EQ(plan.BatchAt(3).firstPath_, 9u);
    ASSERT_EQ(plan.BatchAt(3).pathCount_, 1u);
}

TEST(ScriptTest, TestBatchPlanHasNoEmptyExactMultipleTail) {
    const Script::BatchPlan_ plan(2 * Script::BATCH_SIZE, 1);

    ASSERT_EQ(plan.BatchCount(), 2u);
    ASSERT_EQ(plan.BatchAt(0).pathCount_, Script::BATCH_SIZE);
    ASSERT_EQ(plan.BatchAt(1).firstPath_, Script::BATCH_SIZE);
    ASSERT_EQ(plan.BatchAt(1).pathCount_, Script::BATCH_SIZE);
}

TEST(ScriptTest, TestBatchPlanHandlesZeroPathsAndRejectsZeroThreads) {
    const Script::BatchPlan_ empty(0, 4);

    ASSERT_EQ(empty.BatchSize(), 0u);
    ASSERT_EQ(empty.BatchCount(), 0u);
    ASSERT_THROW((void)empty.BatchAt(0), Dal::Exception_);
    ASSERT_THROW((void)Script::BatchPlan_(1, 0), Dal::Exception_);
}

TEST(ScriptTest, TestPositionedBatchesPreserveSequentialStreamWithBoundedLinearReplay) {
    PoolRestore_ restore;
    for (const size_t threads : {1, 4}) {
        restore.pool_->Start(threads, true);
        const Script::BatchPlan_ plan(33 * Script::BATCH_SIZE + 17, restore.pool_->NumThreads());
        PositioningAudit_ audit;
        Vector_<> actual(plan.BatchCount()), expected(plan.BatchCount());
        auto sequential = Script::CreateRNG("irn", 3, false);
        Vector_<> values(3);
        for (size_t i = 0; i < plan.BatchCount(); ++i)
            for (size_t path = 0; path < plan.BatchAt(i).pathCount_; ++path) {
                sequential->FillNormal(&values);
                expected[i] += values[0] + values[1] + values[2];
            }
        Script::Detail::RunSimulationBatches(restore.pool_, plan, std::make_unique<AuditedIRN_>(&audit),
                                             [&](size_t i, const Script::PathBatch_& batch, std::unique_ptr<Random_> random) {
                                                 Vector_<> normal(3);
                                                 for (size_t path = 0; path < batch.pathCount_; ++path) {
                                                     random->FillNormal(&normal);
                                                     actual[i] += normal[0] + normal[1] + normal[2];
                                                 }
                                                 ++audit.runs_;
                                             });
        ASSERT_EQ(actual, expected);
        ASSERT_EQ(audit.runs_.load(), plan.BatchCount());
        ASSERT_EQ(audit.clones_.load(), plan.BatchCount());
        ASSERT_EQ(audit.replayed_, 3 * plan.BatchAt(plan.BatchCount() - 1).firstPath_);
        ASSERT_LE(audit.peak_.load(), 2 * restore.pool_->NumThreads());
        ASSERT_EQ(audit.live_.load(), 0u);
    }
}

TEST(ScriptTest, TestPositionedBatchesOverlapPositioningAndConsumption) {
    PoolRestore_ restore;
    restore.pool_->Start(2, true);
    if (restore.pool_->NumThreads() < 2)
        GTEST_SKIP() << "requires a pool worker";
    PositioningAudit_ audit;
    std::promise<void> started;
    auto running = started.get_future();
    audit.beforeSeek_ = [&](size_t path) {
        if (path == Script::BATCH_SIZE)
            REQUIRE(running.wait_for(std::chrono::seconds(5)) == std::future_status::ready, "positioning did not overlap consumption");
    };
    Script::Detail::RunSimulationBatches(restore.pool_, Script::BatchPlan_(8 * Script::BATCH_SIZE, 2), std::make_unique<AuditedIRN_>(&audit),
                                         [&](size_t i, const Script::PathBatch_&, std::unique_ptr<Random_>) {
                                             if (i == 0)
                                                 started.set_value();
                                             ++audit.runs_;
                                         });
    ASSERT_EQ(audit.runs_.load(), 8u);
    ASSERT_EQ(audit.live_.load(), 0u);
}

TEST(ScriptTest, TestPositionedBatchesCancelDrainAndRecoverAfterEveryFailureSource) {
    PoolRestore_ restore;
    for (const size_t threads : {1, 4}) {
        restore.pool_->Start(threads, true);
        for (const String_ source : {"seek", "clone", "submission", "worker"}) {
            SCOPED_TRACE(::testing::Message() << "threads=" << threads << " source=" << source);
            PositioningAudit_ audit;
            ConfigurePositioningFailure(source, &audit);
            bool caught = false;
            try {
                Script::TestSupport::RejectSubmissions_ observer("injected submission failure");
                const Script::Detail::ScopedSimulationObserver_ scope(source == "submission" ? &observer : nullptr);
                Script::Detail::RunSimulationBatches(restore.pool_, Script::BatchPlan_(33 * Script::BATCH_SIZE, restore.pool_->NumThreads()),
                                                     std::make_unique<AuditedIRN_>(&audit),
                                                     [&](size_t i, const Script::PathBatch_&, std::unique_ptr<Random_>) {
                                                         ++audit.runs_;
                                                         REQUIRE(source != "worker" || i != 0, "injected worker failure");
                                                     });
            } catch (const Exception_& error) {
                caught = String_(error.what()).find("injected " + source + " failure") != String_::npos;
            }
            ASSERT_TRUE(caught);
            ASSERT_EQ(audit.live_.load(), 0u);
            if (threads == 1) {
                ASSERT_LE(audit.clones_.load(), 2u);
                ASSERT_EQ(audit.runs_.load(), source == "worker" ? 1u : 0u);
            }
            std::atomic<size_t> recovered{0};
            Script::Detail::RunSimulationBatches(
                restore.pool_, Script::BatchPlan_(17, restore.pool_->NumThreads()), Script::CreateRNG("irn", 3, false),
                [&](size_t, const Script::PathBatch_& batch, std::unique_ptr<Random_>) { recovered += batch.pathCount_; });
            ASSERT_EQ(recovered.load(), 17u);
        }
    }
}

TEST(ScriptTest, TestPositionedBatchesDoNotSeekOrCloneForZeroPaths) {
    PositioningAudit_ audit;
    audit.failSeek_ = 0;
    audit.failClone_ = 0;
    Script::Detail::RunSimulationBatches(ThreadPool_::GetInstance(), Script::BatchPlan_(0, 1), std::make_unique<AuditedIRN_>(&audit),
                                         [&](size_t, const Script::PathBatch_&, std::unique_ptr<Random_>) { ++audit.runs_; });
    ASSERT_EQ(audit.clones_.load(), 0u);
    ASSERT_EQ(audit.runs_.load(), 0u);
}

TEST(ScriptTest, TestIRNPreparedParallelPriceAndAllRisksPreservePrecisionAndBridge) {
    PoolRestore_ restore;
    const auto date = XGLOBAL::SetEvaluationDateInScope(Date_(2022, 6, 22));
    const Handle_<ModelData_> modelData(new BSModelData_("", 10.0, 0.2, 0.034, 0.021));
    const Script::ScriptProductData_ data("", {Cell_(Date_(2023, 6, 21)), Cell_(Date_(2024, 6, 21))}, {"x = SPOT()", "payoff PAYS SPOT() + x"});
    constexpr size_t PATHS = 4 * Script::BATCH_SIZE + 17;
    for (const String_ precision : {"Fast", "Precise"})
        for (const bool bridge : {false, true})
            for (const bool compiled : {false, true}) {
                SCOPED_TRACE(::testing::Message() << precision << " bridge=" << bridge << " compiled=" << compiled);
                Script::MonteCarloSettings_ settings;
                settings.rsg_ = "irn";
                settings.normalPrecision_ = precision;
                settings.useBb_ = bridge;
                settings.compiled_ = compiled;
                const auto value = [&](size_t threads, bool aad) {
                    restore.pool_->Start(threads, true);
                    settings.enableAad_ = aad;
                    auto model = CreateModel<double>(modelData);
                    const auto prepared = Script::PrepareScript(data, model.get(), {}, settings);
                    return aad ? Script::MCSimulation<AAD::Number_>(prepared, modelData, PATHS, "irn", bridge, compiled, prepared.MaxNestedIfs(),
                                                                    settings.smooth_)
                               : Script::MCSimulation<double>(prepared, modelData, PATHS, "irn", bridge, compiled);
                };
                const auto serial = value(1, true);
                const auto parallel = value(4, true);
                const auto plain = value(4, false);
                ASSERT_NEAR(serial.aggregated_ / PATHS, parallel.aggregated_ / PATHS, 1e-10);
                ASSERT_NEAR(serial.aggregated_ / PATHS, plain.aggregated_ / PATHS, 1e-10);
                ASSERT_EQ(serial.names_, parallel.names_);
                for (size_t i = 0; i < serial.risks_.size(); ++i)
                    ASSERT_NEAR(serial.risks_[i], parallel.risks_[i], 1e-10);
            }
}

TEST(ScriptTest, TestSimulationTaskGroupDrainsOnSubmissionFailureAndPoolRecovers) {
    ThreadPool_* threadPool = ThreadPool_::GetInstance();
    const size_t originalThreadCount = threadPool->NumThreads();
    threadPool->Start(2, true);
    if (threadPool->NumThreads() < 2) {
        threadPool->Stop();
        GTEST_SKIP() << "requires one pool worker in addition to the caller";
    }

    std::promise<void> taskStarted;
    std::promise<void> releaseTask;
    const std::shared_future<void> release = releaseTask.get_future().share();
    std::atomic<bool> capturedStateDestroyed{false};
    std::atomic<bool> taskFinished{false};
    std::atomic<bool> taskFinishedBeforeCapturedStateDestruction{false};
    std::atomic<bool> taskObservedDestroyedState{false};
    std::future<void> stopper;
    bool submissionRejected = false;

    try {
        CapturedLifetime_ capturedState{&capturedStateDestroyed, &taskFinished, &taskFinishedBeforeCapturedStateDestruction};
        Script::SimulationTaskGroup_ tasks(threadPool, 2);
        ReleaseOnDestruction_ releaseOnDestruction{&releaseTask};
        tasks.Spawn([&, captured = &capturedState]() {
            taskStarted.set_value();
            release.wait();
            static_cast<void>(captured);
            taskObservedDestroyedState.store(capturedStateDestroyed.load());
            taskFinished.store(true);
            return true;
        });
        taskStarted.get_future().wait();

        stopper = std::async(std::launch::async, [threadPool]() { threadPool->Stop(); });
        while (threadPool->IsActive())
            std::this_thread::yield();

        tasks.Spawn([]() { return true; });
        releaseOnDestruction.Release();
        tasks.Complete();
    } catch (const Dal::Exception_&) {
        submissionRejected = true;
    }

    stopper.get();
    threadPool->Start(2, true);
    std::atomic<int> sentinelRuns{0};
    {
        Script::SimulationTaskGroup_ tasks(threadPool, 1);
        tasks.Spawn([&]() {
            ++sentinelRuns;
            return true;
        });
        tasks.Complete();
    }
    threadPool->Stop();

    threadPool->Start(originalThreadCount, true);
    threadPool->Stop();

    ASSERT_TRUE(submissionRejected);
    ASSERT_TRUE(taskFinished.load());
    ASSERT_TRUE(taskFinishedBeforeCapturedStateDestruction.load());
    ASSERT_FALSE(taskObservedDestroyedState.load());
    ASSERT_TRUE(capturedStateDestroyed.load());
    ASSERT_EQ(sentinelRuns.load(), 1);
}

TEST(ScriptTest, TestSimulationTaskGroupDrainsAllTasksBeforeRethrowingFirstFailure) {
    ThreadPool_* threadPool = ThreadPool_::GetInstance();
    const size_t originalThreadCount = threadPool->NumThreads();
    threadPool->Start(1, true);
    std::atomic<int> firstRuns{0};
    std::atomic<int> secondRuns{0};
    bool caughtFirstFailure = false;

    try {
        Script::SimulationTaskGroup_ tasks(threadPool, 2);
        tasks.Spawn([&]() -> bool {
            ++firstRuns;
            throw std::runtime_error("first task failure");
        });
        tasks.Spawn([&]() -> bool {
            ++secondRuns;
            throw std::runtime_error("second task failure");
        });
        tasks.Complete();
    } catch (const std::runtime_error& error) {
        caughtFirstFailure = String_(error.what()).find("first task failure") != String_::npos;
    }

    threadPool->Stop();
    threadPool->Start(originalThreadCount, true);
    threadPool->Stop();

    ASSERT_TRUE(caughtFirstFailure);
    ASSERT_EQ(firstRuns.load(), 1);
    ASSERT_EQ(secondRuns.load(), 1);
}

TEST(ScriptTest, TestAadSimulationPropagatesTaskFailureAndRemainsUsable) {
    const auto evaluationDate = XGLOBAL::SetEvaluationDateInScope(Date_(2022, 6, 22));
    const Date_ exerciseDate(2024, 6, 21);
    Vector_<Cell_> eventDates{Cell_(String_("STRIKE")), Cell_(exerciseDate)};
    Vector_<String_> events{"11.0", "call pays MAX(spot() - STRIKE, 0.0)"};
    Script::ScriptProduct_ product(eventDates, events);
    const int maxNestedIfs = static_cast<int>(product.PreProcess(true, false));
    const Handle_<ModelData_> model(new BSModelData_("bsmodel", 10.0, 0.20, 0.034, 0.021));

    ASSERT_THROW(Script::MCSimulation<AAD::Number_>(product, model, 64, "invalid", false, true, maxNestedIfs), Dal::Exception_);

    const Script::SimResults_ recovered = Script::MCSimulation<AAD::Number_>(product, model, 64, "sobol", false, true, maxNestedIfs);
    ASSERT_TRUE(std::isfinite(recovered.aggregated_));
    for (const double risk : recovered.risks_)
        ASSERT_TRUE(std::isfinite(risk));
}

TEST(ScriptTest, TestAadBatchRejectsNestedIndependentRecordingBeforeDiscardingOuterGraph) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(Date_(2022, 6, 22));
    Script::ScriptProduct_ product({Cell_(Date_(2024, 6, 21))}, {"payoff PAYS SPOT()"});
    const int depth = static_cast<int>(product.PreProcess(true, false));
    const Handle_<ModelData_> model(new BSModelData_("bsmodel", 10.0, 0.20, 0.034, 0.021));
    const auto metadata = CreateModel<double>(model);
    const String_ method("sobol");
    const Script::Detail::AADBatchSettings_ settings{
        method, false, depth, 0.01, 4, metadata->Parameters().size(), product.ConstVarNames().size(), product.PayOffIdx()};
    Script::SimResults_ result(Vector::Join(metadata->ParameterLabels(), product.ConstVarNames()));
    AAD::RecordingScope_ outer;
    AAD::Number_ input;
    outer.RegisterInput(input, 3.0);
    outer.StartRecording();
    AAD::Number_ output = input * input;
    outer.FinishRecording();
    ASSERT_THROW(Script::Detail::EvaluateAADBatch(product, model, settings, std::nullopt, Script::PathBatch_{0, 4}, &result), Exception_);
    outer.ClearAdjoints();
    AAD::Adjoint(output) = 1.0;
    outer.Reverse();
    ASSERT_DOUBLE_EQ(AAD::AdjointValue(input), 6.0);
    outer.Close();
}

TEST(ScriptTest, TestPositionedIRNAadFailurePreservesOuterRecordingAndRecovers) {
    PoolRestore_ restore;
    restore.pool_->Start(1, true);
    const auto date = XGLOBAL::SetEvaluationDateInScope(Date_(2022, 6, 22));
    Script::ScriptProduct_ product({Cell_(Date_(2024, 6, 21))}, {"payoff PAYS SPOT()"});
    const int depth = product.PreProcess(true, false);
    const Handle_<ModelData_> model(new BSModelData_("", 10.0, 0.2, 0.034, 0.021));
    {
        AAD::RecordingScope_ outer;
        AAD::Number_ input;
        outer.RegisterInput(input, 3.0);
        outer.StartRecording();
        AAD::Number_ output = input * input;
        outer.FinishRecording();
        ASSERT_THROW(Script::MCSimulation<AAD::Number_>(product, model, 4 * Script::BATCH_SIZE, "irn", false, true, depth), Exception_);
        outer.ClearAdjoints();
        AAD::Adjoint(output) = 1.0;
        outer.Reverse();
        ASSERT_DOUBLE_EQ(AAD::AdjointValue(input), 6.0);
        outer.Close();
    }
    const auto recovered = Script::MCSimulation<AAD::Number_>(product, model, 8193, "irn", false, true, depth);
    ASSERT_TRUE(std::isfinite(recovered.aggregated_));
    ASSERT_NEAR(recovered["spot"], recovered.aggregated_ / (8193 * 10.0), 1e-10);
}

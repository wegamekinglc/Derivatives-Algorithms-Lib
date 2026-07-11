//
// Created by wegamekinglc on 2026/7/11.
//

#include <gtest/gtest.h>

#include <atomic>
#include <cmath>
#include <future>
#include <limits>
#include <stdexcept>
#include <thread>
#include <tuple>

#include <dal/curve/tapeguard.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/simulation.hpp>
#include <dal/storage/globals.hpp>
#include <dal/utilities/exceptions.hpp>

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
    TapeGuard_ tapeGuard(AAD::Tape());
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

#if defined(DAL_USE_ADEPT_AAD)
TEST(ScriptTest, TestCompiledAadOperandStacksHaveEvaluationStateLifetime) {
    const auto evaluationDate = XGLOBAL::SetEvaluationDateInScope(Date_(2022, 6, 22));
    const Date_ exerciseDate(2024, 6, 21);
    Vector_<Cell_> eventDates{Cell_(String_("STRIKE")), Cell_(exerciseDate)};
    Vector_<String_> events{"11.0", "call pays MAX(spot() - STRIKE, 0.0)"};
    Script::ScriptProduct_ product(eventDates, events);
    product.PreProcess(true, false);
    const Script::ScriptCompiled_ compiled = product.Compile(true);

    auto evaluation = std::async(std::launch::async, [&]() {
        AAD::Tape_* tape = AAD::Tape();
        TapeGuard_ tapeGuard(tape);
        const size_t registeredBefore = tape->n_gradients_registered();
        double firstPayoff = 0.0;
        double secondPayoff = 0.0;

        for (int run = 0; run < 2; ++run) {
            AAD::NewRecording(*tape);
            AAD::Scenario_<AAD::Number_> scenario(product.EventDates().size());
            for (auto& sample : scenario) {
                sample.spot_ = 13.0;
                sample.numeraire_ = 1.0;
            }
            auto state = product.BuildEvalState<AAD::Number_>();
            compiled.Evaluate(scenario, state);
            const double payoff = AAD::Value(state.VarVals()[product.PayOffIdx()]);
            if (run == 0)
                firstPayoff = payoff;
            else
                secondPayoff = payoff;
        }

        return std::make_tuple(registeredBefore, tape->n_gradients_registered(), firstPayoff, secondPayoff);
    });

    const auto [registeredBefore, registeredAfter, firstPayoff, secondPayoff] = evaluation.get();
    ASSERT_EQ(registeredAfter, registeredBefore);
    ASSERT_TRUE(std::isfinite(firstPayoff));
    ASSERT_NEAR(secondPayoff, firstPayoff, 1e-12);
}
#endif

//
// Created by Codex on 2026/10/04.
//

#include <gtest/gtest.h>

#include <limits>
#include <thread>
#include <type_traits>

#include <dal/math/aad/profiling.hpp>
#include <dal/math/aad/recording.hpp>
#include <dal/script/detail/profiling.hpp>

using namespace Dal;
using namespace Dal::AAD;

TEST(AADProfilingTest, TestCompiledAvailabilityMatchesConfiguration) {
#if defined(DAL_ENABLE_AAD_PROFILING)
    ASSERT_TRUE(ProfilingAvailable());
#else
    ASSERT_FALSE(ProfilingAvailable());
    static_assert(std::is_nothrow_constructible_v<ProfilingSpan_, AADProfilingPhase_::Value_>);
    RecordingScope_ recording;
    Number_ x;
    recording.RegisterInput(x, 2.0);
    recording.StartRecording();
    Number_ y = x * x;
    recording.FinishRecording();
    ProfilingData_ data;
    ASSERT_THROW(ProfilingScope_ profile(&data), Exception_);
    Adjoint(y) = 1.0;
    recording.Reverse();
    ASSERT_DOUBLE_EQ(Value(y), 4.0);
    ASSERT_DOUBLE_EQ(AdjointValue(x), 4.0);
    recording.Close();
#endif
}

#if defined(DAL_ENABLE_AAD_PROFILING)
TEST(AADProfilingTest, TestArrayMeasurementRunsOnlyInsideExplicitScope) {
    size_t calls = 0;
    const auto measure = [&] {
        ++calls;
        ProfilingMemoryStatistics_ sample;
        sample.workspaceArrays_ = {16, 32, true};
        return sample;
    };
    CaptureMemoryForProfiling(measure);
    ASSERT_EQ(calls, 0);
    ProfilingData_ data;
    {
        ProfilingScope_ profile(&data);
        CaptureMemoryForProfiling(measure);
    }
    CaptureMemoryForProfiling(measure);
    ASSERT_EQ(calls, 1);
    ASSERT_EQ(data.memorySamples_, 1);
    ASSERT_EQ(data.memory_.workspaceArrays_.liveBytes_, 16);
    ASSERT_TRUE(data.complete_);
}

TEST(AADProfilingTest, TestKnownArrayPayloadSeparatesLiveValuesAndRetainedCapacity) {
    Vector_<> values{1.0, 2.0};
    values.reserve(8);
    Scenario_<double> path(1);
    path[0].observations_ = {1.0, 2.0, 3.0};
    ProfilingData_ data;
    {
        ProfilingScope_ profile(&data);
        ProfilingMemoryStatistics_ sample;
        sample.pathArrays_ = Script::Detail::ProfilePathArrays(path);
        sample.workspaceArrays_ = Script::Detail::ProfileArrays(values);
        CaptureMemoryForProfiling(sample);
        values.Resize(1);
        sample.workspaceArrays_ = Script::Detail::ProfileArrays(values);
        CaptureMemoryForProfiling(sample);
    }
    ASSERT_TRUE(data.complete_);
    ASSERT_EQ(data.memorySamples_, 2);
    ASSERT_EQ(data.memory_.workspaceArrays_.liveBytes_, 2 * sizeof(double));
    ASSERT_EQ(data.memory_.workspaceArrays_.capacityBytes_, values.capacity() * sizeof(double));
    ASSERT_EQ(data.memory_.pathArrays_.liveBytes_, sizeof(Sample_<double>) + 3 * sizeof(double));
    ASSERT_GE(data.memory_.pathArrays_.capacityBytes_, data.memory_.pathArrays_.liveBytes_);
    ASSERT_EQ(data.memory_.resultArrays_.liveBytes_, 0);
    ASSERT_EQ(data.memory_.regressionArrays_.liveBytes_, 0);
}

TEST(AADProfilingTest, TestInvalidArrayPayloadDoesNotWrapOrReportComplete) {
    ProfilingData_ data;
    {
        ProfilingScope_ profile(&data);
        ProfilingMemoryStatistics_ sample;
        const auto maximum = std::numeric_limits<std::uint64_t>::max();
        sample.workspaceArrays_ = Script::Detail::JoinProfileArrays({maximum, maximum, true}, {1, 1, true});
        ASSERT_FALSE(sample.workspaceArrays_.valid_);
        CaptureMemoryForProfiling(sample);
    }
    ASSERT_FALSE(data.complete_);
    ASSERT_TRUE(data.invalidMeasurement_);
    ASSERT_FALSE(data.memory_.workspaceArrays_.valid_);
    ASSERT_EQ(data.memory_.workspaceArrays_.liveBytes_, 0);
}

TEST(AADProfilingTest, TestSameThreadTaskCpuIsCountedOnceAcrossNestedTaskGroups) {
    ProfilingData_ data, independent;
    {
        ProfilingScope_ profile(&data);
        ProfilingTaskSet_ tasks(1);
        {
            ProfilingScope_ separate(&independent);
        }
        {
            ProfilingTaskScope_ task(tasks, 0);
            ProfilingTaskSet_ children(1);
            ProfilingTaskScope_ child(children, 0);
            ProfilingSpan_ span(AADProfilingPhase_::Value_::PAYOFF);
        }
    }
    const auto& task = data.taskGroups_.front()[0];
    const auto& child = task.taskGroups_.front()[0];
    ASSERT_TRUE(data.complete_);
    ASSERT_TRUE(independent.complete_);
    ASSERT_EQ(data.selfCpuAvailable_, data.cpuAvailable_ && task.cpuAvailable_ && child.cpuAvailable_);
    if (data.selfCpuAvailable_) {
        ASSERT_EQ(data.cpuNanoseconds_, data.selfCpuNanoseconds_ + task.cpuNanoseconds_);
        ASSERT_EQ(task.cpuNanoseconds_, task.selfCpuNanoseconds_ + child.cpuNanoseconds_);
        ASSERT_EQ(child.cpuNanoseconds_, child.selfCpuNanoseconds_);
        ASSERT_EQ(data.cpuNanoseconds_, data.selfCpuNanoseconds_ + task.selfCpuNanoseconds_ + child.selfCpuNanoseconds_);
    }
}

TEST(AADProfilingTest, TestSeparateThreadTaskCpuDoesNotSubtractFromWaitingThread) {
    ProfilingData_ data;
    {
        ProfilingScope_ profile(&data);
        ProfilingTaskSet_ tasks(1);
        std::thread thread([&] {
            ProfilingTaskScope_ task(tasks, 0);
            ProfilingSpan_ span(AADProfilingPhase_::Value_::PAYOFF);
        });
        thread.join();
    }
    const auto& task = data.taskGroups_.front()[0];
    ASSERT_TRUE(data.complete_);
    ASSERT_NE(data.thread_, task.thread_);
    ASSERT_EQ(data.selfCpuAvailable_, data.cpuAvailable_);
    ASSERT_EQ(task.selfCpuAvailable_, task.cpuAvailable_);
    ASSERT_EQ(data.cpuNanoseconds_, data.selfCpuNanoseconds_);
    ASSERT_EQ(task.cpuNanoseconds_, task.selfCpuNanoseconds_);
}

TEST(AADProfilingTest, TestObservedGraphKeepsIndependentGradientAndTapePeak) {
    Clear(*Tape());
    ProfilingData_ data;
    {
        ProfilingScope_ profile(&data);
        RecordingScope_ recording;
        Number_ x, y;
        recording.RegisterInput(x, 2.0);
        recording.RegisterInput(y, 3.0);
        recording.StartRecording();
        Number_ product, output;
        {
            ProfilingSpan_ span(AADProfilingPhase_::Value_::PAYOFF);
            product = x * y;
            output = product + x;
        }
        profile.CaptureTape(*Tape());
        recording.FinishRecording();
        {
            ProfilingSpan_ span(AADProfilingPhase_::Value_::REVERSE);
            Adjoint(output) = 1.0;
            recording.Reverse();
        }
        ASSERT_DOUBLE_EQ(Value(output), 8.0);
        ASSERT_DOUBLE_EQ(AdjointValue(x), 4.0);
        ASSERT_DOUBLE_EQ(AdjointValue(y), 2.0);
        recording.Close();
        profile.CaptureTape(*Tape());
    }
    ASSERT_EQ(data.Phase(AADProfilingPhase_::Value_::PAYOFF).calls_, 1);
    ASSERT_EQ(data.Phase(AADProfilingPhase_::Value_::REVERSE).calls_, 1);
    ASSERT_EQ(data.highWater_.nodes_, 4);
    ASSERT_EQ(data.highWater_.edges_, 4);
    ASSERT_GT(data.highWater_.capacityBytes_, 0);
    ASSERT_EQ(data.tapeSamples_, 2);
    ASSERT_TRUE(data.complete_);
}

TEST(AADProfilingTest, TestNestedWindowsExcludeChildWallTimeFromParentSelfTime) {
    ProfilingData_ data;
    {
        ProfilingScope_ profile(&data);
        ProfilingSpan_ parent(AADProfilingPhase_::Value_::PREPARE);
        {
            ProfilingSpan_ child(AADProfilingPhase_::Value_::WORKER_INIT);
            ProfilingSpan_ grandchild(AADProfilingPhase_::Value_::PAYOFF);
        }
    }
    const auto& prepare = data.Phase(AADProfilingPhase_::Value_::PREPARE);
    const auto& worker = data.Phase(AADProfilingPhase_::Value_::WORKER_INIT);
    const auto& payoff = data.Phase(AADProfilingPhase_::Value_::PAYOFF);
    ASSERT_EQ(prepare.calls_, 1);
    ASSERT_EQ(worker.calls_, 1);
    ASSERT_EQ(payoff.calls_, 1);
    ASSERT_EQ(prepare.wallNanoseconds_, prepare.selfWallNanoseconds_ + worker.wallNanoseconds_);
    ASSERT_EQ(worker.wallNanoseconds_, worker.selfWallNanoseconds_ + payoff.wallNanoseconds_);
    ASSERT_EQ(payoff.wallNanoseconds_, payoff.selfWallNanoseconds_);
    ASSERT_LE(prepare.cpuSamples_, prepare.calls_);
    ASSERT_TRUE(data.complete_);
}

TEST(AADProfilingTest, TestNestedCollectorRestoresOuterCollectorAfterRejection) {
    ProfilingData_ outer, inner;
    {
        ProfilingScope_ profile(&outer);
        ASSERT_THROW(ProfilingScope_ invalid(nullptr), Exception_);
        ASSERT_THROW(ProfilingScope_ duplicate(&outer), Exception_);
        ASSERT_THROW(ProfilingSpan_ invalid(AADProfilingPhase_{}), Exception_);
        {
            ProfilingScope_ nested(&inner);
            ProfilingSpan_ span(AADProfilingPhase_::Value_::PAYOFF);
            ASSERT_THROW(profile.CaptureTape(*Tape()), Exception_);
        }
        ProfilingSpan_ span(AADProfilingPhase_::Value_::REVERSE);
    }
    ASSERT_THROW(static_cast<void>(outer.Phase(AADProfilingPhase_{})), Exception_);
    ASSERT_EQ(outer.Phase(AADProfilingPhase_::Value_::PAYOFF).calls_, 0);
    ASSERT_EQ(outer.Phase(AADProfilingPhase_::Value_::REVERSE).calls_, 1);
    ASSERT_EQ(inner.Phase(AADProfilingPhase_::Value_::PAYOFF).calls_, 1);
    ASSERT_TRUE(outer.complete_);
    ASSERT_TRUE(inner.complete_);
}

TEST(AADProfilingTest, TestBusinessFailureMarksIncompleteAndRestoresNextRequest) {
    ProfilingData_ failed, next;
    const auto fail = [&] {
        ProfilingScope_ profile(&failed);
        ProfilingSpan_ span(AADProfilingPhase_::Value_::PAYOFF);
        THROW("business failure");
    };
    ASSERT_THROW(fail(), Exception_);
    ASSERT_FALSE(failed.complete_);
    ASSERT_EQ(failed.Phase(AADProfilingPhase_::Value_::PAYOFF).calls_, 1);
    {
        ProfilingScope_ profile(&next);
        ProfilingSpan_ span(AADProfilingPhase_::Value_::REVERSE);
    }
    ASSERT_TRUE(next.complete_);
    ASSERT_EQ(next.Phase(AADProfilingPhase_::Value_::REVERSE).calls_, 1);
}

TEST(AADProfilingTest, TestWorkersKeepSeparateCollectorsAndRejectForeignScopeCapture) {
    ProfilingData_ parent, worker;
    bool foreignCaptureRejected = false;
    {
        ProfilingScope_ profile(&parent);
        std::thread thread([&] {
            try {
                profile.CaptureTape(*Tape());
            } catch (const Exception_&) {
                foreignCaptureRejected = true;
            }
            ProfilingScope_ local(&worker);
            ProfilingSpan_ span(AADProfilingPhase_::Value_::PATH_FORWARD);
        });
        thread.join();
        ProfilingSpan_ span(AADProfilingPhase_::Value_::REDUCE);
    }
    ASSERT_TRUE(foreignCaptureRejected);
    ASSERT_EQ(parent.Phase(AADProfilingPhase_::Value_::PATH_FORWARD).calls_, 0);
    ASSERT_EQ(worker.Phase(AADProfilingPhase_::Value_::PATH_FORWARD).calls_, 1);
    ASSERT_EQ(parent.Phase(AADProfilingPhase_::Value_::REDUCE).calls_, 1);
    ASSERT_TRUE(parent.complete_);
    ASSERT_TRUE(worker.complete_);
}

TEST(AADProfilingTest, TestActualBlockAllocationsDistinguishGrowthReuseAndClear) {
    ProfilingData_ data;
    {
        ProfilingScope_ profile(&data);
        BlockList_<double, 4> blocks;
        ASSERT_EQ(data.blockAllocations_, 1);
        ASSERT_EQ(data.allocatedArrayBytes_, 4 * sizeof(double));
        for (size_t i = 0; i < 5; ++i)
            blocks.EmplaceBack();
        ASSERT_EQ(blocks.AllocatedBlocks(), 2);
        ASSERT_EQ(data.blockAllocations_, 2);
        blocks.Rewind();
        for (size_t i = 0; i < 5; ++i)
            blocks.EmplaceBack();
        ASSERT_EQ(data.blockAllocations_, 2);
        blocks.Clear();
        ASSERT_EQ(blocks.AllocatedBlocks(), 1);
        ASSERT_EQ(data.blockAllocations_, 3);
        ASSERT_EQ(data.allocatedArrayBytes_, 12 * sizeof(double));
    }
    ASSERT_TRUE(data.complete_);
}

TEST(AADProfilingTest, TestExplicitSpanFinishIsIdempotent) {
    ProfilingData_ data;
    {
        ProfilingScope_ profile(&data);
        ProfilingSpan_ span(AADProfilingPhase_::Value_::WORKER_INIT);
        span.Finish();
        span.Finish();
        ProfilingSpan_ next(AADProfilingPhase_::Value_::PAYOFF);
    }
    ASSERT_EQ(data.Phase(AADProfilingPhase_::Value_::WORKER_INIT).calls_, 1);
    ASSERT_EQ(data.Phase(AADProfilingPhase_::Value_::PAYOFF).calls_, 1);
    ASSERT_TRUE(data.complete_);
}

TEST(AADProfilingTest, TestUnexecutedTaskPreventsCompleteMeasurement) {
    ProfilingData_ data;
    {
        ProfilingScope_ profile(&data);
        ProfilingTaskSet_ tasks(2);
        ASSERT_THROW(static_cast<void>(tasks.Data(2)), Exception_);
        ProfilingTaskScope_ task(tasks, 0);
        ProfilingSpan_ span(AADProfilingPhase_::Value_::PAYOFF);
    }
    ASSERT_EQ(data.taskGroups_.size(), 1);
    ASSERT_TRUE(data.taskGroups_.front()[0].complete_);
    ASSERT_FALSE(data.taskGroups_.front()[1].complete_);
    ASSERT_FALSE(data.complete_);
}

TEST(AADProfilingTest, TestMeasurementCounterOverflowDoesNotWrapOrReportCompletion) {
    ProfilingData_ data;
    {
        ProfilingScope_ profile(&data);
        data.blockAllocations_ = std::numeric_limits<std::uint64_t>::max();
        BlockList_<double, 4> blocks;
        ASSERT_EQ(data.blockAllocations_, std::numeric_limits<std::uint64_t>::max());
        ASSERT_TRUE(data.invalidMeasurement_);
    }
    ASSERT_FALSE(data.complete_);
}

TEST(AADProfilingTest, TestFullFinalNodeBlockIsSampledBeforeRewind) {
    Clear(*Tape());
    ProfilingData_ data;
    {
        ProfilingScope_ profile(&data);
        RecordingScope_ recording;
        Vector_<Number_> inputs(BLOCK_SIZE);
        for (auto& input : inputs)
            recording.RegisterInput(input, 1.0);
        profile.CaptureTape(*Tape());
        recording.StartRecording();
        recording.FinishRecording();
        recording.Close();
        profile.CaptureTape(*Tape());
    }
    ASSERT_EQ(data.highWater_.nodes_, BLOCK_SIZE);
    ASSERT_EQ(data.highWater_.edges_, 0);
    ASSERT_EQ(data.highWater_.blocks_, 4);
    ASSERT_EQ(data.highWater_.liveBytes_, BLOCK_SIZE * sizeof(TapNode_));
    ASSERT_EQ(data.tapeSamples_, 2);
    ASSERT_TRUE(data.complete_);
}
#endif

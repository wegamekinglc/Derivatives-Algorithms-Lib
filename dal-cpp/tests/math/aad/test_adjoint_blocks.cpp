//
// Created by Codex on 2026/10/06.
//

#include <gtest/gtest.h>

#include <limits>

#include <dal/math/aad/adjointblocks.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/math/aad/recording.hpp>
#include <dal/math/aad/statistics.hpp>

using namespace Dal;
using namespace Dal::AAD;

TEST(AADAdjointBlockTest, TestPartitionsWithoutAllocatingOutputDescriptors) {
    AdjointBlockSettings_ settings;
    settings.maxWidth_ = 4;
    const auto plan = PlanAdjointBlocks(10, 3, settings);
    ASSERT_EQ(plan.Width(), 4);
    ASSERT_EQ(plan.BlockCount(), 3);
    ASSERT_EQ(plan.Block(0).firstOutput_, 0);
    ASSERT_EQ(plan.Block(1).firstOutput_, 4);
    ASSERT_EQ(plan.Block(2).firstOutput_, 8);
    ASSERT_EQ(plan.Block(2).outputs_, 2);
    ASSERT_EQ(plan.Block(2).width_, 4);
    ASSERT_EQ(plan.NumericResultBytes(), 320);
    ASSERT_THROW(plan.Block(3), Exception_);
    ASSERT_EQ(PlanAdjointBlocks(3, 0).BlockCount(), 3);
    ASSERT_EQ(PlanAdjointBlocks(3, 0).NumericResultBytes(), 24);
    settings.maxWidth_ = ADJ_SIZE;
    const auto maximumWidth = PlanAdjointBlocks(ADJ_SIZE + 1, 0, settings);
    ASSERT_EQ(maximumWidth.Width(), ADJ_SIZE);
    ASSERT_EQ(maximumWidth.BlockCount(), 2);
    ASSERT_EQ(maximumWidth.Block(1).outputs_, 1);
}

TEST(AADAdjointBlockTest, TestBudgetsNarrowWidthAndRejectBelowOneLane) {
    AdjointBlockSettings_ settings;
    settings.maxWidth_ = 4;
    settings.concurrentWorkers_ = 2;
    settings.batchResultSlots_ = 3;
    const size_t laneBytes = 2 * sizeof(Number_) + 3 * 3 * sizeof(double);
    settings.numericResultBudgetBytes_ = 120;
    settings.minimumScratchBudgetBytes_ = 2 * laneBytes;
    const auto plan = PlanAdjointBlocks(5, 2, settings);
    ASSERT_EQ(plan.Width(), 2);
    ASSERT_EQ(plan.BlockCount(), 3);
    ASSERT_EQ(plan.Block(2).outputs_, 1);
    ASSERT_EQ(plan.MinimumScratchBytes(), 2 * laneBytes);
    settings.minimumScratchBudgetBytes_ = laneBytes - 1;
    ASSERT_THROW(PlanAdjointBlocks(5, 2, settings), Exception_);
    settings.minimumScratchBudgetBytes_ = laneBytes;
    ASSERT_EQ(PlanAdjointBlocks(5, 2, settings).Width(), 1);
    settings.numericResultBudgetBytes_ = 119;
    ASSERT_THROW(PlanAdjointBlocks(5, 2, settings), Exception_);
    settings.numericResultBudgetBytes_ = 0;
    ASSERT_THROW(PlanAdjointBlocks(5, 2, settings), Exception_);
    settings.numericResultBudgetBytes_.reset();
    settings.minimumScratchBudgetBytes_ = 0;
    ASSERT_THROW(PlanAdjointBlocks(5, 2, settings), Exception_);
}

TEST(AADAdjointBlockTest, TestInvalidDimensionsWidthAndWorkerGeometryReject) {
    ASSERT_THROW(PlanAdjointBlocks(0, 1), Exception_);
    ASSERT_THROW(PlanAdjointBlocks(static_cast<size_t>(std::numeric_limits<int>::max()) + 1, 1), Exception_);
    ASSERT_THROW(PlanAdjointBlocks(1, static_cast<size_t>(std::numeric_limits<int>::max()) + 1), Exception_);
    AdjointBlockSettings_ settings;
    settings.maxWidth_ = 0;
    ASSERT_THROW(PlanAdjointBlocks(1, 1, settings), Exception_);
    settings.maxWidth_ = ADJ_SIZE + 1;
    ASSERT_THROW(PlanAdjointBlocks(1, 1, settings), Exception_);
    settings.maxWidth_ = 1;
    settings.concurrentWorkers_ = 0;
    ASSERT_THROW(PlanAdjointBlocks(1, 1, settings), Exception_);
    settings.concurrentWorkers_ = 2;
    ASSERT_THROW(PlanAdjointBlocks(1, 1, settings), Exception_);
    settings.batchResultSlots_ = 2;
    ASSERT_EQ(PlanAdjointBlocks(1, 1, settings).Width(), 1);
}

TEST(AADAdjointBlockTest, TestOverflowPlanningPreservesLiveRecording) {
    Clear(*Tape());
    RecordingScope_ recording;
    Number_ input;
    recording.RegisterInput(input, 2.0);
    recording.StartRecording();
    Number_ output = input * input;
    recording.FinishRecording();
    const auto before = MeasureTape(*Tape());
    const auto maximum = static_cast<size_t>(std::numeric_limits<int>::max());
    ASSERT_THROW(PlanAdjointBlocks(maximum, maximum), Exception_);
    AdjointBlockSettings_ settings;
    settings.concurrentWorkers_ = std::numeric_limits<size_t>::max();
    settings.batchResultSlots_ = settings.concurrentWorkers_;
    ASSERT_THROW(PlanAdjointBlocks(1, 0, settings), Exception_);
    settings.concurrentWorkers_ = std::numeric_limits<size_t>::max() / sizeof(Number_);
    settings.batchResultSlots_ = settings.concurrentWorkers_;
    ASSERT_THROW(PlanAdjointBlocks(1, 0, settings), Exception_);
    settings.maxWidth_ = 4;
    settings.concurrentWorkers_ = std::numeric_limits<size_t>::max() / (4 * (sizeof(Number_) + sizeof(double))) + 1;
    settings.batchResultSlots_ = settings.concurrentWorkers_;
    ASSERT_THROW(PlanAdjointBlocks(4, 0, settings), Exception_);
    ASSERT_EQ(PlanAdjointBlocks(9, 1).BlockCount(), 9);
    const auto after = MeasureTape(*Tape());
    ASSERT_EQ(after.nodes_, before.nodes_);
    ASSERT_EQ(after.edges_, before.edges_);
    ASSERT_EQ(after.capacityBytes_, before.capacityBytes_);
    NativeOperations_::SetSeed(output, 1.0);
    recording.Reverse();
    ASSERT_NEAR(NativeOperations_::ReadAdjoint(input), 4.0, 1e-10);
    recording.Close();
    Clear(*Tape());
}

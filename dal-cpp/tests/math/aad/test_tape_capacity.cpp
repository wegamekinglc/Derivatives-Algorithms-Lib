//
// Created by Codex on 2026/10/06.
//

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <dal/math/aad/blockallocation.hpp>
#include <dal/math/aad/recording.hpp>
#include <dal/math/aad/statistics.hpp>
#include <dal/math/aad/tapecapacity.hpp>
#include <future>
#include <limits>
#include <new>
#include <tuple>

using namespace Dal;
using namespace Dal::AAD;

TEST(AADTapeCapacityTest, TestColdWorkerReservesBeforeInitialTapeAllocation) {
    const size_t nodeBytes = sizeof(std::array<TapNode_, BLOCK_SIZE>);
    const size_t initial =
        nodeBytes + sizeof(std::array<double, DATA_SIZE>) + sizeof(std::array<double*, DATA_SIZE>) + sizeof(std::array<double, ADJ_SIZE>);
    auto worker = std::async(std::launch::async, [initial] {
        TapeCapacityBudget_ insufficient(initial - 1);
        bool rejected = false;
        try {
            TapeCapacityScope_ capacity(&insufficient);
        } catch (const Exception_&) {
            rejected = true;
        }
        const auto failedCurrent = insufficient.CapacityBytes();
        const auto failedPeak = insufficient.PeakCapacityBytes();
        TapeCapacityBudget_ exact(initial);
        {
            TapeCapacityScope_ capacity(&exact);
            RecordingScope_ recording;
            Number_ input;
            recording.RegisterInput(input, 3.0);
            recording.StartRecording();
            Number_ output = input * input;
            recording.FinishRecording();
            Adjoint(output) = 1.0;
            recording.Reverse();
            REQUIRE(AdjointValue(input) == 6.0, "Cold worker recovery produced an invalid derivative");
            recording.Close();
        }
        return std::make_tuple(rejected, failedCurrent, failedPeak, exact.CapacityBytes());
    });
    const auto result = worker.get();
    ASSERT_TRUE(std::get<0>(result));
    ASSERT_EQ(std::get<1>(result), 0);
    ASSERT_EQ(std::get<2>(result), initial - nodeBytes);
    ASSERT_EQ(std::get<3>(result), initial);
}

TEST(AADTapeCapacityTest, TestRejectsBeforeAllocationAndRecovers) {
    Clear(*Tape());
    const auto initial = MeasureTape(*Tape()).capacityBytes_;
    TapeCapacityBudget_ insufficient(initial - 1);
    ASSERT_THROW(TapeCapacityScope_ scope(&insufficient), Exception_);
    ASSERT_EQ(insufficient.CapacityBytes(), 0);
    ASSERT_EQ(MeasureTape(*Tape()).capacityBytes_, initial);

    TapeCapacityBudget_ budget(initial);
    {
        TapeCapacityScope_ capacity(&budget);
        RecordingScope_ recording;
        Number_ input;
        recording.RegisterInput(input, 2.0);
        recording.StartRecording();
        Number_ output = input;
        for (size_t i = 1; i < BLOCK_SIZE; ++i)
            output = output + input;
        ASSERT_THROW(output = output + input, Exception_);
        ASSERT_EQ(Tape()->nodes_.AllocatedBlocks(), 1);
        ASSERT_EQ(budget.CapacityBytes(), initial);
        recording.Close();
    }
    ASSERT_EQ(budget.CapacityBytes(), initial);
    ASSERT_EQ(budget.PeakCapacityBytes(), initial);

    RecordingScope_ recovered;
    Number_ input;
    recovered.RegisterInput(input, 3.0);
    recovered.StartRecording();
    Number_ output = input * input;
    recovered.FinishRecording();
    Adjoint(output) = 1.0;
    recovered.Reverse();
    ASSERT_DOUBLE_EQ(AdjointValue(input), 6.0);
}

TEST(AADTapeCapacityTest, TestCleanupReservationSurvivesInputAllocationFailure) {
    Clear(*Tape());
    const auto initial = MeasureTape(*Tape()).capacityBytes_;
    const size_t cleanup = std::max({sizeof(std::array<TapNode_, BLOCK_SIZE>), sizeof(std::array<double, DATA_SIZE>),
                                     sizeof(std::array<double*, DATA_SIZE>), sizeof(std::array<double, ADJ_SIZE>)});
    TapeCapacityBudget_ budget(initial + cleanup);
    TapeCapacityScope_ capacity(&budget, true);
    {
        RecordingScope_ recording;
        Number_ input;
        for (size_t i = 0; i < BLOCK_SIZE; ++i)
            recording.RegisterInput(input, 2.0);
        ASSERT_THROW(recording.RegisterInput(input, 2.0), Exception_);
        ASSERT_EQ(Tape()->nodes_.AllocatedBlocks(), 1);
        ASSERT_EQ(budget.CapacityBytes(), initial);
        recording.Close();
    }
    {
        RecordingScope_ recovered;
        Number_ input;
        recovered.RegisterInput(input, 3.0);
        recovered.StartRecording();
        Number_ output = input * input;
        recovered.FinishRecording();
        Adjoint(output) = 1.0;
        recovered.Reverse();
        ASSERT_DOUBLE_EQ(AdjointValue(input), 6.0);
        recovered.Close();
    }
    ASSERT_EQ(budget.CapacityBytes(), initial);
    ASSERT_EQ(budget.PeakCapacityBytes(), initial + cleanup);
    capacity.Close();
    TapeCapacityScope_ readmit(&budget, true);
    ASSERT_EQ(budget.CapacityBytes(), initial);
}

TEST(AADTapeCapacityTest, TestGrowthCannotBorrowCleanupSpaceDuringReplacement) {
    Clear(*Tape());
    const auto initial = MeasureTape(*Tape()).capacityBytes_;
    const auto cleanup = TapeCleanupCapacityBytes();
    TapeCapacityBudget_ budget(initial + cleanup);
    TapeCapacityScope_ capacity(&budget, true);
    {
        BlockAllocationTicket_ replacement(&Tape()->nodes_, cleanup, true);
        ASSERT_EQ(budget.CapacityBytes(), budget.LimitBytes());
        ASSERT_THROW(BlockAllocationTicket_ growth(&Tape()->adjointsMulti_, ADJ_SIZE * sizeof(double)), Exception_);
        ASSERT_EQ(budget.CapacityBytes(), budget.LimitBytes());
    }
    ASSERT_EQ(budget.CapacityBytes(), initial);
    ASSERT_EQ(budget.PeakCapacityBytes(), budget.LimitBytes());
}

TEST(AADTapeCapacityTest, TestConcurrentCleanupReservationsProtectEveryWorker) {
    const size_t initial = sizeof(std::array<TapNode_, BLOCK_SIZE>) + sizeof(std::array<double, DATA_SIZE>) + sizeof(std::array<double*, DATA_SIZE>) +
                           sizeof(std::array<double, ADJ_SIZE>);
    const auto cleanup = TapeCleanupCapacityBytes();
    TapeCapacityBudget_ budget(2 * (initial + cleanup));
    std::promise<void> start, exit;
    const auto startGate = start.get_future().share();
    const auto exitGate = exit.get_future().share();
    std::array<std::promise<void>, 2> admitted, borrowed;
    std::array<std::future<void>, 2> workers;
    for (size_t i = 0; i < workers.size(); ++i) {
        workers[i] = std::async(std::launch::async, [&, i] {
            TapeCapacityScope_ capacity(&budget, true);
            admitted[i].set_value();
            startGate.wait();
            {
                BlockAllocationTicket_ replacement(&Tape()->nodes_, cleanup, true);
                borrowed[i].set_value();
                exitGate.wait();
            }
            Clear(*Tape());
        });
    }
    bool admissionsReady = true;
    for (auto& ready : admitted)
        admissionsReady &= ready.get_future().wait_for(std::chrono::seconds(5)) == std::future_status::ready;
    start.set_value();
    bool replacementsReady = true;
    for (auto& ready : borrowed)
        replacementsReady &= ready.get_future().wait_for(std::chrono::seconds(5)) == std::future_status::ready;
    const auto overlap = budget.CapacityBytes();
    exit.set_value();
    workers[0].get();
    workers[1].get();
    ASSERT_TRUE(admissionsReady);
    ASSERT_TRUE(replacementsReady);
    ASSERT_EQ(overlap, budget.LimitBytes());
    ASSERT_EQ(budget.CapacityBytes(), 2 * initial);
    ASSERT_EQ(budget.PeakCapacityBytes(), budget.LimitBytes());
}

TEST(AADTapeCapacityTest, TestAccountsAllListsAndSkippedTails) {
    Clear(*Tape());
    const auto initial = MeasureTape(*Tape()).capacityBytes_;
    const size_t dataBlock = DATA_SIZE * sizeof(double);
    {
        TapeCapacityBudget_ budget(initial + dataBlock - 1);
        TapeCapacityScope_ capacity(&budget);
        Tape()->ders_.EmplaceBackMulti(DATA_SIZE);
        ASSERT_THROW(Tape()->ders_.EmplaceBackMulti(1), Exception_);
        ASSERT_EQ(Tape()->ders_.AllocatedBlocks(), 1);
        ASSERT_EQ(budget.CapacityBytes(), initial);
    }
    Clear(*Tape());
    {
        TapeCapacityBudget_ budget(initial + DATA_SIZE * sizeof(double*) - 1);
        TapeCapacityScope_ capacity(&budget);
        Tape()->argPtrs_.EmplaceBackMulti(DATA_SIZE);
        ASSERT_THROW(Tape()->argPtrs_.EmplaceBackMulti(1), Exception_);
        ASSERT_EQ(Tape()->argPtrs_.AllocatedBlocks(), 1);
        ASSERT_EQ(budget.CapacityBytes(), initial);
    }
    Clear(*Tape());
    const size_t adjointBlock = ADJ_SIZE * sizeof(double);
    TapeCapacityBudget_ budget(initial + adjointBlock);
    {
        TapeCapacityScope_ capacity(&budget);
        Tape()->adjointsMulti_.EmplaceBackMulti(ADJ_SIZE - 1);
        Tape()->adjointsMulti_.EmplaceBackMulti(2);
        ASSERT_EQ(Tape()->adjointsMulti_.OccupiedSlots(), ADJ_SIZE + 2);
        ASSERT_EQ(budget.CapacityBytes(), initial + adjointBlock);
        ASSERT_THROW(Tape()->adjointsMulti_.EmplaceBackMulti(ADJ_SIZE), Exception_);
        ASSERT_EQ(Tape()->adjointsMulti_.AllocatedBlocks(), 2);
        BlockList_<double, 4> unrelated;
        unrelated.EmplaceBackMulti(4);
        unrelated.EmplaceBack();
        unrelated.Clear();
        ASSERT_EQ(budget.CapacityBytes(), initial + adjointBlock);
    }
    {
        TapeCapacityScope_ readmit(&budget);
        ASSERT_EQ(budget.CapacityBytes(), initial + adjointBlock);
    }
    TapeCapacityBudget_ smaller(initial);
    ASSERT_THROW(TapeCapacityScope_ readmit(&smaller), Exception_);
    ASSERT_EQ(smaller.CapacityBytes(), 0);
    ASSERT_EQ(Tape()->adjointsMulti_.AllocatedBlocks(), 2);
    Clear(*Tape());
}

TEST(AADTapeCapacityTest, TestClearReservesTransientOverlapAndPreservesGraphOnRejection) {
    Clear(*Tape());
    const auto initial = MeasureTape(*Tape()).capacityBytes_;
    {
        TapeCapacityBudget_ budget(initial);
        TapeCapacityScope_ capacity(&budget);
        RecordingScope_ recording;
        Number_ input;
        recording.RegisterInput(input, 4.0);
        recording.StartRecording();
        Number_ output = input * input;
        recording.FinishRecording();
        ASSERT_THROW(Tape()->nodes_.Clear(), Exception_);
        ASSERT_EQ(budget.CapacityBytes(), initial);
        Adjoint(output) = 1.0;
        recording.Reverse();
        ASSERT_DOUBLE_EQ(AdjointValue(input), 8.0);
        recording.Close();
    }
    const size_t adjointBlock = ADJ_SIZE * sizeof(double);
    TapeCapacityBudget_ budget(initial + 2 * adjointBlock);
    {
        TapeCapacityScope_ capacity(&budget);
        Tape()->adjointsMulti_.EmplaceBackMulti(ADJ_SIZE);
        Tape()->adjointsMulti_.EmplaceBack();
        ASSERT_EQ(budget.CapacityBytes(), initial + adjointBlock);
        Tape()->adjointsMulti_.Clear();
        ASSERT_EQ(Tape()->adjointsMulti_.AllocatedBlocks(), 1);
        ASSERT_EQ(Tape()->adjointsMulti_.OccupiedSlots(), 0);
        ASSERT_EQ(budget.CapacityBytes(), initial);
        ASSERT_EQ(budget.PeakCapacityBytes(), initial + 2 * adjointBlock);
    }
}

TEST(AADTapeCapacityTest, TestRefundsFailedAllocationAndChecksOverflow) {
    Clear(*Tape());
    const auto initial = MeasureTape(*Tape()).capacityBytes_;
    const size_t bytes = DATA_SIZE * sizeof(double);
    TapeCapacityBudget_ budget(std::numeric_limits<size_t>::max());
    TapeCapacityScope_ capacity(&budget);
    ASSERT_THROW(
        {
            BlockAllocationTicket_ allocation(&Tape()->ders_, bytes);
            ASSERT_EQ(budget.CapacityBytes(), initial + bytes);
            throw std::bad_alloc();
        },
        std::bad_alloc);
    ASSERT_EQ(budget.CapacityBytes(), initial);
    ASSERT_EQ(Tape()->ders_.AllocatedBlocks(), 1);
    ASSERT_THROW(BlockAllocationTicket_ allocation(&Tape()->ders_, std::numeric_limits<size_t>::max()), Exception_);
    ASSERT_EQ(budget.CapacityBytes(), initial);
}

TEST(AADTapeCapacityTest, TestRejectsNestedRecordingAndForeignCloseWithoutChangingOwner) {
    Clear(*Tape());
    const auto initial = MeasureTape(*Tape()).capacityBytes_;
    TapeCapacityBudget_ budget(initial);
    ASSERT_THROW(TapeCapacityScope_ invalid(nullptr), Exception_);
    {
        RecordingScope_ recording;
        ASSERT_THROW(TapeCapacityScope_ invalid(&budget), Exception_);
        ASSERT_EQ(budget.CapacityBytes(), 0);
        recording.Close();
    }
    TapeCapacityScope_ capacity(&budget);
    ASSERT_THROW(TapeCapacityScope_ nested(&budget), Exception_);
    auto foreign = std::async(std::launch::async, [&capacity] {
        try {
            capacity.Close();
            return false;
        } catch (const Exception_&) {
            return true;
        }
    });
    ASSERT_TRUE(foreign.get());
    {
        RecordingScope_ recording;
        ASSERT_THROW(capacity.Close(), Exception_);
        recording.Close();
    }
    capacity.Close();
    capacity.Close();
    TapeCapacityScope_ readmit(&budget);
    ASSERT_EQ(budget.CapacityBytes(), initial);
}

TEST(AADTapeCapacityTest, TestConcurrentWorkersKeepDetachedCachedCapacityCharged) {
    Clear(*Tape());
    const auto initial = MeasureTape(*Tape()).capacityBytes_;
    const size_t adjointBlock = ADJ_SIZE * sizeof(double);
    TapeCapacityBudget_ budget(2 * initial + adjointBlock);
    std::promise<void> grow;
    const auto growGate = grow.get_future().share();
    std::promise<void> exit;
    const auto exitGate = exit.get_future().share();
    std::array<std::promise<void>, 2> admitted, detached;
    std::array<std::future<bool>, 2> workers;
    for (size_t i = 0; i < workers.size(); ++i) {
        workers[i] = std::async(std::launch::async, [&, i] {
            Clear(*Tape());
            bool grew = false;
            {
                TapeCapacityScope_ capacity(&budget);
                admitted[i].set_value();
                growGate.wait();
                Tape()->adjointsMulti_.EmplaceBackMulti(ADJ_SIZE);
                try {
                    Tape()->adjointsMulti_.EmplaceBack();
                    grew = true;
                } catch (const Exception_&) {
                }
            }
            detached[i].set_value();
            exitGate.wait();
            return grew;
        });
    }
    bool admissionsReady = true;
    for (auto& ready : admitted)
        admissionsReady &= ready.get_future().wait_for(std::chrono::seconds(5)) == std::future_status::ready;
    const auto admittedCapacity = budget.CapacityBytes();
    grow.set_value();
    bool detachmentsReady = true;
    for (auto& ready : detached)
        detachmentsReady &= ready.get_future().wait_for(std::chrono::seconds(5)) == std::future_status::ready;
    const auto retainedCapacity = budget.CapacityBytes();
    bool thirdRejected = false;
    try {
        TapeCapacityScope_ third(&budget);
    } catch (const Exception_&) {
        thirdRejected = true;
    }
    exit.set_value();
    const auto first = workers[0].get();
    const auto second = workers[1].get();
    ASSERT_TRUE(admissionsReady);
    ASSERT_TRUE(detachmentsReady);
    ASSERT_EQ(admittedCapacity, 2 * initial);
    ASSERT_EQ(retainedCapacity, 2 * initial + adjointBlock);
    ASSERT_NE(first, second);
    ASSERT_TRUE(thirdRejected);
    ASSERT_EQ(budget.PeakCapacityBytes(), budget.LimitBytes());
}

//
// Created on 2026/10/04.
//

#include <dal/math/aad/aad.hpp>
#include <dal/math/aad/statistics.hpp>
#include <dal/platform/platform.hpp>
#include <gtest/gtest.h>

using namespace Dal::AAD;

TEST(AADTapeStatisticsTest, TestLogicalStorageAndReusedCapacityAreDistinct) {
    Clear(*Tape());
    const auto empty = MeasureTape(*Tape());
    ASSERT_EQ(empty.nodes_, 0);
    ASSERT_EQ(empty.edges_, 0);
    ASSERT_EQ(empty.liveBytes_, 0);
    ASSERT_EQ(empty.occupiedBytes_, 0);
    ASSERT_EQ(empty.blocks_, 4);
    ASSERT_GT(empty.capacityBytes_, 0);
    {
        auto mode = SetNumResultsForAAD(true, 5);
        Number_ seed(1.0), factor(2.0);
        Number_ result = seed * factor + 0.25;
        ASSERT_DOUBLE_EQ(Value(result), 2.25);
        const auto graph = MeasureTape(*Tape());
        ASSERT_EQ(graph.nodes_, 3);
        ASSERT_EQ(graph.edges_, 2);
        ASSERT_EQ(graph.liveBytes_, 3 * sizeof(TapNode_) + 2 * (sizeof(double) + sizeof(double*)) + 15 * sizeof(double));
        ASSERT_EQ(graph.occupiedBytes_, graph.liveBytes_);
        ASSERT_GE(graph.capacityBytes_, graph.liveBytes_);
        const size_t capacity = graph.capacityBytes_;
        Rewind(*Tape());
        const auto rewound = MeasureTape(*Tape());
        ASSERT_EQ(rewound.nodes_, 0);
        ASSERT_EQ(rewound.edges_, 0);
        ASSERT_EQ(rewound.liveBytes_, 0);
        ASSERT_EQ(rewound.occupiedBytes_, 0);
        ASSERT_EQ(rewound.capacityBytes_, capacity);
    }
    Clear(*Tape());
}

TEST(AADTapeStatisticsTest, TestFullNodeBlockWithoutAllocatingAnotherBlock) {
    Tape_ tape(false);
    for (size_t i = 0; i < BLOCK_SIZE; ++i)
        tape.RecordNode<0>();
    const auto usage = MeasureTape(tape);
    ASSERT_EQ(usage.nodes_, BLOCK_SIZE);
    ASSERT_EQ(usage.edges_, 0);
    ASSERT_EQ(usage.blocks_, 4);
    ASSERT_EQ(usage.liveBytes_, BLOCK_SIZE * sizeof(TapNode_));
    tape.RecordNode<0>();
    const auto rolled = MeasureTape(tape);
    ASSERT_EQ(rolled.nodes_, BLOCK_SIZE + 1);
    ASSERT_EQ(rolled.blocks_, 5);
    ASSERT_EQ(rolled.liveBytes_, (BLOCK_SIZE + 1) * sizeof(TapNode_));
}

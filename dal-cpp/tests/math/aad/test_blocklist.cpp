//
// Created by wegam on 2022/4/18.
//

#include <gtest/gtest.h>

#include <utility>
#include <vector>

#include <dal/platform/platform.hpp>
#include <dal/math/aad/blocklist.hpp>

using namespace Dal::AAD;

TEST(AADTest, TestBlockListInit) {
    BlockList_<double, 10> blocks;
    ASSERT_EQ(blocks.Size(), 0);
}

TEST(AADTest, TestBlockListEmplaceBack) {
    BlockList_<double, 10> blocks;
    blocks.EmplaceBack();
    ASSERT_EQ(blocks.Size(), 1);
}

TEST(AADTest, TestBlockListEmplaceBackMulti) {
    BlockList_<double, 10> blocks;
    blocks.EmplaceBackMulti(5);
    ASSERT_EQ(blocks.Size(), 5);
}

TEST(AADTest, TestBlockListSizeAtFullFinalBlockDoesNotAllocateOrTraversePastEnd) {
    BlockList_<double, 4> blocks;
    blocks.EmplaceBackMulti(4);
    ASSERT_EQ(blocks.Size(), 4);
    ASSERT_EQ(blocks.AllocatedBlocks(), 1);
}

TEST(AADTest, TestBlockListSizeAtFullReusedBlockExcludesRetainedCapacity) {
    BlockList_<double, 4> blocks;
    blocks.EmplaceBackMulti(3);
    blocks.EmplaceBackMulti(2);
    ASSERT_EQ(blocks.Size(), 6);
    ASSERT_EQ(blocks.AllocatedBlocks(), 2);
    blocks.Rewind();
    blocks.EmplaceBackMulti(4);
    ASSERT_EQ(blocks.Size(), 4);
    ASSERT_EQ(blocks.AllocatedBlocks(), 2);
}

TEST(AADTest, TestBlockListRewind) {
    BlockList_<double, 10> blocks;
    blocks.EmplaceBackMulti(5);
    blocks.Rewind();
    ASSERT_EQ(blocks.Size(), 0);
}

TEST(AADTest, TestBlockListRewindToMark) {
    BlockList_<double, 10> blocks;
    blocks.EmplaceBackMulti(5);
    blocks.SetMark();
    blocks.EmplaceBackMulti(3);
    ASSERT_EQ(blocks.Size(), 8);
    blocks.RewindToMark();
    ASSERT_EQ(blocks.Size(), 5);
}

TEST(AADTest, TestBlockListEmplaceBackMultiRejectsOversizedN) {
    BlockList_<double, 10> blocks;
    ASSERT_THROW(blocks.EmplaceBackMulti(11), Dal::Exception_);
    ASSERT_THROW(blocks.EmplaceBackMulti(0), Dal::Exception_);
}

TEST(AADTest, TestBlockListMarkDefaultsToStart) {
    BlockList_<double, 4> blocks;
    ASSERT_TRUE(blocks.Mark() == blocks.Begin());
    blocks.EmplaceBackMulti(3);
    blocks.RewindToMark();
    ASSERT_EQ(blocks.Size(), 0);
}

TEST(AADTest, TestBlockListClearResetsMark) {
    BlockList_<double, 4> blocks;
    blocks.EmplaceBackMulti(4);
    blocks.EmplaceBackMulti(2);
    blocks.SetMark();
    blocks.Clear();
    ASSERT_TRUE(blocks.Mark() == blocks.Begin());
    blocks.EmplaceBackMulti(3);
    blocks.RewindToMark();
    ASSERT_EQ(blocks.Size(), 0);
}

TEST(AADTest, TestBlockListStorageIncludesPaddingAndRetainsCapacityAfterRewind) {
    BlockList_<double, 4> blocks;
    ASSERT_EQ(blocks.AllocatedBlocks(), 1);
    ASSERT_EQ(blocks.OccupiedSlots(), 0);
    blocks.EmplaceBackMulti(3);
    blocks.SetMark();
    blocks.EmplaceBackMulti(2);
    ASSERT_EQ(blocks.AllocatedBlocks(), 2);
    ASSERT_EQ(blocks.OccupiedSlots(), 6);
    blocks.RewindToMark();
    ASSERT_EQ(blocks.OccupiedSlots(), 3);
    ASSERT_EQ(blocks.AllocatedBlocks(), 2);
    blocks.Rewind();
    ASSERT_EQ(blocks.OccupiedSlots(), 0);
    ASSERT_EQ(blocks.AllocatedBlocks(), 2);
    blocks.EmplaceBackMulti(4);
    ASSERT_EQ(blocks.OccupiedSlots(), 4);
    blocks.SetMark();
    ASSERT_EQ(blocks.OccupiedSlots(), 4);
    ASSERT_EQ(blocks.AllocatedBlocks(), 2);
    blocks.Clear();
    ASSERT_EQ(blocks.OccupiedSlots(), 0);
    ASSERT_EQ(blocks.AllocatedBlocks(), 1);
}

TEST(AADTest, TestBlockListLiveRangesExcludeUnoccupiedAndRewoundStorage) {
    BlockList_<double, 4> blocks;
    std::vector<std::pair<const double*, const double*>> ranges;
    const auto collect = [&] {
        ranges.clear();
        blocks.ForEachLiveRange([&](const double* first, const double* last) { ranges.emplace_back(first, last); });
    };
    collect();
    ASSERT_TRUE(ranges.empty());
    const auto* first = blocks.EmplaceBackMulti(4);
    collect();
    ASSERT_EQ(ranges.size(), 1);
    ASSERT_EQ(ranges[0].first, first);
    ASSERT_EQ(ranges[0].second, first + 4);
    blocks.SetMark();
    const auto* second = blocks.EmplaceBackMulti(2);
    collect();
    ASSERT_EQ(ranges.size(), 2);
    ASSERT_EQ(ranges[1].first, second);
    ASSERT_EQ(ranges[1].second, second + 2);
    blocks.RewindToMark();
    collect();
    ASSERT_EQ(ranges.size(), 1);
    ASSERT_EQ(ranges[0].second, first + 4);
    ASSERT_EQ(blocks.AllocatedBlocks(), 2);
    blocks.Rewind();
    collect();
    ASSERT_TRUE(ranges.empty());
    blocks.EmplaceBack();
    collect();
    ASSERT_EQ(ranges.size(), 1);
    ASSERT_EQ(ranges[0].second, first + 1);
}

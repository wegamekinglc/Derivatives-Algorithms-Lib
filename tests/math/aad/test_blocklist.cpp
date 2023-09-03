//
// Created by wegam on 2022/4/18.
//


#include <dal/math/aad/blocklist.hpp>
#include <gtest/gtest.h>

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

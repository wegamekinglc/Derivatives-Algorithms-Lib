//
// Created by Codex on 2026/9/13.
//

#include <gtest/gtest.h>

#include <dal/platform/platform.hpp>

#include <dal/storage/globals.hpp>

#include <dal/indice/fixings.hpp>

using namespace Dal;

TEST(FixHistoryTest, TestGlobalFirstHeadersAndStorageRoundTrip) {
    const DateTime_ first(Date_(2026, 9, 11), 0.0);
    const DateTime_ second(Date_(2026, 9, 12), 0.0);
    FixHistory_ global;
    global.vals_ = {{second, 90.0}, {first, 80.0}};
    ASSERT_EQ(XGLOBAL::StoreFixings("EQ[DAL199_HISTORY_IDENTITY]", global, false), 2);
    global.vals_ = {{first, 81.0}};
    ASSERT_EQ(XGLOBAL::StoreFixings("EQ[DAL199_HISTORY_IDENTITY]", global), 2);
    const FixHistory_ restored = Global::Fixings_().History("EQ[DAL199_HISTORY_IDENTITY]");
    ASSERT_EQ(restored.vals_.size(), 2);
    ASSERT_EQ(restored.vals_[0].first, first);
    ASSERT_DOUBLE_EQ(restored.vals_[0].second, 81.0);
    ASSERT_EQ(restored.vals_[1].first, second);
    ASSERT_DOUBLE_EQ(restored.vals_[1].second, 90.0);

    const IndexFixHistory_ indexed({{first, restored.vals_[0].second}, {second, restored.vals_[1].second}});
    ASSERT_DOUBLE_EQ(indexed.Find(first), 81.0);
    ASSERT_DOUBLE_EQ(indexed.Find(second), 90.0);
}

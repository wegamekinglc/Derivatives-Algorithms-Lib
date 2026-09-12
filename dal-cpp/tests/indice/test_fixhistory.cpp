//
// Created by Codex on 2026/9/13.
//

#include <gtest/gtest.h>

#include <string>
#include <type_traits>

#include <dal/platform/platform.hpp>

#include <dal/indice/fixings.hpp>

#include <dal/storage/globals.hpp>

using namespace Dal;

static_assert(!std::is_same_v<IndexFixHistory_, FixHistory_>);
static_assert(std::is_aggregate_v<FixHistory_>);
static_assert(std::is_same_v<decltype(FixHistory::Empty()), const IndexFixHistory_&>);

TEST(FixHistoryTest, TestDistinctHistoriesAndExactLookup) {
    const DateTime_ midnight(Date_(2026, 9, 11), 0.0);
    const DateTime_ intraday(Date_(2026, 9, 11), 11, 0);
    IndexFixHistory_::vals_t values = {{midnight, 0.0}, {intraday, -80.0}};
    const IndexFixHistory_ indexed(values);
    values[midnight] = 100.0;
    ASSERT_DOUBLE_EQ(indexed.Find(midnight), 0.0);
    ASSERT_DOUBLE_EQ(indexed.Find(intraday), -80.0);

    FixHistory_ global;
    global.vals_ = {{midnight, indexed.Find(midnight)}, {intraday, indexed.Find(intraday)}};
    ASSERT_EQ(global.vals_.size(), 2);
    ASSERT_EQ(global.vals_[0].first, midnight);
    ASSERT_DOUBLE_EQ(global.vals_[0].second, 0.0);
    ASSERT_DOUBLE_EQ(global.vals_[1].second, -80.0);

    const DateTime_ missing(Date_(2026, 9, 11), 10, 0);
    ASSERT_DOUBLE_EQ(indexed.Find(missing, true), -INF);
    try {
        static_cast<void>(indexed.Find(missing));
        FAIL() << "a different timestamp must remain missing";
    } catch (const Exception_& error) {
        ASSERT_NE(std::string(error.what()).find("no fixings for that time"), std::string::npos);
    }
}

TEST(FixHistoryTest, TestEmptyHistorySingleton) {
    const IndexFixHistory_& first = FixHistory::Empty();
    ASSERT_EQ(&first, &FixHistory::Empty());
    const DateTime_ time(Date_(2026, 9, 11), 0.0);
    ASSERT_DOUBLE_EQ(first.Find(time, true), -INF);
    ASSERT_THROW(first.Find(time), Exception_);
}

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <cmath>
#include <limits>
#include <thread>
#include <utility>

#include "../../benchmarks/script_mc_perf/lsmcstoragestudy.hpp"

using Dal::Script::StudyReadRow_;
using Dal::Script::StudyRows_;

TEST(LsmcStorageStudyTest, TestBinary32WritesRoundAndReadsPromoteToDouble) {
    StudyRows_<float> rows(2, 3);
    rows.ZeroPaths(0, 3);
    rows[0][0] = 16777217.0;
    rows[0][1] = 1.25;
    rows[0][2] = 0.1;
    const auto& stored = rows;
    ASSERT_EQ(stored[0][0], 16777216.0);
    ASSERT_EQ(stored[0][1], 1.25);
    ASSERT_EQ(stored[0][2], static_cast<double>(0.1f));
    ASSERT_EQ(stored[1][0], 0.0);
    rows[1][0] = rows[0][1];
    ASSERT_EQ(stored[1][0], 1.25);
    ASSERT_EQ(rows.AllocatedBytes(), 6 * sizeof(float));
    rows[0][0] += 1.0;
    ASSERT_EQ(stored[0][0], 16777216.0);
    ASSERT_EQ(stored[0][2] * stored[0][2], static_cast<double>(0.1f) * static_cast<double>(0.1f));
}

TEST(LsmcStorageStudyTest, TestBinary64ControlAndMixedReadRowsPreserveValues) {
    StudyRows_<double> rows(1, 2);
    rows.ZeroPaths(0, 2);
    rows[0][0] = 16777217.0;
    rows[0][0] += 0.125;
    const auto& stored = rows;
    ASSERT_EQ(stored[0][0], 16777217.125);
    ASSERT_EQ(rows.AllocatedBytes(), 2 * sizeof(double));
    const double doubles[]{1.0, 1.0 / 3.0};
    const float singles[]{1.0f, 1.0f / 3.0f};
    ASSERT_EQ((StudyReadRow_(doubles) + 1)[0], doubles[1]);
    ASSERT_EQ((StudyReadRow_(singles) + 1)[0], static_cast<double>(singles[1]));
    ASSERT_FALSE(static_cast<bool>(StudyReadRow_{}));
}

TEST(LsmcStorageStudyTest, TestOverflowAndNonfiniteWritesFailBeforeConversion) {
    StudyRows_<float> rows(1, 2);
    rows.ZeroPaths(0, 2);
    const double limit = static_cast<double>(std::numeric_limits<float>::max());
    rows[0][0] = limit;
    ASSERT_THROW(rows[0][0] = std::nextafter(limit, std::numeric_limits<double>::infinity()), Dal::Exception_);
    ASSERT_THROW(rows[0][0] += limit, Dal::Exception_);
    ASSERT_THROW(rows[0][1] = std::numeric_limits<double>::quiet_NaN(), Dal::Exception_);
    ASSERT_THROW(rows[0][1] = std::numeric_limits<double>::infinity(), Dal::Exception_);
    ASSERT_THROW(rows[0][1] = -std::numeric_limits<double>::infinity(), Dal::Exception_);
    rows[0][1] = std::numeric_limits<double>::denorm_min();
    ASSERT_EQ(static_cast<const StudyRows_<float>&>(rows)[0][1], 0.0);
    StudyRows_<double> control(1, 1);
    ASSERT_THROW(control[0][0] = std::numeric_limits<double>::infinity(), Dal::Exception_);
}

TEST(LsmcStorageStudyTest, TestAllocationAndPathRangeBoundsAreChecked) {
    ASSERT_THROW(StudyRows_<float>(std::numeric_limits<size_t>::max(), 2), Dal::Exception_);
    StudyRows_<float> empty(0, 0);
    empty.ZeroPaths(0, 0);
    ASSERT_NO_THROW(empty.ZeroPaths(7, 3));
    ASSERT_EQ(empty.AllocatedBytes(), 0);
    StudyRows_<float> rows(1, 3);
    rows.ZeroPaths(0, 3);
    rows[0][0] = 1.0;
    rows[0][1] = 2.0;
    rows[0][2] = 3.0;
    rows.ZeroPaths(1, 1);
    const auto& stored = rows;
    ASSERT_EQ(stored[0][0], 1.0);
    ASSERT_EQ(stored[0][1], 0.0);
    ASSERT_EQ(stored[0][2], 3.0);
    ASSERT_THROW(rows.ZeroPaths(4, 0), Dal::Exception_);
    ASSERT_THROW(rows.ZeroPaths(2, 2), Dal::Exception_);
}

TEST(LsmcStorageStudyTest, TestLiveRowBytesSurviveOwnershipMoves) {
    const size_t before = Dal::Script::StudyCurrentRowBytes();
    {
        StudyRows_<float> training(3, 8);
        StudyRows_<double> validation(2, 4);
        ASSERT_EQ(Dal::Script::StudyCurrentRowBytes(), before + 3 * 8 * sizeof(float) + 2 * 4 * sizeof(double));
        auto moved = std::move(training);
        ASSERT_EQ(Dal::Script::StudyCurrentRowBytes(), before + moved.AllocatedBytes() + validation.AllocatedBytes());
        moved = StudyRows_<float>(1, 2);
        ASSERT_EQ(Dal::Script::StudyCurrentRowBytes(), before + 2 * sizeof(float) + validation.AllocatedBytes());
        ASSERT_GE(Dal::Script::StudyPeakRowBytes(), Dal::Script::StudyCurrentRowBytes());
    }
    ASSERT_EQ(Dal::Script::StudyCurrentRowBytes(), before);
}

TEST(LsmcStorageStudyTest, TestConcurrentRowsKeepIndependentValuesAndAccurateOwnership) {
    const size_t before = Dal::Script::StudyCurrentRowBytes();
    std::atomic<size_t> ready{0};
    std::atomic<bool> release{false};
    std::array<double, 4> values{};
    std::array<std::thread, 4> workers;
    for (size_t worker = 0; worker < workers.size(); ++worker) {
        workers[worker] = std::thread([&, worker] {
            StudyRows_<float> rows(2, 17);
            rows.ZeroPaths(0, 17);
            rows[0][0] = static_cast<double>(worker) + 0.25;
            values[worker] = static_cast<const StudyRows_<float>&>(rows)[0][0];
            ready.fetch_add(1, std::memory_order_release);
            while (!release.load(std::memory_order_acquire))
                std::this_thread::yield();
        });
    }
    while (ready.load(std::memory_order_acquire) != workers.size())
        std::this_thread::yield();
    const size_t live = Dal::Script::StudyCurrentRowBytes();
    release.store(true, std::memory_order_release);
    for (auto& worker : workers)
        worker.join();
    ASSERT_EQ(live, before + workers.size() * 2 * 17 * sizeof(float));
    ASSERT_EQ(Dal::Script::StudyCurrentRowBytes(), before);
    for (size_t worker = 0; worker < values.size(); ++worker)
        ASSERT_EQ(values[worker], static_cast<double>(worker) + 0.25);
}

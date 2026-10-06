//
// Created by Codex on 2026/10/06.
//

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <future>
#include <limits>
#include <new>
#include <vector>

#include <dal/math/buffercapacity.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/math/vectors.hpp>
#include <dal/platform/platform.hpp>
#include <dal/utilities/exceptions.hpp>

using namespace Dal;

namespace {
    struct ThrowingCopy_ {
        double value_ = 3.0;
        inline static bool reject_ = false;
        ThrowingCopy_() = default;
        ThrowingCopy_(const ThrowingCopy_& other) : value_(other.value_) { REQUIRE(!reject_, "controlled buffer element copy failure"); }
        ThrowingCopy_& operator=(const ThrowingCopy_&) = default;
    };
} // namespace

TEST(BufferCapacityTest, TestRejectsGrowthBeforeAllocationAndPreservesValues) {
    BufferCapacityBudget_ budget(71);
    BufferCapacityScope_ scope(&budget);
    Vector_<> values(4, 3.0);
    ASSERT_EQ(budget.CapacityBytes(), values.capacity() * sizeof(double));
    ASSERT_EQ(budget.CapacityBytes(), 32);
    const auto* storage = values.data();
    ASSERT_THROW(values.reserve(5), Exception_);
    ASSERT_EQ(values.data(), storage);
    ASSERT_EQ(values.capacity(), 4);
    ASSERT_EQ(budget.CapacityBytes(), 32);
    for (const auto value : values)
        ASSERT_DOUBLE_EQ(value, 3.0);
}

TEST(BufferCapacityTest, TestExactOverlapReleasesOldStorageAndOwnsNoContainerState) {
    static_assert(sizeof(Vector_<double>) == sizeof(std::vector<double>));
    static_assert(sizeof(Vector_<bool>) == sizeof(std::vector<bool>));
    static_assert(std::is_empty_v<Detail::BufferAllocator_<double>>);
    BufferCapacityBudget_ budget(72);
    {
        BufferCapacityScope_ scope(&budget);
        Vector_<> values(4, 3.0);
        values.reserve(5);
        ASSERT_EQ(values.capacity(), 5);
        ASSERT_EQ(budget.CapacityBytes(), 40);
        ASSERT_EQ(budget.PeakCapacityBytes(), 72);
        Vector_<> moved = std::move(values);
        Vector_<> swapped;
        moved.Swap(&swapped);
        ASSERT_TRUE(moved.empty());
        ASSERT_EQ(budget.CapacityBytes(), 40);
        ASSERT_EQ(swapped.size(), 4);
        ASSERT_DOUBLE_EQ(swapped.front(), 3.0);
    }
    ASSERT_EQ(budget.CapacityBytes(), 0);
    ASSERT_EQ(budget.PeakCapacityBytes(), 72);
}

TEST(BufferCapacityTest, TestCountsNestedMatricesPackedBooleanAndExcludesText) {
    BufferCapacityBudget_ budget(1024);
    {
        BufferCapacityScope_ scope(&budget);
        Vector_<Vector_<double>> nested(2);
        const auto structural = nested.capacity() * sizeof(Vector_<double>);
        ASSERT_EQ(budget.CapacityBytes(), structural);
        nested[0].Resize(3);
        nested[1].Resize(4);
        const auto nestedBytes = structural + (nested[0].capacity() + nested[1].capacity()) * sizeof(double);
        ASSERT_EQ(budget.CapacityBytes(), nestedBytes);
        Matrix_<> matrix(2, 3, 1.0);
        ASSERT_EQ(budget.CapacityBytes(), nestedBytes + 6 * sizeof(double));
        Vector_<bool> bits(128, true);
        const auto numericBytes = nestedBytes + 6 * sizeof(double) + bits.capacity() / 8;
        ASSERT_EQ(budget.CapacityBytes(), numericBytes);
        Vector_<String_> labels(3, "excluded text");
        Vector_<std::string> standardLabels(2, "excluded standard text");
        ASSERT_EQ(budget.CapacityBytes(), numericBytes);
    }
    ASSERT_EQ(budget.CapacityBytes(), 0);
}

TEST(BufferCapacityTest, TestAllocationAndElementFailureRefundReservations) {
    BufferCapacityBudget_ budget(72);
    BufferCapacityScope_ scope(&budget);
    Vector_<ThrowingCopy_> values(4);
    ThrowingCopy_::reject_ = true;
    try {
        values.reserve(5);
        ThrowingCopy_::reject_ = false;
        FAIL() << "controlled element copy must fail";
    } catch (const Exception_&) {
        ThrowingCopy_::reject_ = false;
    }
    ASSERT_EQ(values.capacity(), 4);
    ASSERT_EQ(budget.CapacityBytes(), 32);
    ASSERT_EQ(budget.PeakCapacityBytes(), 72);
    ASSERT_THROW(
        {
            Detail::BufferAllocationTicket_ ticket(&budget, 5, sizeof(double));
            throw std::bad_alloc();
        },
        std::bad_alloc);
    ASSERT_EQ(budget.CapacityBytes(), 32);
    ASSERT_THROW(Detail::BufferAllocationTicket_ ticket(&budget, std::numeric_limits<size_t>::max(), 2), Exception_);
    ASSERT_EQ(budget.CapacityBytes(), 32);
}

TEST(BufferCapacityTest, TestKnownFixedPayloadAndScopeOwnership) {
    BufferCapacityBudget_ budget(32);
    ASSERT_THROW(BufferCapacityScope_ invalid(nullptr), Exception_);
    ASSERT_THROW(BufferCapacityScope_ tooLarge(&budget, 33), Exception_);
    ASSERT_EQ(budget.CapacityBytes(), 0);
    BufferCapacityScope_ scope(&budget, 24);
    ASSERT_EQ(budget.CapacityBytes(), 24);
    ASSERT_THROW(BufferCapacityScope_ nested(&budget), Exception_);
    {
        Vector_<> one(1);
        ASSERT_EQ(budget.CapacityBytes(), 32);
        ASSERT_THROW(one.reserve(2), Exception_);
    }
    auto foreign = std::async(std::launch::async, [&scope] {
        try {
            scope.Close();
            return false;
        } catch (const Exception_&) {
            return true;
        }
    });
    ASSERT_TRUE(foreign.get());
    scope.Close();
    scope.Close();
    ASSERT_EQ(budget.CapacityBytes(), 0);
    Vector_<> unbudgeted(100);
    ASSERT_EQ(budget.CapacityBytes(), 0);
}

TEST(BufferCapacityTest, TestConcurrentWorkersShareOneLimitAndReleaseAfterDrain) {
    BufferCapacityBudget_ budget(64);
    std::promise<void> exit;
    const auto exitGate = exit.get_future().share();
    std::array<std::promise<void>, 2> ready;
    std::array<std::future<void>, 2> workers;
    for (size_t i = 0; i < workers.size(); ++i) {
        workers[i] = std::async(std::launch::async, [&, i] {
            BufferCapacityScope_ scope(&budget);
            Vector_<> values(4);
            ready[i].set_value();
            exitGate.wait();
        });
    }
    bool admitted = true;
    for (auto& signal : ready)
        admitted &= signal.get_future().wait_for(std::chrono::seconds(5)) == std::future_status::ready;
    const auto capacity = budget.CapacityBytes();
    bool rejected = false;
    {
        BufferCapacityScope_ coordinator(&budget);
        try {
            Vector_<> additional(1);
        } catch (const Exception_&) {
            rejected = true;
        }
    }
    exit.set_value();
    for (auto& worker : workers)
        worker.get();
    ASSERT_TRUE(admitted);
    ASSERT_TRUE(rejected);
    ASSERT_EQ(capacity, 64);
    ASSERT_EQ(budget.CapacityBytes(), 0);
    ASSERT_EQ(budget.PeakCapacityBytes(), 64);
}

TEST(BufferCapacityTest, TestCallerWorkerSharesCoordinatorBudgetAndClosesInOrder) {
    BufferCapacityBudget_ budget(32);
    BufferCapacityBudget_ foreign(32);
    BufferCapacityScope_ coordinator(&budget, 24);
    {
        auto worker = BufferCapacityScope_::ForWorker(&budget, 8);
        ASSERT_EQ(budget.CapacityBytes(), 32);
        ASSERT_THROW(coordinator.Close(), Exception_);
        ASSERT_THROW(static_cast<void>(BufferCapacityScope_::ForWorker(&foreign, 0)), Exception_);
        ASSERT_THROW(Vector_<> additional(1), Exception_);
        worker.Close();
        ASSERT_EQ(budget.CapacityBytes(), 24);
    }
    coordinator.Close();
    ASSERT_EQ(budget.CapacityBytes(), 0);
}

TEST(BufferCapacityTest, TestRecycledAddressIsReadmittedBeforeRetiredReservationIsReleased) {
    BufferCapacityBudget_ budget(2 * sizeof(double));
    BufferCapacityScope_ scope(&budget);
    double storage = 0.0;
    auto allocate = [&] { return Detail::AllocateBuffer(1, sizeof(double), [&] { return &storage; }, [](double*) {}); };
    auto* first = allocate();
    double* recycled = nullptr;
    size_t duringRelease = 0;
    bool admitted = false;
    Detail::DeallocateBuffer(first, [&](double*) {
        // A pool may recycle the freed address before its deallocation callback returns.
        try {
            recycled = allocate();
            admitted = true;
            duringRelease = budget.CapacityBytes();
        } catch (const Exception_&) {
        }
    });
    ASSERT_TRUE(admitted);
    ASSERT_EQ(recycled, first);
    ASSERT_EQ(duringRelease, 2 * sizeof(double));
    ASSERT_EQ(budget.CapacityBytes(), sizeof(double));
    Detail::DeallocateBuffer(recycled, [](double*) {});
    ASSERT_EQ(budget.CapacityBytes(), 0);
}

TEST(BufferCapacityTest, TestReservationRemainsChargedUntilPhysicalDeallocationCompletes) {
    BufferCapacityBudget_ budget(sizeof(double));
    BufferCapacityScope_ scope(&budget);
    std::array<double, 2> storage{};
    auto* first = Detail::AllocateBuffer(1, sizeof(double), [&] { return &storage[0]; }, [](double*) {});
    bool rejected = false;
    size_t duringRelease = 0;
    Detail::DeallocateBuffer(first, [&](double*) {
        duringRelease = budget.CapacityBytes();
        try {
            static_cast<void>(Detail::AllocateBuffer(1, sizeof(double), [&] { return &storage[1]; }, [](double*) {}));
        } catch (const Exception_&) {
            rejected = true;
        }
    });
    ASSERT_TRUE(rejected);
    ASSERT_EQ(duringRelease, sizeof(double));
    ASSERT_EQ(budget.CapacityBytes(), 0);
}

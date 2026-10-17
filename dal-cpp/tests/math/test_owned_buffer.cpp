//
// Created by Codex on 2026/10/7.
//

#include <gtest/gtest.h>

#include <dal/math/bufferallocation.hpp>
#include <dal/math/buffercapacity.hpp>
#include <dal/math/vectors.hpp>
#include <dal/utilities/exceptions.hpp>

namespace {
    class OwnedAccount_ final : public Dal::Detail::OwnedBufferAccount_ {
        size_t limit_;

    public:
        size_t current_ = 0;
        size_t peak_ = 0;
        explicit OwnedAccount_(size_t limit) : limit_(limit) {}
        void Reserve(size_t bytes) override {
            REQUIRE(bytes <= limit_ - current_, "Owned buffer limit exceeded");
            current_ += bytes;
            peak_ = std::max(peak_, current_);
        }
        void Release(size_t bytes) noexcept override { current_ -= bytes; }
    };

    struct ThrowingValue_ {
        bool* reject_ = nullptr;
        ThrowingValue_() = default;
        explicit ThrowingValue_(bool* reject) : reject_(reject) {}
        ThrowingValue_(const ThrowingValue_& other) : reject_(other.reject_) {
            REQUIRE(reject_ == nullptr || !*reject_, "Controlled owned element failure");
        }
        ThrowingValue_& operator=(const ThrowingValue_&) = default;
    };
} // namespace

TEST(OwnedBufferTest, TestActualVectorCapacityAndReplacementOverlapAreAccounted) {
    OwnedAccount_ account(1024);
    Dal::Detail::OwnedBufferScope_ scope(&account);
    {
        Dal::Vector_<double> values(4);
        ASSERT_EQ(account.current_, values.capacity() * sizeof(double));
        const auto original = account.current_;
        values.reserve(16);
        ASSERT_EQ(account.current_, values.capacity() * sizeof(double));
        ASSERT_EQ(account.peak_, original + values.capacity() * sizeof(double));
    }
    ASSERT_EQ(account.current_, 0);
}

TEST(OwnedBufferTest, TestRejectedAllocationPreservesExistingStorage) {
    OwnedAccount_ account(8 * sizeof(double));
    Dal::Detail::OwnedBufferScope_ scope(&account);
    {
        Dal::Vector_<double> values(4);
        ASSERT_THROW(values.reserve(8), Dal::Exception_);
        ASSERT_EQ(values.size(), 4);
        ASSERT_EQ(values.capacity(), 4);
        ASSERT_EQ(account.current_, 4 * sizeof(double));
    }
    ASSERT_EQ(account.current_, 0);
}

TEST(OwnedBufferTest, TestAlignedAndZeroCountAllocationsRetainTheirSemantics) {
    struct alignas(64) AlignedValue_ {
        double value_ = 7.0;
    };
    OwnedAccount_ account(1024);
    Dal::Detail::OwnedBufferScope_ scope(&account);
    {
        Dal::Detail::BufferAllocator_<double> allocator;
        auto* empty = allocator.allocate(0);
        ASSERT_EQ(account.current_, 0);
        allocator.deallocate(empty, 0);
        Dal::Vector_<AlignedValue_> values(2);
        ASSERT_EQ(reinterpret_cast<std::uintptr_t>(values.data()) % alignof(AlignedValue_), 0);
        ASSERT_EQ(account.current_, values.capacity() * sizeof(AlignedValue_));
        ASSERT_DOUBLE_EQ(values[0].value_, 7.0);
    }
    ASSERT_EQ(account.current_, 0);
}

TEST(OwnedBufferTest, TestGenericBudgetFailureRefundsTheOwnedReservation) {
    Dal::BufferCapacityBudget_ budget(sizeof(double));
    Dal::BufferCapacityScope_ buffers(&budget);
    OwnedAccount_ account(1024);
    Dal::Detail::OwnedBufferScope_ scope(&account);
    ASSERT_THROW(Dal::Vector_<double> rejected(8), Dal::Exception_);
    ASSERT_EQ(account.current_, 0);
    ASSERT_EQ(budget.CapacityBytes(), 0);
    {
        Dal::Vector_<double> value(1, 3.0);
        ASSERT_EQ(account.current_, sizeof(double));
        ASSERT_EQ(budget.CapacityBytes(), sizeof(double));
        ASSERT_DOUBLE_EQ(value[0], 3.0);
    }
    ASSERT_EQ(account.current_, 0);
    ASSERT_EQ(budget.CapacityBytes(), 0);
}

TEST(OwnedBufferTest, TestThrowingElementCopyReleasesNewStorage) {
    OwnedAccount_ account(1024);
    Dal::Detail::OwnedBufferScope_ scope(&account);
    bool reject = false;
    {
        Dal::Vector_<ThrowingValue_> values(4, ThrowingValue_(&reject));
        const auto initial = values.capacity() * sizeof(ThrowingValue_);
        reject = true;
        ASSERT_THROW(values.reserve(5), Dal::Exception_);
        reject = false;
        ASSERT_EQ(values.size(), 4);
        ASSERT_EQ(values.capacity(), 4);
        ASSERT_EQ(account.current_, initial);
    }
    ASSERT_EQ(account.current_, 0);
}

TEST(OwnedBufferTest, TestNullAndNestedAccountsRejectWithoutChangingOwnership) {
    OwnedAccount_ first(1024), second(1024);
    ASSERT_THROW(Dal::Detail::OwnedBufferScope_ invalid(nullptr), Dal::Exception_);
    Dal::Detail::OwnedBufferScope_ scope(&first);
    ASSERT_THROW(Dal::Detail::OwnedBufferScope_ nested(&second), Dal::Exception_);
    {
        Dal::Vector_<double> values(2);
        ASSERT_EQ(first.current_, 2 * sizeof(double));
        ASSERT_EQ(second.current_, 0);
    }
    ASSERT_EQ(first.current_, 0);
}

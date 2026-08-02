#include "bcg_allocation_probe.hpp"

#include <atomic>
#include <cstdlib>
#include <limits>
#include <new>

#if defined(_WIN32)
#include <malloc.h>
#endif

namespace {
    struct ProbeState_ {
        std::atomic<std::uint64_t> allocationRequests_{0};
        std::atomic<std::uint64_t> beginCalls_{0};
        std::atomic<std::uint64_t> endCalls_{0};
        std::atomic<unsigned> depth_{0};
        std::atomic<bool> failedClosed_{false};
    };

    ProbeState_ state_;
    Dal::BcgAllocationProbePrivate_::Snapshot_ lastSnapshot_{};

    void CountAllocationRequest_() noexcept {
        if (state_.depth_.load(std::memory_order_relaxed) == 1 && !state_.failedClosed_.load(std::memory_order_relaxed))
            state_.allocationRequests_.fetch_add(1, std::memory_order_relaxed);
    }

    std::size_t NonZeroSize_(std::size_t size) noexcept { return size == 0 ? 1 : size; }

    void* Allocate_(std::size_t size) {
        CountAllocationRequest_();
        if (void* result = std::malloc(NonZeroSize_(size)))
            return result;
        throw std::bad_alloc();
    }

    void* AllocateNoThrow_(std::size_t size) noexcept {
        try {
            return Allocate_(size);
        } catch (...) {
            return nullptr;
        }
    }

    void* AllocateAligned_(std::size_t size, std::size_t alignment) {
        CountAllocationRequest_();
#if defined(_WIN32)
        if (void* result = _aligned_malloc(NonZeroSize_(size), alignment))
            return result;
#else
        void* result = nullptr;
        if (posix_memalign(&result, alignment, NonZeroSize_(size)) == 0)
            return result;
#endif
        throw std::bad_alloc();
    }

    void* AllocateAlignedNoThrow_(std::size_t size, std::size_t alignment) noexcept {
        try {
            return AllocateAligned_(size, alignment);
        } catch (...) {
            return nullptr;
        }
    }

    void DeallocateAligned_(void* address) noexcept {
#if defined(_WIN32)
        _aligned_free(address);
#else
        std::free(address);
#endif
    }
} // namespace

namespace Dal {
    namespace BcgAllocationProbePrivate_ {
        void Reset_() noexcept {
            if (state_.depth_.load(std::memory_order_relaxed) != 0) {
                state_.failedClosed_.store(true, std::memory_order_relaxed);
                return;
            }
            state_.allocationRequests_.store(0, std::memory_order_relaxed);
            state_.beginCalls_.store(0, std::memory_order_relaxed);
            state_.endCalls_.store(0, std::memory_order_relaxed);
            state_.failedClosed_.store(false, std::memory_order_relaxed);
            lastSnapshot_ = {};
        }

        void Begin_() noexcept {
            state_.beginCalls_.fetch_add(1, std::memory_order_relaxed);
            unsigned expected = 0;
            if (!state_.depth_.compare_exchange_strong(expected, 1, std::memory_order_relaxed)) {
                state_.failedClosed_.store(true, std::memory_order_relaxed);
                state_.depth_.store(0, std::memory_order_relaxed);
            }
        }

        void End_() noexcept {
            state_.endCalls_.fetch_add(1, std::memory_order_relaxed);
            unsigned expected = 1;
            if (!state_.depth_.compare_exchange_strong(expected, 0, std::memory_order_relaxed)) {
                state_.failedClosed_.store(true, std::memory_order_relaxed);
                state_.depth_.store(0, std::memory_order_relaxed);
            }
        }

        Snapshot_ Read_() noexcept {
            const std::uint64_t begins = state_.beginCalls_.load(std::memory_order_relaxed);
            const std::uint64_t ends = state_.endCalls_.load(std::memory_order_relaxed);
            const bool failed = state_.failedClosed_.load(std::memory_order_relaxed);
            return {state_.allocationRequests_.load(std::memory_order_relaxed), begins, ends,
                    !failed && begins == ends && state_.depth_.load(std::memory_order_relaxed) == 0, failed};
        }

        Measurement_::Measurement_() noexcept : active_(true) { Begin_(); }

        Measurement_::~Measurement_() noexcept {
            if (active_) {
                End_();
                lastSnapshot_ = Read_();
            }
        }

        Snapshot_ Measurement_::Finish_() noexcept {
            if (active_) {
                End_();
                active_ = false;
                lastSnapshot_ = Read_();
            }
            return lastSnapshot_;
        }

        Snapshot_ LastSnapshotForTest_() noexcept { return lastSnapshot_; }
    } // namespace BcgAllocationProbePrivate_
} // namespace Dal

void* operator new(std::size_t size) { return Allocate_(size); }
void* operator new[](std::size_t size) { return Allocate_(size); }
void* operator new(std::size_t size, const std::nothrow_t&) noexcept { return AllocateNoThrow_(size); }
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept { return AllocateNoThrow_(size); }
void* operator new(std::size_t size, std::align_val_t alignment) { return AllocateAligned_(size, static_cast<std::size_t>(alignment)); }
void* operator new[](std::size_t size, std::align_val_t alignment) { return AllocateAligned_(size, static_cast<std::size_t>(alignment)); }
void* operator new(std::size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept {
    return AllocateAlignedNoThrow_(size, static_cast<std::size_t>(alignment));
}
void* operator new[](std::size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept {
    return AllocateAlignedNoThrow_(size, static_cast<std::size_t>(alignment));
}

void operator delete(void* address) noexcept { std::free(address); }
void operator delete[](void* address) noexcept { std::free(address); }
void operator delete(void* address, std::size_t) noexcept { std::free(address); }
void operator delete[](void* address, std::size_t) noexcept { std::free(address); }
void operator delete(void* address, const std::nothrow_t&) noexcept { std::free(address); }
void operator delete[](void* address, const std::nothrow_t&) noexcept { std::free(address); }
void operator delete(void* address, std::align_val_t) noexcept { DeallocateAligned_(address); }
void operator delete[](void* address, std::align_val_t) noexcept { DeallocateAligned_(address); }
void operator delete(void* address, std::size_t, std::align_val_t) noexcept { DeallocateAligned_(address); }
void operator delete[](void* address, std::size_t, std::align_val_t) noexcept { DeallocateAligned_(address); }
void operator delete(void* address, std::align_val_t, const std::nothrow_t&) noexcept { DeallocateAligned_(address); }
void operator delete[](void* address, std::align_val_t, const std::nothrow_t&) noexcept { DeallocateAligned_(address); }

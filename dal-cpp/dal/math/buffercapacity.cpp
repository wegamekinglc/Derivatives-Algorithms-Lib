//
// Created by Codex on 2026/10/06.
//

#include <algorithm>
#include <limits>
#include <map>
#include <mutex>
#include <string>
#include <thread>

#include <dal/math/bufferallocation.hpp>
#include <dal/math/buffercapacity.hpp>
#include <dal/platform/platform.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal {
    namespace {
        void RequireBufferCapacity(size_t current, size_t extra, size_t limit) {
            if (extra > limit - current) {
                const auto required =
                    extra <= std::numeric_limits<size_t>::max() - current ? std::to_string(current + extra) : std::string("above-size_t-range");
                THROW("Scratch buffer capacity budget exceeded [required=" + required + ", limit=" + std::to_string(limit) + "]");
            }
        }
    } // namespace

    struct BufferCapacityBudget_::Impl_ {
        const size_t limitBytes_;
        size_t capacityBytes_ = 0;
        size_t peakBytes_ = 0;
        size_t activeScopes_ = 0;
        std::mutex mutex_;
        std::map<std::uintptr_t, size_t> allocations_;

        explicit Impl_(size_t limit) : limitBytes_(limit) {}
    };

    struct BufferCapacityScope_::Attachment_ {
        BufferCapacityBudget_* budget_;
        const size_t fixedBytes_;
        const std::thread::id owner_ = std::this_thread::get_id();

        Attachment_(BufferCapacityBudget_* budget, size_t bytes) : budget_(budget), fixedBytes_(bytes) {}
    };

    BufferCapacityBudget_::BufferCapacityBudget_(size_t limitBytes) : impl_(std::make_unique<Impl_>(limitBytes)) {}

    BufferCapacityBudget_::~BufferCapacityBudget_() noexcept {
        if (impl_->activeScopes_ != 0)
            std::terminate();
    }

    size_t BufferCapacityBudget_::LimitBytes() const { return impl_->limitBytes_; }

    size_t BufferCapacityBudget_::CapacityBytes() const {
        const std::lock_guard<std::mutex> lock(impl_->mutex_);
        return impl_->capacityBytes_;
    }

    size_t BufferCapacityBudget_::PeakCapacityBytes() const {
        const std::lock_guard<std::mutex> lock(impl_->mutex_);
        return impl_->peakBytes_;
    }

    void BufferCapacityBudget_::Reserve(size_t bytes) {
        const std::lock_guard<std::mutex> lock(impl_->mutex_);
        RequireBufferCapacity(impl_->capacityBytes_, bytes, impl_->limitBytes_);
        impl_->capacityBytes_ += bytes;
        impl_->peakBytes_ = std::max(impl_->peakBytes_, impl_->capacityBytes_);
    }

    void BufferCapacityBudget_::Cancel(size_t bytes) noexcept {
        const std::lock_guard<std::mutex> lock(impl_->mutex_);
        if (bytes > impl_->capacityBytes_)
            std::terminate();
        impl_->capacityBytes_ -= bytes;
    }

    void BufferCapacityBudget_::Commit(const void* allocation, size_t bytes) {
        const std::lock_guard<std::mutex> lock(impl_->mutex_);
        REQUIRE(impl_->allocations_.emplace(reinterpret_cast<std::uintptr_t>(allocation), bytes).second,
                "Scratch buffer allocation: address is already admitted");
    }

    void BufferCapacityBudget_::Release(std::uintptr_t allocation) noexcept {
        const std::lock_guard<std::mutex> lock(impl_->mutex_);
        const auto found = impl_->allocations_.find(allocation);
        if (found != impl_->allocations_.end()) {
            impl_->capacityBytes_ -= found->second;
            impl_->allocations_.erase(found);
        }
    }

    BufferCapacityScope_::BufferCapacityScope_(BufferCapacityBudget_* budget, size_t fixedPayloadBytes) {
        REQUIRE(budget != nullptr, "Scratch buffer scope: budget must not be null");
        REQUIRE(Detail::CurrentBufferBudget() == nullptr, "Scratch buffer scope: nested scope is unsupported");
        auto attachment = std::make_unique<Attachment_>(budget, fixedPayloadBytes);
        budget->Reserve(fixedPayloadBytes);
        {
            const std::lock_guard<std::mutex> lock(budget->impl_->mutex_);
            ++budget->impl_->activeScopes_;
        }
        attachment_ = std::move(attachment);
        Detail::CurrentBufferBudget() = budget;
    }

    BufferCapacityScope_::~BufferCapacityScope_() noexcept { Close(); }

    void BufferCapacityScope_::Close() {
        if (!attachment_)
            return;
        REQUIRE(attachment_->owner_ == std::this_thread::get_id(), "Scratch buffer scope: close must run on the owning thread");
        auto* budget = attachment_->budget_;
        budget->Cancel(attachment_->fixedBytes_);
        {
            const std::lock_guard<std::mutex> lock(budget->impl_->mutex_);
            --budget->impl_->activeScopes_;
        }
        Detail::CurrentBufferBudget() = nullptr;
        attachment_.reset();
    }

    namespace Detail {
        BufferCapacityBudget_*& CurrentBufferBudget() noexcept {
            static thread_local BufferCapacityBudget_* budget = nullptr;
            return budget;
        }

        BufferAllocationTicket_::BufferAllocationTicket_(BufferCapacityBudget_* budget, size_t count, size_t elementBytes) : budget_(budget) {
            REQUIRE(elementBytes > 0 && count <= std::numeric_limits<size_t>::max() / elementBytes,
                    "Scratch buffer allocation: payload extent overflow");
            bytes_ = count * elementBytes;
            budget_->Reserve(bytes_);
        }

        BufferAllocationTicket_::~BufferAllocationTicket_() noexcept {
            if (budget_ != nullptr)
                budget_->Cancel(bytes_);
        }

        void BufferAllocationTicket_::Commit(const void* allocation) {
            budget_->Commit(allocation, bytes_);
            budget_ = nullptr;
        }

        void ReleaseBufferAllocation(std::uintptr_t allocation) noexcept { CurrentBufferBudget()->Release(allocation); }
    } // namespace Detail
} // namespace Dal

//
// Created by Codex on 2026/10/06.
//

#include <algorithm>
#include <array>
#include <limits>
#include <map>
#include <mutex>
#include <string>
#include <thread>

#include <dal/math/aad/blockallocation.hpp>
#include <dal/math/aad/recording.hpp>
#include <dal/math/aad/tapecapacity.hpp>
#include <dal/platform/platform.hpp>

namespace Dal::AAD {
    namespace {
        constexpr size_t TAPE_LISTS = 4;
        thread_local TapeCapacityScope_* activeScope = nullptr;

        size_t PayloadBytes(size_t blocks, size_t blockBytes) {
            REQUIRE(blocks <= std::numeric_limits<size_t>::max() / blockBytes, "Tape capacity: payload extent overflow");
            return blocks * blockBytes;
        }

        size_t TotalBytes(const std::array<size_t, TAPE_LISTS>& sizes) {
            size_t result = 0;
            for (const auto size : sizes) {
                REQUIRE(size <= std::numeric_limits<size_t>::max() - result, "Tape capacity: payload sum overflow");
                result += size;
            }
            return result;
        }

        void RequireCapacity(size_t current, size_t extra, size_t limit) {
            if (extra > limit - current) {
                const auto required =
                    extra <= std::numeric_limits<size_t>::max() - current ? std::to_string(current + extra) : std::string("above-size_t-range");
                THROW("Tape capacity budget exceeded [required=" + required + ", limit=" + std::to_string(limit) + "]");
            }
        }
    } // namespace

    struct TapeCapacityBudget_::Impl_ {
        const size_t limitBytes_;
        size_t capacityBytes_ = 0;
        size_t peakBytes_ = 0;
        size_t activeScopes_ = 0;
        std::mutex mutex_;
        std::map<const Tape_*, std::array<size_t, TAPE_LISTS>> tapes_;

        explicit Impl_(size_t limit) : limitBytes_(limit) {}
    };

    struct TapeCapacityScope_::Attachment_ {
        TapeCapacityBudget_::Impl_* budget_;
        std::array<size_t, TAPE_LISTS>* sizes_ = nullptr;
        std::array<const void*, TAPE_LISTS> lists_;
        std::thread::id owner_ = std::this_thread::get_id();

        Attachment_(TapeCapacityBudget_::Impl_* budget, Tape_* tape)
            : budget_(budget), lists_{&tape->nodes_, &tape->ders_, &tape->argPtrs_, &tape->adjointsMulti_} {}
    };

    TapeCapacityBudget_::TapeCapacityBudget_(size_t limitBytes) : impl_(std::make_unique<Impl_>(limitBytes)) {}

    TapeCapacityBudget_::~TapeCapacityBudget_() noexcept {
        if (impl_->activeScopes_ != 0)
            std::terminate();
    }

    size_t TapeCapacityBudget_::LimitBytes() const { return impl_->limitBytes_; }

    size_t TapeCapacityBudget_::CapacityBytes() const {
        const std::lock_guard<std::mutex> lock(impl_->mutex_);
        return impl_->capacityBytes_;
    }

    size_t TapeCapacityBudget_::PeakCapacityBytes() const {
        const std::lock_guard<std::mutex> lock(impl_->mutex_);
        return impl_->peakBytes_;
    }

    TapeCapacityScope_::TapeCapacityScope_(TapeCapacityBudget_* budget) {
        REQUIRE(budget != nullptr, "Tape capacity scope: budget must not be null");
        RequireRecordingModeChangeAllowed();
        REQUIRE(activeScope == nullptr, "Tape capacity scope: nested capacity scope is unsupported");
        auto* tape = Tape();
        auto attachment = std::make_unique<Attachment_>(budget->impl_.get(), tape);
        const std::array<size_t, TAPE_LISTS> sizes{PayloadBytes(tape->nodes_.AllocatedBlocks(), sizeof(std::array<TapNode_, BLOCK_SIZE>)),
                                                   PayloadBytes(tape->ders_.AllocatedBlocks(), sizeof(std::array<double, DATA_SIZE>)),
                                                   PayloadBytes(tape->argPtrs_.AllocatedBlocks(), sizeof(std::array<double*, DATA_SIZE>)),
                                                   PayloadBytes(tape->adjointsMulti_.AllocatedBlocks(), sizeof(std::array<double, ADJ_SIZE>))};
        const auto capacity = TotalBytes(sizes);
        auto& ledger = *attachment->budget_;
        {
            const std::lock_guard<std::mutex> lock(ledger.mutex_);
            const auto existing = ledger.tapes_.find(tape);
            const auto prior = existing == ledger.tapes_.end() ? 0 : TotalBytes(existing->second);
            const auto otherTapes = ledger.capacityBytes_ - prior;
            RequireCapacity(otherTapes, capacity, ledger.limitBytes_);
            auto& entry = ledger.tapes_[tape];
            entry = sizes;
            attachment->sizes_ = &entry;
            ledger.capacityBytes_ = otherTapes + capacity;
            ledger.peakBytes_ = std::max(ledger.peakBytes_, ledger.capacityBytes_);
            ++ledger.activeScopes_;
        }
        attachment_ = std::move(attachment);
        activeScope = this;
    }

    TapeCapacityScope_::~TapeCapacityScope_() noexcept { Close(); }

    void TapeCapacityScope_::Close() {
        if (!attachment_)
            return;
        REQUIRE(attachment_->owner_ == std::this_thread::get_id(), "Tape capacity scope: close must run on the owning thread");
        RequireRecordingModeChangeAllowed();
        auto& ledger = *attachment_->budget_;
        {
            const std::lock_guard<std::mutex> lock(ledger.mutex_);
            --ledger.activeScopes_;
        }
        activeScope = nullptr;
        attachment_.reset();
    }

    size_t TapeCapacityScope_::Slot(const void* list) const {
        const auto& lists = attachment_->lists_;
        return static_cast<size_t>(std::find(lists.begin(), lists.end(), list) - lists.begin());
    }

    void TapeCapacityScope_::Reserve(size_t slot, size_t bytes) {
        auto& ledger = *attachment_->budget_;
        const std::lock_guard<std::mutex> lock(ledger.mutex_);
        RequireCapacity(ledger.capacityBytes_, bytes, ledger.limitBytes_);
        (*attachment_->sizes_)[slot] += bytes;
        ledger.capacityBytes_ += bytes;
        ledger.peakBytes_ = std::max(ledger.peakBytes_, ledger.capacityBytes_);
    }

    void TapeCapacityScope_::Release(size_t slot, size_t bytes) noexcept {
        auto& ledger = *attachment_->budget_;
        const std::lock_guard<std::mutex> lock(ledger.mutex_);
        auto& capacity = (*attachment_->sizes_)[slot];
        if (bytes > capacity || bytes > ledger.capacityBytes_)
            std::terminate();
        capacity -= bytes;
        ledger.capacityBytes_ -= bytes;
    }

    BlockAllocationTicket_::BlockAllocationTicket_(const void* list, size_t bytes) {
        if (activeScope != nullptr) {
            const auto slot = activeScope->Slot(list);
            if (slot != TAPE_LISTS) {
                activeScope->Reserve(slot, bytes);
                scope_ = activeScope;
                slot_ = slot;
                bytes_ = bytes;
            }
        }
    }

    BlockAllocationTicket_::~BlockAllocationTicket_() noexcept {
        if (scope_ != nullptr)
            scope_->Release(slot_, bytes_);
    }

    void ReleaseBlockAllocation(const void* list, size_t bytes) noexcept {
        if (activeScope != nullptr) {
            const auto slot = activeScope->Slot(list);
            if (slot != TAPE_LISTS)
                activeScope->Release(slot, bytes);
        }
    }
} // namespace Dal::AAD

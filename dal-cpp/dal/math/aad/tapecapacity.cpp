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

        std::array<size_t, TAPE_LISTS> TapePayloads(const Tape_& tape) {
            return {PayloadBytes(tape.nodes_.AllocatedBlocks(), sizeof(std::array<TapNode_, BLOCK_SIZE>)),
                    PayloadBytes(tape.ders_.AllocatedBlocks(), sizeof(std::array<double, DATA_SIZE>)),
                    PayloadBytes(tape.argPtrs_.AllocatedBlocks(), sizeof(std::array<double*, DATA_SIZE>)),
                    PayloadBytes(tape.adjointsMulti_.AllocatedBlocks(), sizeof(std::array<double, ADJ_SIZE>))};
        }

        void RequireCapacity(size_t current, size_t extra, size_t limit) {
            if (current > limit || extra > limit - current) {
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
        size_t cleanupBytes_ = 0;
        std::mutex mutex_;
        std::map<const Tape_*, std::array<size_t, TAPE_LISTS>> tapes_;

        explicit Impl_(size_t limit) : limitBytes_(limit) {}
    };

    struct TapeCapacityScope_::Attachment_ {
        TapeCapacityBudget_::Impl_* budget_;
        std::array<size_t, TAPE_LISTS> initialSizes_{};
        std::array<size_t, TAPE_LISTS>* sizes_ = &initialSizes_;
        std::array<const void*, TAPE_LISTS> lists_{};
        size_t cleanupBytes_ = 0;
        std::thread::id owner_ = std::this_thread::get_id();

        explicit Attachment_(TapeCapacityBudget_::Impl_* budget) : budget_(budget) {}
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

    size_t TapeCleanupCapacityBytes() {
        return std::max({sizeof(std::array<TapNode_, BLOCK_SIZE>), sizeof(std::array<double, DATA_SIZE>), sizeof(std::array<double*, DATA_SIZE>),
                         sizeof(std::array<double, ADJ_SIZE>)});
    }

    TapeCapacityScope_::TapeCapacityScope_(TapeCapacityBudget_* budget) : TapeCapacityScope_(budget, false) {}

    TapeCapacityScope_::TapeCapacityScope_(TapeCapacityBudget_* budget, bool reserveCleanup) {
        REQUIRE(budget != nullptr, "Tape capacity scope: budget must not be null");
        RequireRecordingModeChangeAllowed();
        REQUIRE(activeScope == nullptr, "Tape capacity scope: nested capacity scope is unsupported");
        attachment_ = std::make_unique<Attachment_>(budget->impl_.get());
        try {
            ReserveCleanup(reserveCleanup);
            activeScope = this;
            AdmitTape();
        } catch (...) {
            CancelAdmission();
            throw;
        }
    }

    void TapeCapacityScope_::ReserveCleanup(bool reserveCleanup) {
        if (!reserveCleanup)
            return;
        auto& ledger = *attachment_->budget_;
        const std::lock_guard<std::mutex> lock(ledger.mutex_);
        const auto bytes = TapeCleanupCapacityBytes();
        RequireCapacity(ledger.capacityBytes_, ledger.cleanupBytes_, ledger.limitBytes_);
        RequireCapacity(ledger.capacityBytes_ + ledger.cleanupBytes_, bytes, ledger.limitBytes_);
        ledger.cleanupBytes_ += bytes;
        attachment_->cleanupBytes_ = bytes;
    }

    void TapeCapacityScope_::AdmitTape() {
        auto* tape = Tape();
        const auto sizes = TapePayloads(*tape);
        const auto capacity = TotalBytes(sizes);
        auto& ledger = *attachment_->budget_;
        {
            const std::lock_guard<std::mutex> lock(ledger.mutex_);
            const auto existing = ledger.tapes_.find(tape);
            const auto prior = existing == ledger.tapes_.end() ? 0 : TotalBytes(existing->second);
            const auto otherTapes = ledger.capacityBytes_ - prior - TotalBytes(attachment_->initialSizes_);
            RequireCapacity(otherTapes, capacity, ledger.limitBytes_ - ledger.cleanupBytes_);
            auto& entry = ledger.tapes_[tape];
            entry = sizes;
            attachment_->sizes_ = &entry;
            attachment_->lists_ = {&tape->nodes_, &tape->ders_, &tape->argPtrs_, &tape->adjointsMulti_};
            ledger.capacityBytes_ = otherTapes + capacity;
            ledger.peakBytes_ = std::max(ledger.peakBytes_, ledger.capacityBytes_);
            ++ledger.activeScopes_;
        }
    }

    void TapeCapacityScope_::CancelAdmission() noexcept {
        auto& ledger = *attachment_->budget_;
        {
            const std::lock_guard<std::mutex> lock(ledger.mutex_);
            for (const auto bytes : attachment_->initialSizes_)
                ledger.capacityBytes_ -= bytes;
            ledger.cleanupBytes_ -= attachment_->cleanupBytes_;
        }
        activeScope = nullptr;
        attachment_.reset();
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
            ledger.cleanupBytes_ -= attachment_->cleanupBytes_;
        }
        activeScope = nullptr;
        attachment_.reset();
    }

    size_t TapeCapacityScope_::Slot(const void* list) {
        auto& lists = attachment_->lists_;
        auto found = std::find(lists.begin(), lists.end(), list);
        if (found == lists.end() && attachment_->sizes_ == &attachment_->initialSizes_) {
            found = std::find(lists.begin(), lists.end(), nullptr);
            if (found != lists.end())
                *found = list;
        }
        return static_cast<size_t>(found - lists.begin());
    }

    void TapeCapacityScope_::Reserve(size_t slot, size_t bytes, bool replacement) {
        auto& ledger = *attachment_->budget_;
        const std::lock_guard<std::mutex> lock(ledger.mutex_);
        const bool cleanup = replacement && attachment_->cleanupBytes_ != 0;
        if (cleanup)
            REQUIRE(bytes <= attachment_->cleanupBytes_, "Tape capacity: replacement exceeds reserved cleanup payload");
        RequireCapacity(ledger.capacityBytes_, bytes, cleanup ? ledger.limitBytes_ : ledger.limitBytes_ - ledger.cleanupBytes_);
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

    BlockAllocationTicket_::BlockAllocationTicket_(const void* list, size_t bytes) : BlockAllocationTicket_(list, bytes, false) {}

    BlockAllocationTicket_::BlockAllocationTicket_(const void* list, size_t bytes, bool replacement) {
        if (activeScope != nullptr) {
            const auto slot = activeScope->Slot(list);
            if (slot != TAPE_LISTS) {
                activeScope->Reserve(slot, bytes, replacement);
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

    bool TapeCapacityActive() noexcept { return activeScope != nullptr; }

    void ReleaseBlockAllocation(const void* list, size_t bytes) noexcept {
        if (activeScope != nullptr) {
            const auto slot = activeScope->Slot(list);
            if (slot != TAPE_LISTS)
                activeScope->Release(slot, bytes);
        }
    }
} // namespace Dal::AAD

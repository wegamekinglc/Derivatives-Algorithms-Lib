//
// Created by Codex on 2026/10/06.
//

#pragma once

#include <cstddef>
#include <memory>

namespace Dal::AAD {
    class BlockAllocationTicket_;
    class TapeCapacityScope_;

    class TapeCapacityBudget_ {
        struct Impl_;
        std::unique_ptr<Impl_> impl_;
        friend class TapeCapacityScope_;

    public:
        explicit TapeCapacityBudget_(size_t limitBytes);
        ~TapeCapacityBudget_() noexcept;
        TapeCapacityBudget_(const TapeCapacityBudget_&) = delete;
        TapeCapacityBudget_& operator=(const TapeCapacityBudget_&) = delete;
        [[nodiscard]] size_t LimitBytes() const;
        [[nodiscard]] size_t CapacityBytes() const;
        [[nodiscard]] size_t PeakCapacityBytes() const;
    };

    // The request budget outlives all scopes; close recordings before closing their capacity scope.
    // Detached worker tapes remain charged until budget destruction or their next admission.
    class TapeCapacityScope_ {
        struct Attachment_;
        std::unique_ptr<Attachment_> attachment_;

        size_t Slot(const void* list) const;
        void Reserve(size_t slot, size_t bytes);
        void Release(size_t slot, size_t bytes) noexcept;
        friend class BlockAllocationTicket_;
        friend void ReleaseBlockAllocation(const void*, size_t) noexcept;

    public:
        explicit TapeCapacityScope_(TapeCapacityBudget_* budget);
        ~TapeCapacityScope_() noexcept;
        TapeCapacityScope_(const TapeCapacityScope_&) = delete;
        TapeCapacityScope_& operator=(const TapeCapacityScope_&) = delete;
        void Close();
    };
} // namespace Dal::AAD

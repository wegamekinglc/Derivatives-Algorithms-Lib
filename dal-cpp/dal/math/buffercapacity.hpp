//
// Created by Codex on 2026/10/06.
//

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

namespace Dal {
    namespace Detail {
        class BufferAllocationTicket_;
        void ReleaseBufferAllocation(std::uintptr_t) noexcept;
    } // namespace Detail

    // Buffer addresses differ from tape-list aggregates, so allocation ownership is tracked separately.
    class BufferCapacityBudget_ {
        struct Impl_;
        std::unique_ptr<Impl_> impl_;
        friend class BufferCapacityScope_;
        friend class Detail::BufferAllocationTicket_;
        friend void Detail::ReleaseBufferAllocation(std::uintptr_t) noexcept;

        void Reserve(size_t bytes);
        void Cancel(size_t bytes) noexcept;
        void Commit(const void* allocation, size_t bytes);
        void Release(std::uintptr_t allocation) noexcept;

    public:
        explicit BufferCapacityBudget_(size_t limitBytes);
        ~BufferCapacityBudget_() noexcept;
        BufferCapacityBudget_(const BufferCapacityBudget_&) = delete;
        BufferCapacityBudget_& operator=(const BufferCapacityBudget_&) = delete;
        [[nodiscard]] size_t LimitBytes() const;
        [[nodiscard]] size_t CapacityBytes() const;
        [[nodiscard]] size_t PeakCapacityBytes() const;
    };

    // Destroy request buffers inside an attached scope; the budget outlives all worker/coordinator scopes.
    class BufferCapacityScope_ {
        struct Attachment_;
        std::unique_ptr<Attachment_> attachment_;
        BufferCapacityScope_(BufferCapacityBudget_* budget, size_t fixedPayloadBytes, bool reuseAttachment);

    public:
        explicit BufferCapacityScope_(BufferCapacityBudget_* budget, size_t fixedPayloadBytes = 0);
        ~BufferCapacityScope_() noexcept;
        BufferCapacityScope_(const BufferCapacityScope_&) = delete;
        BufferCapacityScope_& operator=(const BufferCapacityScope_&) = delete;
        [[nodiscard]] static BufferCapacityScope_ ForWorker(BufferCapacityBudget_* budget, size_t fixedPayloadBytes = 0);
        void Close();
    };
} // namespace Dal

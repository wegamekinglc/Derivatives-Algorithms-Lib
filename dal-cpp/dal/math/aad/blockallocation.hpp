//
// Created by Codex on 2026/10/06.
//

#pragma once

#include <cstddef>

namespace Dal::AAD {
    class TapeCapacityScope_;

    // Reserves at the cold block-allocation boundary and refunds failed allocations.
    class BlockAllocationTicket_ {
        TapeCapacityScope_* scope_ = nullptr;
        size_t slot_ = 0;
        size_t bytes_ = 0;

    public:
        BlockAllocationTicket_(const void* list, size_t bytes);
        BlockAllocationTicket_(const void* list, size_t bytes, bool replacement);
        ~BlockAllocationTicket_() noexcept;
        BlockAllocationTicket_(const BlockAllocationTicket_&) = delete;
        BlockAllocationTicket_& operator=(const BlockAllocationTicket_&) = delete;
        void Commit() noexcept { scope_ = nullptr; }
    };

    void ReleaseBlockAllocation(const void* list, size_t bytes) noexcept;
    [[nodiscard]] bool TapeCapacityActive() noexcept;
} // namespace Dal::AAD

//
// Created by Codex on 2026/10/04.
//

#pragma once

#include <cstddef>

#include <dal/platform/host.hpp>

#if defined(DAL_ENABLE_AAD_PROFILING)
namespace Dal::AAD {
    void RecordBlockAllocation(size_t arrayBytes) noexcept;
    void CaptureTapeForProfiling();
} // namespace Dal::AAD
#else
namespace Dal::AAD {
    FORCE_INLINE void CaptureTapeForProfiling() noexcept {}
} // namespace Dal::AAD
#endif

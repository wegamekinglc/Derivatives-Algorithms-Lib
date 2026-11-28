#pragma once

#include <cstddef>

namespace TapePerfS0 {
    struct AllocationSnapshot_ {
        size_t live_, peak_, allocations_, frees_, reallocInPlace_, reallocMoved_, reallocFailed_, aligned_, alignedBytes_, largest_;
    };
} // namespace TapePerfS0

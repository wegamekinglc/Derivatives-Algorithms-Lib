#pragma once

#include <cstddef>
#include <cstdint>
#include <new>

namespace Dal {
    namespace BcgAllocationProbePrivate_ {
#if defined(_MSVC_STL_VERSION) && _ITERATOR_DEBUG_LEVEL != 0
        inline constexpr bool MSVC_CHECKED_ITERATORS = true;
#else
        inline constexpr bool MSVC_CHECKED_ITERATORS = false;
#endif

        struct Snapshot_ {
            std::uint64_t allocationRequests_;
            std::uint64_t beginCalls_;
            std::uint64_t endCalls_;
            bool balanced_;
            bool failedClosed_;
        };

        // These callbacks are a private test seam.  They are never installed in a
        // public header, library target, export set, or installed artifact.
        void Reset_() noexcept;
        void Begin_() noexcept;
        void End_() noexcept;
        Snapshot_ Read_() noexcept;
        void FailAfter_(std::size_t successfulRequests) noexcept;
        void CancelFailure_() noexcept;

        class Measurement_ {
            bool active_;

        public:
            Measurement_() noexcept;
            Measurement_(const Measurement_&) = delete;
            Measurement_& operator=(const Measurement_&) = delete;
            ~Measurement_() noexcept;
            Snapshot_ Finish_() noexcept;
        };

        Snapshot_ LastSnapshotForTest_() noexcept;
    } // namespace BcgAllocationProbePrivate_
} // namespace Dal

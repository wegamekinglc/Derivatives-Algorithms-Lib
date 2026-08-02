#pragma once

#include <cstddef>
#include <cstdint>
#include <new>

namespace Dal {
    namespace BcgAllocationProbePrivate_ {
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

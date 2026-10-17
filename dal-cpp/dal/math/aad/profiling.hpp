//
// Created by Codex on 2026/10/04.
//

#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <exception>
#include <list>
#include <optional>
#include <thread>
#include <vector>

#include <dal/math/aad/statistics.hpp>

/*IF--------------------------------------------------------------------------
enumeration AADProfilingPhase
    Explicit production diagnostic timing window
switchable
alternative PREPARE
alternative CALIBRATE
alternative WORKER_INIT
alternative PATH_FORWARD
alternative PAYOFF
alternative REVERSE
alternative REVERSE_SUFFIX
alternative REVERSE_PREFIX
alternative WAIT
alternative REDUCE
alternative SAMPLE_TAPE
alternative QUOTE_MAP
alternative LSM_TRAIN
alternative LSM_REGRESS
alternative LSM_REPLAY
-IF-------------------------------------------------------------------------*/

namespace Dal::AAD {
#include <dal/auto/MG_AADProfilingPhase_enum.hpp>

    [[nodiscard]] constexpr bool ProfilingAvailable() {
#if defined(DAL_ENABLE_AAD_PROFILING)
        return true;
#else
        return false;
#endif
    }

    struct ProfilingPhaseStatistics_ {
        std::uint64_t calls_ = 0;
        std::uint64_t wallNanoseconds_ = 0;
        std::uint64_t selfWallNanoseconds_ = 0;
        std::uint64_t cpuNanoseconds_ = 0;
        std::uint64_t cpuSamples_ = 0;
    };

    struct ProfilingArrayStatistics_ {
        std::uint64_t liveBytes_ = 0;
        std::uint64_t capacityBytes_ = 0;
        bool valid_ = true;
    };

    struct ProfilingMemoryStatistics_ {
        ProfilingArrayStatistics_ pathArrays_;
        ProfilingArrayStatistics_ workspaceArrays_;
        ProfilingArrayStatistics_ resultArrays_;
        ProfilingArrayStatistics_ regressionArrays_;
    };

    struct ProfilingData_ {
        std::array<ProfilingPhaseStatistics_, static_cast<size_t>(AADProfilingPhase_::Value_::_N_VALUES)> phases_{};
        std::list<std::vector<ProfilingData_>> taskGroups_;
        TapeStatistics_ highWater_;
        ProfilingMemoryStatistics_ memory_;
        std::thread::id thread_;
        std::uint64_t wallNanoseconds_ = 0;
        std::uint64_t cpuNanoseconds_ = 0;
        std::uint64_t selfCpuNanoseconds_ = 0;
        std::uint64_t tapeSamples_ = 0;
        std::uint64_t memorySamples_ = 0;
        std::uint64_t blockAllocations_ = 0;
        std::uint64_t allocatedArrayBytes_ = 0;
        bool cpuAvailable_ = false;
        bool selfCpuAvailable_ = false;
        bool complete_ = false;
        bool invalidMeasurement_ = false;

        [[nodiscard]] const ProfilingPhaseStatistics_& Phase(AADProfilingPhase_ phase) const {
            const auto index = static_cast<int>(phase.Switch());
            REQUIRE(index >= 0 && index < static_cast<int>(phases_.size()), "AADProfiling.Phase: invalid phase");
            return phases_[static_cast<size_t>(index)];
        }

    private:
        bool active_ = false;
        friend class ProfilingScope_;
    };

#if defined(DAL_ENABLE_AAD_PROFILING)
    [[nodiscard]] bool ProfilingActive() noexcept;
    void CaptureMemoryForProfiling(const ProfilingMemoryStatistics_& sample) noexcept;
    template <class F_> void CaptureMemoryForProfiling(const F_& measure) {
        if (ProfilingActive())
            CaptureMemoryForProfiling(measure());
    }
#else
    [[nodiscard]] constexpr bool ProfilingActive() noexcept { return false; }
    FORCE_INLINE void CaptureMemoryForProfiling(const ProfilingMemoryStatistics_&) noexcept {}
    template <class F_> FORCE_INLINE void CaptureMemoryForProfiling(const F_&) noexcept {}
#endif

    class ProfilingSpan_;

    class ProfilingScope_ {
#if defined(DAL_ENABLE_AAD_PROFILING)
        ProfilingData_* data_;
        ProfilingData_* previous_;
        ProfilingScope_* previousScope_;
        ProfilingData_* root_;
        ProfilingSpan_* previousSpan_;
        std::thread::id owner_;
        std::chrono::steady_clock::time_point wallStart_;
        std::uint64_t cpuStart_ = 0;
        std::uint64_t childCpu_ = 0;
        bool cpuAvailable_ = false;
        bool childrenCpuAvailable_ = true;
        int exceptions_;
        friend class ProfilingTaskSet_;
        friend class ProfilingTaskScope_;
#endif

    public:
        explicit ProfilingScope_(ProfilingData_* data);
        ~ProfilingScope_() noexcept;
        ProfilingScope_(const ProfilingScope_&) = delete;
        ProfilingScope_& operator=(const ProfilingScope_&) = delete;
        ProfilingScope_(ProfilingScope_&&) = delete;
        ProfilingScope_& operator=(ProfilingScope_&&) = delete;
        void CaptureTape(const Tape_& tape);
    };

    class ProfilingSpan_ {
#if defined(DAL_ENABLE_AAD_PROFILING)
        ProfilingData_* data_ = nullptr;
        ProfilingSpan_* parent_ = nullptr;
        std::chrono::steady_clock::time_point wallStart_;
        std::uint64_t cpuStart_ = 0;
        std::uint64_t childWall_ = 0;
        size_t phase_ = 0;
        bool cpuAvailable_ = false;
        friend class ProfilingScope_;
#endif

    public:
#if defined(DAL_ENABLE_AAD_PROFILING)
        explicit ProfilingSpan_(AADProfilingPhase_ phase);
        ~ProfilingSpan_() noexcept;
        void Finish() noexcept;
#else
        explicit FORCE_INLINE ProfilingSpan_(AADProfilingPhase_) noexcept {}
        explicit FORCE_INLINE ProfilingSpan_(AADProfilingPhase_::Value_) noexcept {}
        ~ProfilingSpan_() noexcept = default;
        FORCE_INLINE void Finish() noexcept {}
#endif
        ProfilingSpan_(const ProfilingSpan_&) = delete;
        ProfilingSpan_& operator=(const ProfilingSpan_&) = delete;
        ProfilingSpan_(ProfilingSpan_&&) = delete;
        ProfilingSpan_& operator=(ProfilingSpan_&&) = delete;
    };

    class ProfilingTaskSet_ {
#if defined(DAL_ENABLE_AAD_PROFILING)
        std::vector<ProfilingData_>* data_ = nullptr;
        ProfilingData_* root_ = nullptr;
        friend class ProfilingTaskScope_;
#endif

    public:
#if defined(DAL_ENABLE_AAD_PROFILING)
        explicit ProfilingTaskSet_(size_t count);
        [[nodiscard]] ProfilingData_* Data(size_t index) const;
#else
        explicit ProfilingTaskSet_(size_t) noexcept {}
#endif
        ProfilingTaskSet_(const ProfilingTaskSet_&) = delete;
        ProfilingTaskSet_& operator=(const ProfilingTaskSet_&) = delete;
        ProfilingTaskSet_(ProfilingTaskSet_&&) = delete;
        ProfilingTaskSet_& operator=(ProfilingTaskSet_&&) = delete;
    };

    class ProfilingTaskScope_ {
#if defined(DAL_ENABLE_AAD_PROFILING)
        std::optional<ProfilingScope_> scope_;
#endif

    public:
#if defined(DAL_ENABLE_AAD_PROFILING)
        ProfilingTaskScope_(const ProfilingTaskSet_& tasks, size_t index);
#else
        ProfilingTaskScope_(const ProfilingTaskSet_&, size_t) noexcept {}
#endif
        ProfilingTaskScope_(const ProfilingTaskScope_&) = delete;
        ProfilingTaskScope_& operator=(const ProfilingTaskScope_&) = delete;
        ProfilingTaskScope_(ProfilingTaskScope_&&) = delete;
        ProfilingTaskScope_& operator=(ProfilingTaskScope_&&) = delete;
    };
} // namespace Dal::AAD

//
// Created by Codex on 2026/10/04.
//

#include <dal/platform/platform.hpp>

#include <algorithm>
#include <limits>

#if defined(DAL_ENABLE_AAD_PROFILING)
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
// Windows' VOID macro conflicts with the generated StackInfoType_ enum.
#undef VOID
#else
#include <time.h>
#endif
#endif

#include <dal/math/aad/aad.hpp>
#include <dal/math/aad/profiling.hpp>

namespace Dal::AAD {
#include <dal/auto/MG_AADProfilingPhase_enum.inc>

#if defined(DAL_ENABLE_AAD_PROFILING)
    namespace {
        thread_local ProfilingData_* currentData = nullptr;
        thread_local ProfilingSpan_* currentSpan = nullptr;
        thread_local ProfilingScope_* currentScope = nullptr;

        struct CpuTime_ {
            std::uint64_t nanoseconds_ = 0;
            bool available_ = false;
        };

        CpuTime_ ThreadCpuTime() noexcept {
#if defined(_WIN32)
            FILETIME created{}, exited{}, kernel{}, user{};
            if (!GetThreadTimes(GetCurrentThread(), &created, &exited, &kernel, &user))
                return {};
            const auto ticks = [](const FILETIME& value) { return (static_cast<std::uint64_t>(value.dwHighDateTime) << 32) | value.dwLowDateTime; };
            return {(ticks(kernel) + ticks(user)) * 100, true};
#elif defined(CLOCK_THREAD_CPUTIME_ID)
            timespec time{};
            if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &time) != 0)
                return {};
            return {static_cast<std::uint64_t>(time.tv_sec) * 1000000000 + static_cast<std::uint64_t>(time.tv_nsec), true};
#else
            return {};
#endif
        }

        CpuTime_ ElapsedCpuTime(const CpuTime_& start) noexcept {
            const auto finish = ThreadCpuTime();
            if (!start.available_ || !finish.available_ || finish.nanoseconds_ < start.nanoseconds_)
                return {};
            return {finish.nanoseconds_ - start.nanoseconds_, true};
        }

        void AddMeasurement(std::uint64_t* total, std::uint64_t value, ProfilingData_* data) noexcept {
            if (value > std::numeric_limits<std::uint64_t>::max() - *total) {
                data->invalidMeasurement_ = true;
                return;
            }
            *total += value;
        }

        void RetainHighWater(TapeStatistics_* peak, const TapeStatistics_& sample) {
            peak->nodes_ = std::max(peak->nodes_, sample.nodes_);
            peak->edges_ = std::max(peak->edges_, sample.edges_);
            peak->blocks_ = std::max(peak->blocks_, sample.blocks_);
            peak->liveBytes_ = std::max(peak->liveBytes_, sample.liveBytes_);
            peak->occupiedBytes_ = std::max(peak->occupiedBytes_, sample.occupiedBytes_);
            peak->capacityBytes_ = std::max(peak->capacityBytes_, sample.capacityBytes_);
        }

        void RetainArrayMemory(ProfilingArrayStatistics_* peak, const ProfilingArrayStatistics_& sample, ProfilingData_* data) noexcept {
            if (!sample.valid_ || sample.liveBytes_ > sample.capacityBytes_) {
                peak->valid_ = false;
                data->invalidMeasurement_ = true;
                return;
            }
            peak->liveBytes_ = std::max(peak->liveBytes_, sample.liveBytes_);
            peak->capacityBytes_ = std::max(peak->capacityBytes_, sample.capacityBytes_);
        }

        void CaptureTapeSample(ProfilingData_* data, const Tape_& tape) {
            ProfilingSpan_ span(AADProfilingPhase_::Value_::SAMPLE_TAPE);
            RetainHighWater(&data->highWater_, MeasureTape(tape));
            AddMeasurement(&data->tapeSamples_, 1, data);
        }

        void RecordCpuSample(ProfilingPhaseStatistics_* statistics, std::uint64_t start, bool available, ProfilingData_* data) noexcept {
            const auto cpu = ElapsedCpuTime({start, available});
            if (cpu.available_) {
                AddMeasurement(&statistics->cpuNanoseconds_, cpu.nanoseconds_, data);
                AddMeasurement(&statistics->cpuSamples_, 1, data);
            }
        }

        void RecordScopeCpu(ProfilingData_* data, const CpuTime_& cpu, std::uint64_t childCpu, bool childrenAvailable) noexcept {
            data->cpuAvailable_ = cpu.available_;
            data->cpuNanoseconds_ = cpu.nanoseconds_;
            data->selfCpuAvailable_ = cpu.available_ && childrenAvailable;
            if (!data->selfCpuAvailable_)
                return;
            if (childCpu > cpu.nanoseconds_)
                data->invalidMeasurement_ = true;
            else
                data->selfCpuNanoseconds_ = cpu.nanoseconds_ - childCpu;
        }

        void RecordSpan(ProfilingData_* data, size_t phase, std::uint64_t elapsed, std::uint64_t childWall) noexcept {
            auto& statistics = data->phases_[phase];
            AddMeasurement(&statistics.calls_, 1, data);
            AddMeasurement(&statistics.wallNanoseconds_, elapsed, data);
            AddMeasurement(&statistics.selfWallNanoseconds_, elapsed - childWall, data);
        }

        bool TasksCompleted(const ProfilingData_& data) noexcept {
            return std::all_of(data.taskGroups_.begin(), data.taskGroups_.end(), [](const auto& group) {
                return std::all_of(group.begin(), group.end(), [](const auto& task) { return task.complete_; });
            });
        }
    } // namespace

    void RecordBlockAllocation(size_t arrayBytes) noexcept {
        if (currentData == nullptr)
            return;
        AddMeasurement(&currentData->blockAllocations_, 1, currentData);
        AddMeasurement(&currentData->allocatedArrayBytes_, arrayBytes, currentData);
    }

    bool ProfilingActive() noexcept { return currentData != nullptr; }

    void CaptureMemoryForProfiling(const ProfilingMemoryStatistics_& sample) noexcept {
        if (currentData == nullptr)
            return;
        auto& memory = currentData->memory_;
        RetainArrayMemory(&memory.pathArrays_, sample.pathArrays_, currentData);
        RetainArrayMemory(&memory.workspaceArrays_, sample.workspaceArrays_, currentData);
        RetainArrayMemory(&memory.resultArrays_, sample.resultArrays_, currentData);
        RetainArrayMemory(&memory.regressionArrays_, sample.regressionArrays_, currentData);
        AddMeasurement(&currentData->memorySamples_, 1, currentData);
    }

    void CaptureTapeForProfiling() {
        if (currentData != nullptr)
            CaptureTapeSample(currentData, *Tape());
    }

    ProfilingScope_::ProfilingScope_(ProfilingData_* data)
        : data_(data), previous_(currentData), previousScope_(currentScope), root_(data), previousSpan_(currentSpan),
          owner_(std::this_thread::get_id()), exceptions_(std::uncaught_exceptions()) {
        REQUIRE(data_ != nullptr, "AADProfiling.Scope: data must not be null");
        REQUIRE(!data_->active_, "AADProfiling.Scope: data is already active");
        *data_ = ProfilingData_{};
        data_->active_ = true;
        data_->thread_ = owner_;
        currentData = data_;
        currentSpan = nullptr;
        currentScope = this;
        const auto cpu = ThreadCpuTime();
        cpuStart_ = cpu.nanoseconds_;
        cpuAvailable_ = cpu.available_;
        wallStart_ = std::chrono::steady_clock::now();
    }

    ProfilingScope_::~ProfilingScope_() noexcept {
        // TLS scope identity also enforces the owning thread.
        if (currentScope != this || currentSpan != nullptr)
            std::terminate();
        const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - wallStart_).count();
        if (elapsed < 0)
            data_->invalidMeasurement_ = true;
        else
            data_->wallNanoseconds_ = static_cast<std::uint64_t>(elapsed);
        const auto cpu = ElapsedCpuTime({cpuStart_, cpuAvailable_});
        RecordScopeCpu(data_, cpu, childCpu_, childrenCpuAvailable_);
        // ActiveWait may execute another task of this request on the same thread.
        if (previousScope_ != nullptr && previousScope_->root_ == root_) {
            AddMeasurement(&previousScope_->childCpu_, cpu.nanoseconds_, previous_);
            previousScope_->childrenCpuAvailable_ &= data_->selfCpuAvailable_;
        }
        data_->complete_ = std::uncaught_exceptions() == exceptions_ && !data_->invalidMeasurement_ && TasksCompleted(*data_);
        data_->active_ = false;
        currentData = previous_;
        currentSpan = previousSpan_;
        currentScope = previousScope_;
    }

    void ProfilingScope_::CaptureTape(const Tape_& tape) {
        REQUIRE(owner_ == std::this_thread::get_id(), "AADProfiling.CaptureTape: requires owning thread");
        REQUIRE(currentData == data_, "AADProfiling.CaptureTape: requires the current profiling scope");
        REQUIRE(&tape == Tape(), "AADProfiling.CaptureTape: requires the current native tape");
        CaptureTapeSample(data_, tape);
    }

    ProfilingSpan_::ProfilingSpan_(AADProfilingPhase_ phase) : data_(currentData) {
        const auto index = static_cast<int>(phase.Switch());
        REQUIRE(index >= 0 && index < static_cast<int>(AADProfilingPhase_::Value_::_N_VALUES), "AADProfiling.Span: invalid phase");
        if (data_ == nullptr)
            return;
        phase_ = static_cast<size_t>(index);
        parent_ = currentSpan;
        const auto cpu = ThreadCpuTime();
        cpuStart_ = cpu.nanoseconds_;
        cpuAvailable_ = cpu.available_;
        wallStart_ = std::chrono::steady_clock::now();
        currentSpan = this;
    }

    ProfilingSpan_::~ProfilingSpan_() noexcept { Finish(); }

    void ProfilingSpan_::Finish() noexcept {
        if (data_ == nullptr)
            return;
        if (currentSpan != this || currentData != data_)
            std::terminate();
        const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - wallStart_).count();
        if (elapsed < 0 || static_cast<std::uint64_t>(elapsed) < childWall_) {
            data_->invalidMeasurement_ = true;
        } else {
            RecordSpan(data_, phase_, static_cast<std::uint64_t>(elapsed), childWall_);
            if (parent_ != nullptr)
                AddMeasurement(&parent_->childWall_, static_cast<std::uint64_t>(elapsed), data_);
            RecordCpuSample(&data_->phases_[phase_], cpuStart_, cpuAvailable_, data_);
        }
        currentSpan = parent_;
        data_ = nullptr;
    }

    ProfilingTaskSet_::ProfilingTaskSet_(size_t count) {
        if (currentData == nullptr)
            return;
        currentData->taskGroups_.emplace_back(count);
        data_ = &currentData->taskGroups_.back();
        root_ = currentScope->root_;
    }

    ProfilingData_* ProfilingTaskSet_::Data(size_t index) const {
        if (data_ == nullptr)
            return nullptr;
        REQUIRE(index < data_->size(), "AADProfiling.TaskSet: task index is out of range");
        return &(*data_)[index];
    }

    ProfilingTaskScope_::ProfilingTaskScope_(const ProfilingTaskSet_& tasks, size_t index) {
        if (auto* data = tasks.Data(index)) {
            scope_.emplace(data);
            scope_->root_ = tasks.root_;
        }
    }
#else
    ProfilingScope_::ProfilingScope_(ProfilingData_*) { THROW("AADProfiling.Scope: profiling is not available in this build"); }
    ProfilingScope_::~ProfilingScope_() noexcept = default;
    void ProfilingScope_::CaptureTape(const Tape_&) { THROW("AADProfiling.CaptureTape: profiling is not available in this build"); }
#endif
} // namespace Dal::AAD

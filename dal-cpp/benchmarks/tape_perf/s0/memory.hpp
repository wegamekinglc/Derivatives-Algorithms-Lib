#pragma once

#include <exception>
#include <fstream>
#include <iostream>
#include <optional>
#include <thread>

#include <dal/platform/platform.hpp>

#include "ledger.hpp"

#if defined(__linux__)
#include <dlfcn.h>
#endif

namespace TapePerfS0 {
    struct MemoryRequest_ {
        bool xad_ = false, hot_ = false;
    };

    struct RiskMemory_ {
        size_t logical_ = 0;
        std::optional<size_t> cleanup_;
        const char* logicalKind_ = nullptr;
        std::optional<size_t> tapePeak_, retained_;
    };

#if defined(__linux__)
    inline size_t PeakRssKiB() {
        std::ifstream status("/proc/self/status");
        for (std::string line; std::getline(status, line);)
            if (line.rfind("VmHWM:", 0) == 0)
                return std::stoul(line.substr(6));
        THROW("S0 memory could not read process VmHWM");
    }

    class Ledger_ {
        using Action_ = void (*)();
        using Snapshot_ = void (*)(AllocationSnapshot_*);
        Action_ start_ = reinterpret_cast<Action_>(dlsym(RTLD_DEFAULT, "dal_s0_ledger_start"));
        Action_ reset_ = reinterpret_cast<Action_>(dlsym(RTLD_DEFAULT, "dal_s0_ledger_reset_peak"));
        Snapshot_ snapshot_ = reinterpret_cast<Snapshot_>(dlsym(RTLD_DEFAULT, "dal_s0_ledger_snapshot"));

    public:
        Ledger_() { REQUIRE(start_ && reset_ && snapshot_, "S0 memory requires the Linux allocation ledger via LD_PRELOAD"); }
        void Start() const { start_(); }
        void ResetPeak() const { reset_(); }
        [[nodiscard]] AllocationSnapshot_ Snapshot() const {
            AllocationSnapshot_ value{};
            snapshot_(&value);
            return value;
        }
    };

    template <class F_> void MeasureMemory(const char* name, int directions, MemoryRequest_ request, const F_& withBackend) {
        const Ledger_ ledger;
        AllocationSnapshot_ created{}, before{}, after{};
        size_t resultLive = 0, afterBackend = 0;
        RiskMemory_ risk;
        std::exception_ptr failure;
        std::thread worker([&] {
            try {
                ledger.Start();
                withBackend([&](const auto& evaluate) {
                    created = ledger.Snapshot();
                    if (request.hot_) {
                        for (int i = 0; i < 3; ++i)
                            (void)evaluate([] {});
                        ledger.ResetPeak();
                    }
                    before = ledger.Snapshot();
                    risk = evaluate([&] { resultLive = ledger.Snapshot().live_; });
                    after = ledger.Snapshot();
                });
                afterBackend = ledger.Snapshot().live_;
            } catch (...) {
                failure = std::current_exception();
            }
        });
        worker.join();
        if (failure)
            std::rethrow_exception(failure);
        const auto end = ledger.Snapshot();
        const auto rss = PeakRssKiB();
        std::cout << "# "
                     "s0_memory,case,directions,route,state,construct_live,construct_aligned_bytes,pre_request_live,peak,result_live,"
                     "post_result_live,post_backend_live,"
                     "post_thread_live,largest_request,aligned_allocs,allocs,frees,realloc_in_place,realloc_moved,realloc_failed,"
                     "cleanup_reserve,logical_bytes,logical_kind,maxrss_kib,tape_peak,retained_tape\n"
                  << "s0_memory," << name << ',' << directions << ',' << (request.xad_ ? "xad" : "dal") << ',' << (request.hot_ ? "hot" : "cold")
                  << ',' << created.live_ << ',' << created.alignedBytes_ << ',' << before.live_ << ',' << after.peak_ << ',' << resultLive << ','
                  << after.live_ << ',' << afterBackend << ',' << end.live_ << ',' << after.largest_ << ',' << after.aligned_ << ','
                  << end.allocations_ << ',' << end.frees_ << ',' << end.reallocInPlace_ << ',' << end.reallocMoved_ << ',' << end.reallocFailed_
                  << ',' << (risk.cleanup_ ? std::to_string(*risk.cleanup_) : "unavailable") << ',' << risk.logical_ << ',' << risk.logicalKind_
                  << ',' << rss << ',' << (risk.tapePeak_ ? std::to_string(*risk.tapePeak_) : "unavailable") << ','
                  << (risk.retained_ ? std::to_string(*risk.retained_) : "unavailable") << '\n';
        REQUIRE(resultLive > after.live_, "S0 memory ledger did not observe the owning result release");
        REQUIRE(end.live_ == 0 && end.allocations_ == end.frees_, "S0 memory ledger did not close after backend and thread destruction");
    }
#else
    template <class F_> void MeasureMemory(const char*, int, MemoryRequest_, const F_&) { THROW("S0 memory ledger requires Linux/glibc"); }
#endif
} // namespace TapePerfS0

//
// Created by dal-implementer on 2026-6-28.
//

#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#ifdef _MSC_VER
#include <intrin.h>
#endif

namespace Dal::Bench {
    // First-column width: comfortably fits the longest case name (~60 chars) with headroom.
    constexpr int kNameColumnWidth = 75;

    // Anti-dead-code-elimination sink. Forces the compiler to materialize
    // side effects of the benchmark body so work is not optimized away.
    inline void DoNotOptimize(const void* p) {
        static std::atomic<const void*> sink;
        sink.store(p, std::memory_order_relaxed);
    }

    struct Result_ {
        std::string name;
        int64_t medianNs;
        int64_t minNs;
        int64_t maxNs;
        int repeats;
    };

    // Run body() warmup times (discarded), then repeats times (timed).
    // Returns median/min/max wall-clock nanoseconds per iteration.
    // The clock is steady_clock, never high_resolution_clock: on platforms where the
    // high-resolution clock is not monotonic a captured clock regression yields negative
    // intervals, which the regression gate's row parser then silently drops as unparseable —
    // surfacing later as a bogus "incomplete samples" failure.
    template <class Body_>
    Result_ Run(const std::string& name, Body_&& body, int warmup = 3, int repeats = 10, int innerLoops = 1) {
        for (int i = 0; i < warmup; ++i)
            body();

        std::vector<int64_t> samples;
        samples.reserve(static_cast<size_t>(repeats));
        for (int i = 0; i < repeats; ++i) {
            const auto t0 = std::chrono::steady_clock::now();
            for (int inner = 0; inner < innerLoops; ++inner)
                body();
            const auto t1 = std::chrono::steady_clock::now();
            // innerLoops > 1 amortizes scheduler transients for bodies well under ~50us: report
            // the per-iteration interval so the printed row keeps its per-call meaning.
            samples.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count() / innerLoops);
        }

        std::sort(samples.begin(), samples.end());
        const int64_t median = samples[static_cast<size_t>(repeats) / 2];
        const int64_t minVal = samples.front();
        const int64_t maxVal = samples.back();
        return Result_{name, median, minVal, maxVal, repeats};
    }

    inline void PrintHeader() {
        std::cout << std::setw(kNameColumnWidth) << std::left << "Benchmark"
                  << std::setw(18) << std::right << "Median"
                  << std::setw(18) << std::right << "Min"
                  << std::setw(18) << std::right << "Max"
                  << std::setw(10) << std::right << "Reps"
                  << std::endl;
        std::cout << std::string(kNameColumnWidth + 18 * 3 + 10, '-') << std::endl;
    }

    inline std::string FormatScaled(int64_t ns) {
        const double value = static_cast<double>(ns);
        char buf[64];
        if (ns >= 1000000)
            std::snprintf(buf, sizeof(buf), "%.3f ms", value / 1.0e6);
        else if (ns >= 1000)
            std::snprintf(buf, sizeof(buf), "%.3f us", value / 1.0e3);
        else
            std::snprintf(buf, sizeof(buf), "%lld ns", static_cast<long long>(ns));
        return std::string(buf);
    }

    inline void Print(const Result_& r) {
        std::cout << std::setw(kNameColumnWidth) << std::left << r.name
                  << std::setw(18) << std::right << FormatScaled(r.medianNs)
                  << std::setw(18) << std::right << FormatScaled(r.minNs)
                  << std::setw(18) << std::right << FormatScaled(r.maxNs)
                  << std::setw(10) << std::right << r.repeats
                  << std::endl;
    }
} // namespace Dal::Bench

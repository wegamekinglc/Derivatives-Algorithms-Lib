//
// Created by dal-implementer on 2026-6-28.
//

#pragma once

#include <algorithm>
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
    // Anti-dead-code-elimination sink. Forces the compiler to materialize
    // side effects of the benchmark body so work is not optimized away.
    inline void DoNotOptimize(const void* p) {
#ifdef _MSC_VER
        _ReadWriteBarrier();
        (void)p;
#elif defined(__GNUC__)
        asm volatile("" : : "r"(p) : "memory");
#else
        volatile const void* sink = p;
        (void)sink;
#endif
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
    template <class Body_>
    Result_ Run(const std::string& name, Body_&& body, int warmup = 3, int repeats = 10) {
        for (int i = 0; i < warmup; ++i)
            body();

        std::vector<int64_t> samples;
        samples.reserve(static_cast<size_t>(repeats));
        for (int i = 0; i < repeats; ++i) {
            const auto t0 = std::chrono::high_resolution_clock::now();
            body();
            const auto t1 = std::chrono::high_resolution_clock::now();
            samples.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
        }

        std::sort(samples.begin(), samples.end());
        const int64_t median = samples[static_cast<size_t>(repeats) / 2];
        const int64_t minVal = samples.front();
        const int64_t maxVal = samples.back();
        return Result_{name, median, minVal, maxVal, repeats};
    }

    inline void PrintHeader() {
        std::cout << std::setw(40) << std::left << "Benchmark"
                  << std::setw(14) << std::right << "Median"
                  << std::setw(14) << std::right << "Min"
                  << std::setw(14) << std::right << "Max"
                  << std::setw(8) << std::right << "Reps"
                  << std::endl;
        std::cout << std::string(90, '-') << std::endl;
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
        std::cout << std::setw(40) << std::left << r.name
                  << std::setw(14) << std::right << FormatScaled(r.medianNs)
                  << std::setw(14) << std::right << FormatScaled(r.minNs)
                  << std::setw(14) << std::right << FormatScaled(r.maxNs)
                  << std::setw(8) << std::right << r.repeats
                  << std::endl;
    }
} // namespace Dal::Bench

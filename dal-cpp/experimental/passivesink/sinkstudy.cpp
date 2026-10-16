//
// Created by Codex on 2026/10/16.
//

#include <algorithm>
#include <chrono>
#include <cmath>
#include <dal/benchmarks/bench.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/math/aad/recording.hpp>
#include <dal/math/aad/statistics.hpp>
#include <dal/platform/platform.hpp>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>

using namespace Dal;
using namespace Dal::AAD;

namespace {
    constexpr int STEPS = 50000;
    constexpr int SAMPLES = 20;

    template <bool P_> Number_ Chain(const Number_& seed) {
        Number_ value = seed;
        for (int i = 0; i < STEPS; ++i) {
            if constexpr (P_)
                value = value * 1.0001 + 0.0001;
            else
                value = value * Number_(1.0001) + Number_(0.0001);
        }
        return value;
    }

    template <class F_> long long Minimum(const F_& action) {
        long long result = std::numeric_limits<long long>::max();
        for (int i = 0; i < SAMPLES + 3; ++i) {
            const auto start = std::chrono::steady_clock::now();
            action();
            const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
            if (i >= 3)
                result = std::min(result, static_cast<long long>(elapsed));
        }
        return result;
    }

    template <bool P_> void Run(size_t width) {
        const auto mode = SetNumResultsForAAD(width > 1, width);
        const auto recording = Minimum([] {
            RecordingScope_ scope;
            Number_ seed;
            scope.RegisterInput(seed, 1.0);
            scope.StartRecording();
            const Number_ top = Chain<P_>(seed);
            const double value = Value(top);
            Bench::DoNotOptimize(&value);
        });
        RecordingScope_ scope;
        Number_ seed;
        scope.RegisterInput(seed, 1.0);
        scope.StartRecording();
        Number_ top = Chain<P_>(seed);
        scope.FinishRecording();
        const auto statistics = MeasureTape(*Tape());
        const auto reverse = Minimum([&] {
            scope.ClearAdjoints();
            for (size_t lane = 0; lane < width; ++lane)
                NativeOperations_::SetSeed(top, 1.0, lane);
            scope.Reverse();
        });
        const double expected = std::pow(1.0001, STEPS);
        for (size_t lane = 0; lane < width; ++lane)
            REQUIRE(std::abs(NativeOperations_::ReadAdjoint(seed, lane) - expected) < 1e-8, "chain gradient mismatch");
        REQUIRE(std::abs(Value(top) - (2.0 * expected - 1.0)) < 1e-8, "chain value mismatch");
        std::cout << std::setprecision(17) << "{\"record_ns\":" << recording << ",\"reverse_ns\":" << reverse << ",\"nodes\":" << statistics.nodes_
                  << ",\"edges\":" << statistics.edges_ << ",\"occupied_bytes\":" << statistics.occupiedBytes_
                  << ",\"capacity_bytes\":" << statistics.capacityBytes_ << ",\"number_bytes\":" << sizeof(Number_) << ",\"width\":" << width
                  << ",\"value\":" << Value(top) << ",\"gradient\":" << NativeOperations_::ReadAdjoint(seed) << "}\n";
    }
} // namespace

int main(int argc, char** argv) {
    try {
        REQUIRE(argc == 3, "usage: passive_sink_study number|double 1|4");
        const std::string kind(argv[1]);
        const std::string widthText(argv[2]);
        REQUIRE(widthText == "1" || widthText == "4", "width must be 1 or 4");
        const size_t width = widthText == "1" ? 1 : 4;
        if (kind == "number")
            Run<false>(width);
        else {
            REQUIRE(kind == "double", "kind must be number or double");
            Run<true>(width);
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

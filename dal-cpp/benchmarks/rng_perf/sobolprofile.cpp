//
// Created by Codex on 2026/10/15.
//

#include <charconv>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <limits>
#include <memory>
#include <string>

#include <dal/benchmarks/bench.hpp>
#include <dal/math/random/sobol.hpp>
#include <dal/platform/platform.hpp>

#include "sobolprofile.hpp"

using namespace Dal;

namespace {
    size_t PositiveCount(const char* text) {
        size_t value = 0;
        const char* end = text + std::strlen(text);
        const auto parsed = std::from_chars(text, end, value);
        REQUIRE(parsed.ec == std::errc{} && parsed.ptr == end && value > 0, "expected a positive integer");
        return value;
    }

    void Fill(SequenceSet_* generator, Vector_<>* values, bool uniform) {
        if (uniform)
            generator->FillUniform(values);
        else
            generator->FillNormal(values);
    }
} // namespace

int RunSobolProfile(int argc, char** argv) {
    if (argc != 6 || std::string(argv[1]) != "--sobol-profile") {
        std::cerr << "usage: rng_perf --sobol-profile DIM PATHS uniform|fast|precise REPEATS\n";
        return 2;
    }
    try {
        const auto dimension = PositiveCount(argv[2]), paths = PositiveCount(argv[3]), repeats = PositiveCount(argv[5]);
        REQUIRE(dimension <= static_cast<size_t>(std::numeric_limits<int>::max()), "dimension exceeds integer range");
        const std::string precision = argv[4];
        REQUIRE(precision == "uniform" || precision == "fast" || precision == "precise", "invalid Sobol precision");
        const bool uniform = precision == "uniform", precise = precision == "precise";
        const auto makeGenerator = [&] { return std::unique_ptr<SequenceSet_>(NewSobol(static_cast<int>(dimension), 0, precise, precise)); };
        Vector_<> values(dimension);
        auto reference = makeGenerator();
        std::uint64_t fingerprint = 14695981039346656037ULL;
        for (size_t path = 0; path < paths; ++path) {
            Fill(reference.get(), &values, uniform);
            for (const auto value : values) {
                REQUIRE(std::isfinite(value), "non-finite Sobol value");
                std::uint64_t bits;
                static_assert(sizeof(bits) == sizeof(value));
                std::memcpy(&bits, &value, sizeof(bits));
                fingerprint = (fingerprint ^ bits) * 1099511628211ULL;
            }
        }
        double sink = 0.0;
        for (size_t sample = 0; sample < repeats; ++sample) {
            auto generator = makeGenerator();
            const auto begin = std::chrono::steady_clock::now();
            for (size_t path = 0; path < paths; ++path) {
                Fill(generator.get(), &values, uniform);
                sink += values[0];
            }
            const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - begin).count();
            std::cout << "{\"record\":\"sobol\",\"sample\":" << sample << ",\"dimensions\":" << dimension << ",\"paths\":" << paths
                      << ",\"precision\":\"" << precision << "\",\"wall_ns\":" << elapsed << ",\"fingerprint\":\"" << std::hex << fingerprint
                      << std::dec << "\"}\n";
        }
        Bench::DoNotOptimize(&sink);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

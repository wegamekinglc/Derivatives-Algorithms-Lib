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
#include <string_view>

#include <dal/benchmarks/bench.hpp>
#include <dal/math/random/sobol.hpp>
#include <dal/platform/platform.hpp>

#include "sobolprofile.hpp"

using namespace Dal;

namespace {
    struct SobolOptions_ {
        size_t dimension_, paths_, repeats_;
        std::string precision_;
    };

    size_t PositiveCount(std::string_view text) {
        size_t value = 0;
        const char* end = text.data() + text.size();
        const auto parsed = std::from_chars(text.data(), end, value);
        REQUIRE(parsed.ec == std::errc{} && parsed.ptr == end && value > 0, "expected a positive integer");
        return value;
    }

    SobolOptions_ ParseOptions(char** argv) {
        const auto dimension = PositiveCount(argv[2]), paths = PositiveCount(argv[3]), repeats = PositiveCount(argv[5]);
        REQUIRE(dimension <= static_cast<size_t>(std::numeric_limits<int>::max()), "dimension exceeds integer range");
        const std::string precision = argv[4];
        REQUIRE(precision == "uniform" || precision == "fast" || precision == "precise", "invalid Sobol precision");
        return {dimension, paths, repeats, precision};
    }

    std::unique_ptr<SequenceSet_> MakeGenerator(const SobolOptions_& options) {
        const bool precise = options.precision_ == "precise";
        return std::unique_ptr<SequenceSet_>(NewSobol(static_cast<int>(options.dimension_), 0, precise, precise));
    }

    void Fill(SequenceSet_* generator, Vector_<>* values, bool uniform) {
        if (uniform)
            generator->FillUniform(values);
        else
            generator->FillNormal(values);
    }
    std::uint64_t Fingerprint(const SobolOptions_& options, SequenceSet_* reference, Vector_<>* values) {
        const bool uniform = options.precision_ == "uniform";
        std::uint64_t fingerprint = 14695981039346656037ULL;
        for (size_t path = 0; path < options.paths_; ++path) {
            Fill(reference, values, uniform);
            for (const auto value : *values) {
                REQUIRE(std::isfinite(value), "non-finite Sobol value");
                std::uint64_t bits;
                static_assert(sizeof(bits) == sizeof(value));
                std::memcpy(&bits, &value, sizeof(bits));
                fingerprint = (fingerprint ^ bits) * 1099511628211ULL;
            }
        }
        return fingerprint;
    }

    void RunSamples(const SobolOptions_& options, Vector_<>* values, std::uint64_t fingerprint) {
        const bool uniform = options.precision_ == "uniform";
        double sink = 0.0;
        for (size_t sample = 0; sample < options.repeats_; ++sample) {
            auto generator = MakeGenerator(options);
            const auto begin = std::chrono::steady_clock::now();
            for (size_t path = 0; path < options.paths_; ++path) {
                Fill(generator.get(), values, uniform);
                sink += (*values)[0];
            }
            const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - begin).count();
            std::cout << "{\"record\":\"sobol\",\"sample\":" << sample << ",\"dimensions\":" << options.dimension_ << ",\"paths\":" << options.paths_
                      << ",\"precision\":\"" << options.precision_ << "\",\"wall_ns\":" << elapsed << ",\"fingerprint\":\"" << std::hex << fingerprint
                      << std::dec << "\"}\n";
        }
        Bench::DoNotOptimize(&sink);
    }
} // namespace

int RunSobolProfile(int argc, char** argv) {
    if (argc != 6 || std::string(argv[1]) != "--sobol-profile") {
        std::cerr << "usage: rng_perf --sobol-profile DIM PATHS uniform|fast|precise REPEATS\n";
        return 2;
    }
    try {
        const auto options = ParseOptions(argv);
        Vector_<> values(options.dimension_);
        auto reference = MakeGenerator(options);
        const auto fingerprint = Fingerprint(options, reference.get(), &values);
        RunSamples(options, &values, fingerprint);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

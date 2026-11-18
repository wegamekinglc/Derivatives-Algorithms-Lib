#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <string>

#include <dal/benchmarks/bench.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/math/aad/recording.hpp>
#include <dal/math/interp/interp.hpp>
#include <dal/platform/initall.hpp>
#include <dal/platform/platform.hpp>

#include "activeprofile.hpp"

using namespace Dal;

namespace {
    constexpr int KNOTS = 50, QUERIES = 10000, REPEATS = 10;
    using Clock_ = std::chrono::steady_clock;
    using Times_ = std::array<std::int64_t, 3>;

    std::int64_t Elapsed(Clock_::time_point begin) { return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock_::now() - begin).count(); }

    void Verify(bool flat, const Vector_<AAD::Number_>& ordinates, const Vector_<AAD::Number_>& queries, const AAD::Number_& total) {
        REQUIRE(std::fabs(Value(total) - (flat ? 2. : 51.) * QUERIES) < 1e-7, "active interpolation value mismatch");
        Vector_<> weights(KNOTS, 0.);
        for (int i = 0; i < QUERIES; ++i) {
            REQUIRE(std::fabs(AAD::NativeOperations_::ReadAdjoint(queries[i]) - (flat ? 0. : 2.)) < 1e-12,
                    "active interpolation query gradient mismatch");
            if (flat) {
                weights[0] += 1.;
            } else {
                const double query = 49. * (i + 1) / (QUERIES + 1.);
                const int left = static_cast<int>(std::floor(query));
                weights[left] += 1. - (query - left);
                weights[left + 1] += query - left;
            }
        }
        for (int i = 0; i < KNOTS; ++i)
            REQUIRE(std::fabs(AAD::NativeOperations_::ReadAdjoint(ordinates[i]) - weights[i]) < 1e-8,
                    "active interpolation ordinate gradient mismatch");
    }

    Times_ Action(bool flat, bool verify = false) {
        const auto start = Clock_::now();
        Times_ times{};
        {
            Vector_<> x(KNOTS);
            Vector_<AAD::Number_> y(KNOTS), queries(QUERIES);
            AAD::RecordingScope_ recording;
            for (int i = 0; i < KNOTS; ++i) {
                x[i] = i;
                recording.RegisterInput(y[i], 2. + 2. * i);
            }
            for (int i = 0; i < QUERIES; ++i)
                recording.RegisterInput(queries[i], flat ? -1. : 49. * (i + 1) / (QUERIES + 1.));
            recording.StartRecording();
            const auto record = Clock_::now();
            AAD::Number_ total = 0.;
            for (const auto& query : queries)
                total += InterpLinearImplX(x, y, query);
            recording.FinishRecording();
            times[0] = Elapsed(record);
            const auto reverse = Clock_::now();
            recording.ClearAdjoints();
            AAD::NativeOperations_::SetSeed(total, 1.);
            recording.Reverse();
            times[1] = Elapsed(reverse);
            if (verify)
                Verify(flat, y, queries, total);
            Bench::DoNotOptimize(&total);
            recording.Close();
        }
        // Includes input allocation/registration, recording, reverse, close and destruction.
        times[2] = Elapsed(start);
        return times;
    }

    Times_ Batch(bool flat, int loops) {
        Times_ sum{};
        for (int i = 0; i < loops; ++i) {
            const auto times = Action(flat);
            for (int phase = 0; phase < 3; ++phase)
                sum[phase] += times[phase];
        }
        return sum;
    }

    void PrintPhase(const std::string& profile, const char* phase, int column, int loops, const std::array<Times_, REPEATS>& samples) {
        std::array<std::int64_t, REPEATS> perCall{};
        for (int i = 0; i < REPEATS; ++i)
            perCall[i] = samples[i][column] / loops;
        std::sort(perCall.begin(), perCall.end());
        Bench::Print(
            {"Active linear " + profile + " " + phase + " (50 knots, 10K queries)", perCall[REPEATS / 2], perCall.front(), perCall.back(), REPEATS});
        std::cout << "PROFILE_SAMPLES {\"profile\":\"" << profile << "\",\"case\":\"" << phase << "\",\"knots\":" << KNOTS
                  << ",\"queries\":" << QUERIES << ",\"inner_loops\":" << loops << ",\"samples_ns\":[";
        for (int i = 0; i < REPEATS; ++i)
            std::cout << (i ? "," : "") << samples[i][column];
        std::cout << "],\"per_iteration_ns\":[";
        for (int i = 0; i < REPEATS; ++i)
            std::cout << (i ? "," : "") << std::setprecision(17) << static_cast<double>(samples[i][column]) / loops;
        std::cout << "]}\n";
    }

    void Measure(bool flat, const std::string& profile) {
        int loops = 1;
        while (Batch(flat, loops)[2] < 10000000 && loops < 128)
            loops *= 2;
        Batch(flat, loops);
        Batch(flat, loops);
        std::array<Times_, REPEATS> samples{};
        for (auto& sample : samples)
            sample = Batch(flat, loops);
        Bench::PrintHeader();
        PrintPhase(profile, "record", 0, loops, samples);
        PrintPhase(profile, "reverse", 1, loops, samples);
        PrintPhase(profile, "lifecycle", 2, loops, samples);
    }

    bool ValidOptions(int argc, char** argv) {
        if (argc < 3 || argc > 4 || std::string(argv[1]) != "--active-profile")
            return false;
        const std::string profile = argv[2];
        if (profile != "interval" && profile != "flat")
            return false;
        return argc == 3 || std::string(argv[3]) == "--preflight";
    }
} // namespace

int RunActiveInterpolationProfile(int argc, char** argv) {
    if (!ValidOptions(argc, argv)) {
        std::cerr << "usage: interp_perf --active-profile interval|flat [--preflight]\n";
        return 2;
    }
    try {
        RegisterAll_::Init();
        const std::string profile = argv[2];
        const bool flat = profile == "flat";
        Action(flat, true);
        std::cout << "ACTIVE_PREFLIGHT profile=" << profile << " knots=" << KNOTS << " queries=" << QUERIES << " query_gradient=" << (flat ? 0 : 2)
                  << " ordinate_gradients=verified\n";
        if (argc == 3)
            Measure(flat, profile);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

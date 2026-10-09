//
// Created by Codex on 2026/10/09.
//

#include <exception>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <string>

#include <dal/benchmarks/bench.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/segmentedpath.hpp>
#include <dal/script/simulation.hpp>

#ifndef DAL_SEGMENTED_MC_BASELINE
#include <dal/script/segmentedmontecarlo.hpp>
#endif

#include "segmentedmontecarloperf.hpp"

namespace {
    using namespace Dal;
    constexpr size_t MONTE_CARLO_PATHS = 128;

    struct Options_ {
        std::string shape_ = "short";
        std::string mode_ = "full";
        size_t threads_ = 1;
    };

    Options_ ParseOptions(int argc, char** argv) {
        Options_ result;
        for (int i = 1; i < argc; i += 2) {
            REQUIRE(i + 1 < argc, "segmented MC benchmark option requires a value");
            const std::string option(argv[i]);
            const std::string value(argv[i + 1]);
            if (option == "--case")
                result.shape_ = value;
            else if (option == "--mode")
                result.mode_ = value;
            else if (option == "--threads") {
                REQUIRE(value == "1" || value == "4", "segmented MC benchmark threads must be 1 or 4");
                result.threads_ = value == "4" ? 4 : 1;
            } else
                THROW("unknown segmented MC benchmark option");
        }
        return result;
    }

    size_t Steps(const std::string& shape) {
        if (shape == "short")
            return 16;
        REQUIRE(shape == "running", "segmented MC benchmark case must be short or running");
        return 2048;
    }

    std::shared_ptr<const Script::BlackScholesSegmentedPreparation_> PrepareMonteCarlo(size_t steps) {
        Vector_<Cell_> dates;
        Vector_<String_> events;
        for (size_t i = 0; i < steps; ++i) {
            dates.emplace_back(Date_(2026, 10, 2).AddDays(static_cast<int>(i)));
            String_ event = "running = running + FIX(EQ[DAL196_TEST])";
            if (i + 1 == steps)
                event += " pay PAYS running";
            events.push_back(std::move(event));
        }
        Script::ScriptValuationSettings_ valuation;
        valuation.evaluationDate_ = Date_(2026, 10, 1);
        return std::make_shared<const Script::BlackScholesSegmentedPreparation_>(
            Script::PrepareBlackScholesSegmentedScript(Script::ScriptProductData_("", dates, events), valuation));
    }

    struct Mean_ {
        double value_ = 0.0;
        Vector_<> gradient_;
    };

    Mean_ FullMonteCarlo(const Script::PreparedScript_& prepared, const Handle_<ModelData_>& model) {
        auto result = Script::MCSimulation<AAD::Number_>(prepared, model, MONTE_CARLO_PATHS, "sobol", false, true);
        return {result.aggregated_ / static_cast<double>(MONTE_CARLO_PATHS), result.risks_};
    }

    void Print(const Options_& options, const Bench::Result_& timing, const Mean_& mean) {
        std::cout << std::setprecision(17) << "{\"case\":\"" << options.shape_ << "\",\"mode\":\"" << options.mode_
                  << "\",\"threads\":" << options.threads_ << ",\"paths\":" << MONTE_CARLO_PATHS << ",\"steps\":" << Steps(options.shape_)
                  << ",\"min_ns\":" << timing.minNs << ",\"value\":" << mean.value_ << ",\"gradient\":[";
        for (size_t i = 0; i < mean.gradient_.size(); ++i) {
            if (i != 0)
                std::cout << ',';
            std::cout << mean.gradient_[i];
        }
        std::cout << "]}\n";
    }

    void Run(const Options_& options) {
        REQUIRE(options.mode_ == "full" || options.mode_ == "segmented", "segmented MC benchmark mode must be full or segmented");
        ThreadPool_* pool = ThreadPool_::GetInstance();
        pool->Start(options.threads_, true);
        (void)AAD::Tape();
        const auto prepared = PrepareMonteCarlo(Steps(options.shape_));
        const Handle_<ModelData_> model(new BSModelData_("", 100.0, 0.2, 0.03, 0.01));
        Mean_ mean;
#ifdef DAL_SEGMENTED_MC_BASELINE
        REQUIRE(options.mode_ == "full", "baseline has no segmented MC entry point");
#else
        const Vector_<> parameters{100.0, 0.2, 0.03, 0.01};
        std::optional<Script::BlackScholesSegmentedPath_> kernel;
        if (options.mode_ == "segmented")
            kernel.emplace(prepared);
#endif
        const auto timing = Bench::Run(
            "financial MC " + options.mode_,
            [&]() {
#ifndef DAL_SEGMENTED_MC_BASELINE
                if (kernel) {
                    const auto result = Script::EvaluateBlackScholesSegmentedMonteCarlo(*kernel, parameters, MONTE_CARLO_PATHS);
                    mean = {result.MeanValue(), result.MeanGradient()};
                } else
#endif
                {
                    mean = FullMonteCarlo(prepared->Prepared(), model);
                }
                Bench::DoNotOptimize(&mean);
            },
            1, 3);
        Print(options, timing, mean);
        pool->Stop();
    }
} // namespace

int RunSegmentedMonteCarloBenchmarks(int argc, char** argv) {
    try {
        Dal::RegisterAll_::Init();
        Run(ParseOptions(argc, argv));
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 2;
    }
}

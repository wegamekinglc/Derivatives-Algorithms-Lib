//
// Created by Codex on 2026/10/9.
//

#include <charconv>
#include <iomanip>
#include <iostream>
#include <string>

#include <dal/benchmarks/bench.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/simulation.hpp>

#include "rngpolicyprofile.hpp"

namespace {
    double ParseWidth(const char* text) {
        const std::string input = text;
        size_t consumed = 0;
        const double width = std::stod(input, &consumed);
        REQUIRE(consumed == input.size(), "smoothing width must be a finite positive number");
        Dal::Script::ValidateSmoothing(width);
        return width;
    }

    size_t ParsePositiveInteger(const char* text, const char* name) {
        const std::string input = text;
        size_t value = 0;
        const auto parsed = std::from_chars(input.data(), input.data() + input.size(), value);
        REQUIRE(parsed.ec == std::errc() && parsed.ptr == input.data() + input.size() && value > 0,
                Dal::String_(name) + " must be a positive integer");
        return value;
    }

    Dal::Script::MonteCarloSettings_ ParseSettings(int argc, char** argv) {
        using namespace Dal::Script;
        const std::string mode = argv[2];
        REQUIRE(mode == "double" || mode == "aad", "MODE must be double or aad");
        MonteCarloSettings_ settings;
        settings.rsg_ = argv[3];
        settings.normalPrecision_ = argv[4];
        settings.enableAad_ = mode == "aad";
        if (argc >= 7)
            settings.smooth_ = ParseWidth(argv[6]);
        if (argc >= 8) {
            const std::string evaluator = argv[7];
            REQUIRE(evaluator == "tree" || evaluator == "compiled", "evaluator must be tree or compiled");
            settings.compiled_ = evaluator == "compiled";
        }
        ValidateSimulationSettings(settings);
        return settings;
    }
} // namespace

int RunRngPolicyProfile(int argc, char** argv, Dal::Script::ScriptProductData_ (*factory)(const Dal::String_&)) {
    using namespace Dal;
    using namespace Dal::Script;
    if (argc < 6 || argc > 10) {
        std::cerr << "usage: script_mc_perf --rng-policy double|aad sobol|mrg32|irn Default|Fast|Precise PATHS [SMOOTH] [tree|compiled] "
                     "[BARRIER_SMOOTH] [THREADS]\n";
        return 2;
    }
    const auto settings = ParseSettings(argc, argv);
    const size_t paths = ParsePositiveInteger(argv[5], "PATHS");
    const std::string mode = argv[2];
    const String_ method = settings.rsg_, precision = settings.normalPrecision_;
    const String_ barrierWidth = argc >= 9 ? argv[8] : "0.1";
    static_cast<void>(ParseWidth(barrierWidth.c_str()));
    ThreadPool_::GetInstance()->Start(argc == 10 ? ParsePositiveInteger(argv[9], "THREADS") : 1, true);
    const auto data = factory(barrierWidth);
    const Handle_<ModelData_> modelData(new BSModelData_("bs", 100.0, 0.20, 0.05, 0.02));
    SimResults_ values({});
    const auto timing = Bench::Run(
        "BS weekly barrier " + mode + " " + std::string(method.c_str()) + " " + std::string(precision.c_str()),
        [&] {
            auto model = CreateModel<double>(modelData);
            const auto prepared = PrepareScript(data, model.get(), {}, settings);
            if (settings.enableAad_)
                values = MCSimulation<AAD::Number_>(prepared, modelData, paths, method, false, settings.compiled_, prepared.MaxNestedIfs(),
                                                    settings.smooth_);
            else
                values = MCSimulation<double>(prepared, modelData, paths, method, false, settings.compiled_);
        },
        1, 3);
    Bench::PrintHeader();
    Bench::Print(timing);
    std::cout << std::setprecision(17) << "NUMERICS paths=" << paths << " threads=" << ThreadPool_::GetInstance()->NumThreads()
              << " PV=" << values.aggregated_ / paths;
    if (settings.enableAad_)
        for (size_t i = 0; i < values.risks_.size(); ++i)
            std::cout << " " << values.names_[i] << "=" << values.risks_[i];
    std::cout << '\n';
    return 0;
}

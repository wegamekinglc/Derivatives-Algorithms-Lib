//
// Created by dal-implementer on 2026-7-4.
//
// Script-engine tree-walk vs compiled evaluator benchmarks.

#include <algorithm>
#include <charconv>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>

#include <dal/benchmarks/bench.hpp>
#include <dal/math/aad/aad.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/model/dupire.hpp>
#include <dal/platform/initall.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/event.hpp>
#include <dal/script/simulation.hpp>
#include <dal/storage/globals.hpp>
#include <dal/time/date.hpp>
#include <dal/time/dateincrement.hpp>
#include <dal/time/schedules.hpp>

using namespace Dal;
using namespace Dal::Script;

namespace {
    ScriptProduct_ BuildVanillaProduct() {
        const Date_ maturity = Date_(2025, 1, 1);
        Vector_<Cell_> eventDates;
        Vector_<String_> events;
        eventDates.push_back(Cell_("STRIKE"));
        events.push_back("100.0");
        eventDates.push_back(Cell_(maturity));
        events.push_back(String_("call pays MAX(spot() - STRIKE, 0.0)"));
        return {eventDates, events, "call"};
    }

    ScriptProduct_ BuildWeeklyBarrierProduct() {
        const Date_ start = Date_(2024, 1, 1);
        const Date_ maturity = Date_(2025, 1, 1);
        Vector_<Cell_> eventDates;
        Vector_<String_> events;
        eventDates.push_back(Cell_("STRIKE"));
        events.push_back("100.0");
        eventDates.push_back(Cell_("BARRIER"));
        events.push_back("150.0");
        eventDates.push_back(Cell_(start));
        events.push_back("alive = 1");
        eventDates.push_back(Cell_("START: " + Date::ToString(start) +
                                   " END: " + Date::ToString(maturity) +
                                   " FREQ: 1W"));
        events.push_back("if spot() >= BARRIER:0.1 then alive = 0 end");
        eventDates.push_back(Cell_(maturity));
        events.push_back(String_("uoc pays alive * MAX(spot() - STRIKE, 0.0)"));
        return {eventDates, events, "uoc"};
    }

    Handle_<ModelData_> BuildModelData() {
        return Handle_<ModelData_>(new BSModelData_("bs", 100.0, 0.20, 0.05, 0.02));
    }

    //  Weekly-exercise Bermudan put: every schedule row may exercise into the put
    //  intrinsic (the END row covers maturity), the maturity event pays the vanilla
    //  leg. Valuation diverts to the LSMC driver (prepared pipeline).
    ScriptProductData_ BuildBermudanExerciseProduct(const String_& frequency = "1W", bool deadFix = false) {
        const Date_ start = Date_(2024, 1, 8);
        const Date_ maturity = Date_(2025, 1, 1);
        Vector_<Cell_> eventDates;
        Vector_<String_> events;
        eventDates.push_back(Cell_(String_("STRIKE")));
        events.push_back("100.0");
        eventDates.push_back(Cell_(String_("START: " + Date::ToString(start) + " END: " + Date::ToString(maturity) + " FREQ: " + frequency)));
        events.push_back(deadFix ? "unused = FIX(EQ[DAL418_BENCH])\nEXERCISE MAX(STRIKE - 110.0, 0.0)" : "EXERCISE MAX(STRIKE - spot(), 0.0)");
        eventDates.push_back(Cell_(maturity));
        events.push_back(deadFix ? "call PAYS MAX(FIX(EQ[DAL418_BENCH]) - STRIKE, 0.0)" : "call PAYS MAX(spot() - STRIKE, 0.0)");
        return {"", eventDates, events};
    }

    struct ScriptBenchmarkCase_ {
        const char* name_;
        ScriptProduct_ (*buildProduct_)();
        size_t doublePaths_;
        size_t aadPaths_;
        int eventCount_;
    };

    std::string CaseName(const ScriptBenchmarkCase_& scriptCase,
                         const char* valueType,
                         bool compiled,
                         size_t paths) {
        return std::string("script engine ") + scriptCase.name_ + " " + valueType +
               " compiled=" + (compiled ? "true" : "false") +
               " (" + std::to_string(paths) + " paths x " +
               std::to_string(scriptCase.eventCount_) + " events)";
    }

    void RunDoubleCase(const ScriptBenchmarkCase_& scriptCase, bool compiled, int repeats) {
        double sink = 0.0;
        auto r = Bench::Run(CaseName(scriptCase, "double", compiled, scriptCase.doublePaths_), [&]() {
            ScriptProduct_ product = scriptCase.buildProduct_();
            (void) product.PreProcess(false, false);
            auto results = MCSimulation<double>(
                product, BuildModelData(), scriptCase.doublePaths_, "sobol", false, compiled);
            sink += results.aggregated_;
        }, 1, repeats);
        Bench::Print(r);
        Bench::DoNotOptimize(&sink);
    }

    void RunAadCase(const ScriptBenchmarkCase_& scriptCase, bool compiled, int repeats) {
        double sink = 0.0;
        auto r = Bench::Run(CaseName(scriptCase, "Number_", compiled, scriptCase.aadPaths_), [&]() {
            ScriptProduct_ product = scriptCase.buildProduct_();
            int maxNestedIfs = product.PreProcess(true, true);
            auto results = MCSimulation<Number_>(
                product, BuildModelData(), scriptCase.aadPaths_, "sobol",
                false, compiled, maxNestedIfs, 0.01);
            sink += results.aggregated_;
            if (!results.risks_.empty())
                sink += results.risks_[0];
        }, 1, repeats);
        Bench::Print(r);
        Bench::DoNotOptimize(&sink);
    }

    void RunDoubleExerciseCase(bool compiled, size_t paths, int repeats) {
        double sink = 0.0;
        const std::string name = std::string("script engine bermudan exercise double compiled=") +
                                 (compiled ? "true" : "false") + " (" + std::to_string(paths) + " paths x 54 events)";
        auto r = Bench::Run(name, [&]() {
            MonteCarloSettings_ simulation;
            simulation.compiled_ = compiled;
            auto results = MCSimulation<double>(
                BuildBermudanExerciseProduct(), BuildModelData(), paths,
                ScriptValuationSettings_(), simulation);
            sink += results.aggregated_;
        }, 1, repeats);
        Bench::Print(r);
        Bench::DoNotOptimize(&sink);
    }

    void RunRegressionCase(int degree, int repeats) {
        constexpr size_t N_PATHS = 100000;
        Vector_<> x(N_PATHS), targets(N_PATHS);
        const Vector_<char> included(N_PATHS, 1);
        for (size_t i = 0; i < N_PATHS; ++i) {
            x[i] = 80.0 + 40.0 * static_cast<double>(i) / static_cast<double>(N_PATHS - 1);
            targets[i] = std::max(100.0 - x[i], 0.0);
        }
        double sink = 0.0;
        auto r = Bench::Run(
            "LSMC regression degree=" + std::to_string(degree) + " (100000 paths)",
            [&]() {
                const auto fit = SolveExerciseRegression(x, targets, included, degree);
                sink += RegressionPredict(fit, 95.0);
            },
            1, repeats);
        Bench::Print(r);
        Bench::DoNotOptimize(&sink);
    }

    bool ParsePathCount(const char* text, size_t* count) {
        const char* end = text + std::char_traits<char>::length(text);
        const auto parsed = std::from_chars(text, end, *count);
        return parsed.ec == std::errc() && parsed.ptr == end && *count > 0;
    }

    //  Optional single-run profile for paired baseline/head measurements. Keeping
    //  each case in its own process makes peak RSS attributable to that case.
    int RunLsmcReplayProfile(int argc, char** argv) {
        if (argc != 8 && argc != 9) {
            std::cerr << "usage: script_mc_perf --lsmc-replay TRAINING PRICING 1W|1CD hard|aad bs|dupire tree|compiled [deadfix]\n";
            return 2;
        }
        size_t trainingPaths = 0;
        size_t pricingPaths = 0;
        if (!ParsePathCount(argv[2], &trainingPaths) || !ParsePathCount(argv[3], &pricingPaths)) {
            std::cerr << "invalid LSMC path counts\n";
            return 2;
        }
        if (trainingPaths > static_cast<size_t>(std::numeric_limits<int>::max())) {
            std::cerr << "LSMC training path count exceeds the settings range\n";
            return 2;
        }
        const String_ frequency = argv[4];
        const std::string mode = argv[5];
        const std::string modelName = argv[6];
        const std::string engine = argv[7];
        const bool deadFix = argc == 9 && std::string(argv[8]) == "deadfix";
        if ((frequency != "1W" && frequency != "1CD") || (mode != "hard" && mode != "aad") || (modelName != "bs" && modelName != "dupire") ||
            (engine != "tree" && engine != "compiled") || (argc == 9 && !deadFix)) {
            std::cerr << "invalid LSMC replay profile arguments\n";
            return 2;
        }

        const Handle_<ModelData_> model =
            modelName == "bs"
                ? BuildModelData()
                : Handle_<ModelData_>(new DupireModelData_("dupire", 100.0, 0.05, 0.02, {50.0, 150.0}, {0.0, 2.0}, Matrix_<>(2, 2, 0.20)));
        MonteCarloSettings_ simulation;
        simulation.compiled_ = engine == "compiled";
        simulation.lsmcTrainingPaths_ = trainingPaths;
        simulation.enableAad_ = mode == "aad";
        const auto begin = std::chrono::steady_clock::now();
        double pv = 0.0;
        Vector_<> risks;
        if (mode == "aad") {
            const auto result = MCSimulation<AAD::Number_>(BuildBermudanExerciseProduct(frequency, deadFix), model, pricingPaths,
                                                           ScriptValuationSettings_(), simulation);
            pv = result.aggregated_ / static_cast<double>(pricingPaths);
            risks = result.risks_;
        } else {
            const auto result =
                MCSimulation<double>(BuildBermudanExerciseProduct(frequency, deadFix), model, pricingPaths, ScriptValuationSettings_(), simulation);
            pv = result.aggregated_ / static_cast<double>(pricingPaths);
        }
        const auto end = std::chrono::steady_clock::now();
        const double elapsedMs = std::chrono::duration<double, std::milli>(end - begin).count();
        std::cout << std::setprecision(17) << "LSMC_REPLAY training=" << trainingPaths << " pricing=" << pricingPaths << " frequency=" << frequency
                  << " mode=" << mode << " model=" << modelName << " engine=" << engine << " deadfix=" << deadFix << " time_ms=" << elapsedMs
                  << " pv=" << pv << " risks=";
        for (size_t i = 0; i < risks.size(); ++i)
            std::cout << (i ? "," : "") << risks[i];
        std::cout << '\n';
        return 0;
    }
} // namespace

int main(int argc, char** argv) {
    RegisterAll_::Init();
    Global::Dates_::SetEvaluationDate(Date_(2024, 1, 1));
    if (argc > 1)
        return std::string(argv[1]) == "--lsmc-replay" ? RunLsmcReplayProfile(argc, argv) : 2;
    constexpr int kRepeats = 3;
    Bench::PrintHeader();

    const ScriptBenchmarkCase_ scriptCases[] = {
        {"vanilla", BuildVanillaProduct, 200000, 20000, 1},
        {"weekly barrier", BuildWeeklyBarrierProduct, 100000, 10000, 52},
    };

    for (const auto& scriptCase: scriptCases) {
        RunDoubleCase(scriptCase, false, kRepeats);
        RunDoubleCase(scriptCase, true, kRepeats);
        RunAadCase(scriptCase, false, kRepeats);
        RunAadCase(scriptCase, true, kRepeats);
    }

    //  Early-exercise products: LSMC driver, double engine — pins the cost of the
    //  duplicated path generation/evaluation plus the regressions
    RunDoubleExerciseCase(false, 100000, kRepeats);
    RunDoubleExerciseCase(true, 100000, kRepeats);
    RunRegressionCase(3, kRepeats);
    RunRegressionCase(8, kRepeats);

    return 0;
}

//
// Created by dal-implementer on 2026-7-4.
//
// Script-engine tree-walk vs compiled evaluator benchmarks.

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <initializer_list>
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

#include "correlatedbsperf.hpp"

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

    void
    RunRegressionCase(const std::string& name, const Vector_<>& x, const Vector_<>& targets, const Vector_<char>& included, int degree, int repeats) {
        double sink = 0.0;
        auto r = Bench::Run(
            name,
            [&]() {
                const auto fit = SolveExerciseRegression(x, targets, included, degree);
                sink += RegressionPredict(fit, 95.0);
            },
            1, repeats);
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
        RunRegressionCase("LSMC regression degree=" + std::to_string(degree) + " (100000 paths)", x, targets, included, degree, repeats);
    }

    //  Real regression sets are the in-the-money paths in path order: a mask of about
    //  one half with no pattern a branch predictor can learn
    void RunMaskedRegressionCase(int degree, int repeats) {
        constexpr size_t N_PATHS = 100000;
        Vector_<> x(N_PATHS), targets(N_PATHS);
        Vector_<char> included(N_PATHS);
        uint64_t state = 20260926;
        for (size_t i = 0; i < N_PATHS; ++i) {
            state = state * 6364136223846793005ULL + 1442695040888963407ULL;
            x[i] = 80.0 + 40.0 * static_cast<double>(state >> 11) / 9007199254740992.0;
            targets[i] = std::max(100.0 - x[i], 0.0) + 0.1 * std::sin(x[i]);
            included[i] = static_cast<char>(x[i] < 100.0);
        }
        RunRegressionCase("LSMC regression degree=" + std::to_string(degree) + " ITM mask (100000 paths)", x, targets, included, degree, repeats);
    }

    bool ParsePathCount(const char* text, size_t* count) {
        const char* end = text + std::char_traits<char>::length(text);
        const auto parsed = std::from_chars(text, end, *count);
        return parsed.ec == std::errc() && parsed.ptr == end && *count > 0;
    }

    struct ReplayProfile_ {
        size_t trainingPaths_ = 0;
        size_t pricingPaths_ = 0;
        String_ frequency_;
        std::string mode_;
        std::string model_;
        std::string engine_;
        bool deadFix_ = false;
        bool retrainedPolicy_ = false;
    };

    bool IsAllowed(const std::string& value, std::initializer_list<const char*> choices) {
        return std::any_of(choices.begin(), choices.end(), [&](const char* choice) { return value == choice; });
    }

    bool ValidReplayOptions(int argc, char** argv) {
        if (!IsAllowed(argv[4], {"1W", "1CD"}) || !IsAllowed(argv[5], {"hard", "aad"}) || !IsAllowed(argv[6], {"bs", "dupire"}) ||
            !IsAllowed(argv[7], {"tree", "compiled"}))
            return false;
        bool deadFix = false, retrained = false;
        for (int i = 8; i < argc; ++i) {
            const std::string option(argv[i]);
            if (option == "deadfix" && !deadFix)
                deadFix = true;
            else if (option == "retrained" && !retrained && std::string(argv[5]) == "aad")
                retrained = true;
            else
                return false;
        }
        return true;
    }

    bool ParseReplayCounts(char** argv, ReplayProfile_* profile) {
        return ParsePathCount(argv[2], &profile->trainingPaths_) && ParsePathCount(argv[3], &profile->pricingPaths_) &&
               profile->trainingPaths_ <= static_cast<size_t>(std::numeric_limits<int>::max());
    }

    bool ParseReplayProfile(int argc, char** argv, ReplayProfile_* profile) {
        if (argc < 8 || argc > 10)
            return false;
        if (!ParseReplayCounts(argv, profile) || !ValidReplayOptions(argc, argv))
            return false;
        profile->frequency_ = argv[4];
        profile->mode_ = argv[5];
        profile->model_ = argv[6];
        profile->engine_ = argv[7];
        for (int i = 8; i < argc; ++i) {
            profile->deadFix_ |= std::string(argv[i]) == "deadfix";
            profile->retrainedPolicy_ |= std::string(argv[i]) == "retrained";
        }
        return true;
    }

    SimResults_ RunReplayValuation(const ReplayProfile_& profile, const Handle_<ModelData_>& model, const MonteCarloSettings_& simulation) {
        const auto product = BuildBermudanExerciseProduct(profile.frequency_, profile.deadFix_);
        if (profile.mode_ == "aad")
            return MCSimulation<AAD::Number_>(product, model, profile.pricingPaths_, ScriptValuationSettings_(), simulation);
        return MCSimulation<double>(product, model, profile.pricingPaths_, ScriptValuationSettings_(), simulation);
    }

    //  Optional single-run profile for paired baseline/head measurements. Keeping
    //  each case in its own process makes peak RSS attributable to that case.
    int RunLsmcReplayProfile(int argc, char** argv) {
        ReplayProfile_ profile;
        if (!ParseReplayProfile(argc, argv, &profile)) {
            std::cerr << "usage: script_mc_perf --lsmc-replay TRAINING PRICING 1W|1CD hard|aad bs|dupire tree|compiled [deadfix] [retrained]\n";
            return 2;
        }
        const Handle_<ModelData_> model =
            profile.model_ == "bs"
                ? BuildModelData()
                : Handle_<ModelData_>(new DupireModelData_("dupire", 100.0, 0.05, 0.02, {50.0, 150.0}, {0.0, 2.0}, Matrix_<>(2, 2, 0.20)));
        MonteCarloSettings_ simulation;
        simulation.compiled_ = profile.engine_ == "compiled";
        simulation.lsmcTrainingPaths_ = profile.trainingPaths_;
        simulation.enableAad_ = profile.mode_ == "aad";
        simulation.lsmcPolicyRiskMode_ = profile.retrainedPolicy_ ? "RetrainedBump" : "Frozen";
        const auto begin = std::chrono::steady_clock::now();
        const auto result = RunReplayValuation(profile, model, simulation);
        const auto end = std::chrono::steady_clock::now();
        const double elapsedMs = std::chrono::duration<double, std::milli>(end - begin).count();
        std::cout << std::setprecision(17) << "LSMC_REPLAY training=" << profile.trainingPaths_ << " pricing=" << profile.pricingPaths_
                  << " frequency=" << profile.frequency_ << " mode=" << profile.mode_ << " model=" << profile.model_ << " engine=" << profile.engine_
                  << " deadfix=" << profile.deadFix_ << " policy=" << simulation.lsmcPolicyRiskMode_ << " time_ms=" << elapsedMs
                  << " pv=" << result.aggregated_ / static_cast<double>(profile.pricingPaths_) << " risks=";
        const Vector_<> risks = profile.mode_ == "aad" ? result.risks_ : Vector_<>();
        for (size_t i = 0; i < risks.size(); ++i)
            std::cout << (i ? "," : "") << risks[i];
        std::cout << '\n';
        return 0;
    }
} // namespace

int main(int argc, char** argv) {
    RegisterAll_::Init();
    Global::Dates_::SetEvaluationDate(Date_(2024, 1, 1));
    if (argc > 1) {
        if (std::string(argv[1]) == "--correlated-bs") {
            Bench::PrintHeader();
            RunCorrelatedBSPathCases();
            return 0;
        }
        return std::string(argv[1]) == "--lsmc-replay" ? RunLsmcReplayProfile(argc, argv) : 2;
    }
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
    RunMaskedRegressionCase(3, kRepeats);
    RunMaskedRegressionCase(8, kRepeats);
    RunCorrelatedBSPathCases();

    return 0;
}

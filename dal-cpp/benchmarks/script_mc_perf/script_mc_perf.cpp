//
// Created by dal-implementer on 2026-7-4.
//
// Script-engine per-path evaluator benchmark set.
// MCSimulation is the N_paths x N_events inner loop of every MC pricing -- the
// dominant per-path cost. script_perf only times the parser front-end; it never
// calls MCSimulation. Tree-walk is the default for both specializations;
// compiled=true opts into the flat-stream evaluator. This target times paired
// compiled=false/true runs across simple and schedule-heavy products for both
// double (production value path) and Number_ (AAD/tape-on-MC path).

#include <string>

#include <dal/platform/platform.hpp>
#include <dal/platform/initall.hpp>
#include <dal/math/aad/aad.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/script/event.hpp>
#include <dal/script/simulation.hpp>
#include <dal/storage/globals.hpp>
#include <dal/time/date.hpp>
#include <dal/time/dateincrement.hpp>
#include <dal/time/schedules.hpp>
#include <dal/benchmarks/bench.hpp>

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

    // A weekly-barrier up-and-out call, ~52 monitoring events over a 1Y maturity.
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
} // namespace

int main() {
    RegisterAll_::Init();
    Global::Dates_::SetEvaluationDate(Date_(2024, 1, 1));
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

    return 0;
}

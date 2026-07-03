//
// Created by dal-implementer on 2026-7-4.
//
// Script-engine per-path evaluator micro-benchmark.
// MCSimulation is the N_paths x N_events inner loop of every MC pricing -- the
// dominant per-path cost. script_perf only times the parser front-end; it never
// calls MCSimulation. ValueByMonteCarlo hardcodes compiled=false, so the AST
// interpreter is the production default and is unmonitored; the compiled path is
// exercised only by examples/uoc_compiled. This target times both evaluator forms
// for the double specialization (the production pricing path) and the Number_
// specialization (the AAD/tape-on-MC path).

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
    // A weekly-barrier up-and-out call, ~52 monitoring events over a 1Y maturity.
    // This is the production shape (central-difference LV calibration, XVA bumps)
    // at a path count small enough to keep a benchmark repeat under a few seconds.
    ScriptProduct_ BuildProduct() {
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
        eventDates.push_back(Cell_("START: " + Date::ToString(start) + " END: " + Date::ToString(maturity) + " FREQ: 1W"));
        events.push_back("if spot() >= BARRIER:0.1 then alive = 0 end");
        eventDates.push_back(Cell_(maturity));
        events.push_back(String_("uoc pays alive * MAX(spot() - STRIKE, 0.0)"));
        return {eventDates, events, "uoc"};
    }

    Handle_<ModelData_> BuildModelData() {
        return Handle_<ModelData_>(new BSModelData_("bs", 100.0, 0.20, 0.05, 0.02));
    }
} // namespace

int main() {
    RegisterAll_::Init();
    Global::Dates_::SetEvaluationDate(Date_(2024, 1, 1));
    constexpr int kRepeats = 3;
    Bench::PrintHeader();

    constexpr size_t kDoublePaths = 100000;
    constexpr size_t kAadPaths = 10000;

    {
        double sink = 0.0;
        auto r = Bench::Run("MCSimulation<double> compiled=false (1e5 paths x 52 events)", [&]() {
            ScriptProduct_ product = BuildProduct();
            (void) product.PreProcess(false, false);
            auto results = MCSimulation<double>(product, BuildModelData(), kDoublePaths, "sobol", false, false);
            sink += results.aggregated_;
        }, 1, kRepeats);
        Bench::Print(r);
        Bench::DoNotOptimize(&sink);
    }

    {
        double sink = 0.0;
        auto r = Bench::Run("MCSimulation<double> compiled=true (1e5 paths x 52 events)", [&]() {
            ScriptProduct_ product = BuildProduct();
            (void) product.PreProcess(false, false);
            product.Compile();
            auto results = MCSimulation<double>(product, BuildModelData(), kDoublePaths, "sobol", false, true);
            sink += results.aggregated_;
        }, 1, kRepeats);
        Bench::Print(r);
        Bench::DoNotOptimize(&sink);
    }

    {
        double sink = 0.0;
        auto r = Bench::Run("MCSimulation<Number_> compiled=false (1e4 paths x 52 events)", [&]() {
            ScriptProduct_ product = BuildProduct();
            int maxNestedIfs = product.PreProcess(true, true);
            auto results = MCSimulation<Number_>(product, BuildModelData(), kAadPaths, "sobol", false, false, maxNestedIfs, 0.01);
            sink += results.aggregated_;
            if (!results.risks_.empty())
                sink += results.risks_[0];
        }, 1, kRepeats);
        Bench::Print(r);
        Bench::DoNotOptimize(&sink);
    }

    {
        double sink = 0.0;
        auto r = Bench::Run("MCSimulation<Number_> compiled=true (1e4 paths x 52 events)", [&]() {
            ScriptProduct_ product = BuildProduct();
            int maxNestedIfs = product.PreProcess(true, true);
            product.Compile();
            auto results = MCSimulation<Number_>(product, BuildModelData(), kAadPaths, "sobol", false, true, maxNestedIfs, 0.01);
            sink += results.aggregated_;
            if (!results.risks_.empty())
                sink += results.risks_[0];
        }, 1, kRepeats);
        Bench::Print(r);
        Bench::DoNotOptimize(&sink);
    }

    return 0;
}

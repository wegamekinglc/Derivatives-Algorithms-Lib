//
// Created by wegam on 2026/5/30.
//
// Benchmark set for the script engine front-end.
//
// The engine is split into two independently testable stages:
//   1. Preprocessor_ -- resolves the "definition" part (const variables,
//      macros and schedules) of an events table into dated descriptions.
//   2. Parser_       -- turns those descriptions into the payoff AST.
//
// This benchmark measures each stage on its own and the combined
// ScriptProduct_ construction so we can confirm the refactor into two parts
// does not degrade end-to-end throughput.

#include <iomanip>
#include <iostream>
#include <dal/platform/platform.hpp>
#include <dal/script/event.hpp>
#include <dal/script/preprocessor.hpp>
#include <dal/script/parser.hpp>
#include <dal/storage/globals.hpp>
#include <dal/utilities/algorithms.hpp>
#include <dal/utilities/timer.hpp>

using namespace std;
using namespace Dal;
using namespace Dal::Script;

namespace {
    // A non-trivial barrier/snowball-like events table exercising const
    // variables, macros, a weekly monitoring schedule and a final payoff.
    void BuildEventsTable(Vector_<Cell_>* dates, Vector_<String_>* events) {
        dates->clear();
        events->clear();

        // const variable definitions
        dates->emplace_back("BARRIER");
        events->emplace_back("150.00");
        dates->emplace_back("STRIKE");
        events->emplace_back("120.00");

        // initialization
        dates->emplace_back(Date_(2022, 9, 25));
        events->emplace_back("alive = 1");

        // monitoring schedule
        dates->emplace_back(
            "START: 2022-09-25\n"
            "END: 2025-09-25\n"
            "FREQ: 1W");
        events->emplace_back("IF spot() > BARRIER:0.1 THEN alive = 0 END");

        // final payoff
        dates->emplace_back(Date_(2025, 9, 25));
        events->emplace_back(
            "IF spot() > BARRIER:0.1 THEN alive = 0 END "
            "uoc pays alive * MAX(spot() - STRIKE, 0.0)");
    }

    void Report(const String_& label, int64_t micros, size_t iterations) {
        cout << setw(28) << left << label.c_str()
             << setw(14) << right << fixed << setprecision(3)
             << static_cast<double>(micros) / static_cast<double>(iterations)
             << " us/iter" << endl;
    }
} // namespace

int main() {
    Dal::RegisterAll_::Init();
    Global::Dates_::SetEvaluationDate(Date_(2022, 9, 25));

    Vector_<Cell_> dates;
    Vector_<String_> events;
    BuildEventsTable(&dates, &events);
    const auto table = Zip(dates, events);

    constexpr size_t kIterations = 2000;

    // Stage 1: preprocessing only.
    {
        Preprocessor_ preprocessor;
        volatile size_t sink = 0;
        Timer_ timer;
        for (size_t i = 0; i < kIterations; ++i) {
            auto result = preprocessor.Process(table);
            sink += result.events_.size();
        }
        Report("preprocess (stage 1)", timer.Elapsed<microseconds>(), kIterations);
        (void)sink;
    }

    // Stage 2: parsing only (preprocess once, reparse repeatedly).
    {
        Preprocessor_ preprocessor;
        auto preprocessed = preprocessor.Process(table);
        volatile size_t sink = 0;
        Timer_ timer;
        for (size_t i = 0; i < kIterations; ++i) {
            Parser_ parser(preprocessed.constVariables_);
            for (const auto& dateEvent : preprocessed.events_)
                sink += parser.Parse(dateEvent.second).size();
        }
        Report("parse (stage 2)", timer.Elapsed<microseconds>(), kIterations);
        (void)sink;
    }

    // Combined: full ScriptProduct_ construction (stage 1 + stage 2).
    {
        volatile size_t sink = 0;
        Timer_ timer;
        for (size_t i = 0; i < kIterations; ++i) {
            ScriptProduct_ product(dates, events);
            sink += product.EventDates().size();
        }
        Report("construct (stage 1+2)", timer.Elapsed<microseconds>(), kIterations);
        (void)sink;
    }

    return 0;
}

//
// Created by wegam on 2026/8/17.
//
//  Visualizes a script product with the width-aware Unicode tree dump
//  (ScriptProduct_::DebugTree) -- the human-friendly companion of the legacy
//  s-expression dump (examples/script) and the JSON dump (DebugJson).
//
//  Unicode output needs a UTF-8 terminal; on Windows use Windows Terminal or
//  `chcp 65001`. The ASCII style (ascii = true) is the constrained-console
//  fallback.

#include <iostream>

//  DAL headers first: <windows.h> macros (VOID, ...) clash with DAL enums
#include <dal/platform/platform.hpp>
#include <dal/script/event.hpp>
#include <dal/storage/globals.hpp>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;
using namespace Dal;
using namespace Dal::Script;


int main() {
#ifdef _WIN32
    //  Render the box-drawing glyphs on the Windows console as well
    SetConsoleOutputCP(CP_UTF8);
#endif

    Dal::RegisterAll_::Init();

    Global::Dates_::SetEvaluationDate(Date_(2022, 9, 25));

    Vector_<Cell_> eventDates;
    Vector_<String_> events;

    // macro definition for `BARRIER` and `STRIKE` constants
    eventDates.emplace_back("BARRIER");
    events.emplace_back("150.00");
    eventDates.emplace_back("STRIKE");
    events.emplace_back("120.00");

    // initialization
    eventDates.emplace_back(Date_(2022, 9, 25));
    events.emplace_back("alive = 1");

    // monitor periods -- quarterly over one year keeps the demo output compact
    eventDates.emplace_back(
            "START: 2022-09-25\n"
            "END: 2023-09-25\n"
            "FREQ: 3M");
    events.emplace_back("IF spot() > BARRIER:0.1 THEN alive = 0 END");

    // final payoff
    eventDates.emplace_back(Date_(2023, 9, 25));
    events.emplace_back("uoc pays alive * MAX(spot() - STRIKE, 0.0)");

    ScriptProduct_ product(eventDates, events);
    //  Resolve the variable/constant tables and the payoff slot, so the dump
    //  header shows `Variables:`/`Constants:` -- mirroring what the dal-public
    //  wrapper DebugScriptProductTree does on its private copy
    product.IndexVariables();

    //  Unicode style: statements inline while they fit the width budget
    cout << "=== Unicode tree (width = 125) ===" << endl;
    product.DebugTree(cout, false, 125);

    //  Narrow the budget: oversized statements expand into box-drawing branches
    cout << "\n=== Unicode tree (width = 40) ===" << endl;
    product.DebugTree(cout, false, 40);

    //  ASCII fallback for constrained consoles
    cout << "\n=== ASCII tree (width = 40) ===" << endl;
    product.DebugTree(cout, true, 40);

    return 0;
}

//
// Created by wegam on 2023/6/17.
//

#include <iostream>
#include <dal/platform/platform.hpp>
#include <dal/script/event.hpp>
#include <dal/storage/globals.hpp>

using namespace std;
using namespace Dal;
using namespace Dal::Script;


String_ DebugScriptProduct(const ScriptProduct_& product) {
    std::ostringstream out;
    product.Debug(out);
    String_ rtn(out.str());
    REQUIRE2(rtn.size() != 0, "emtpy script product description", ScriptError_);
    return rtn;
}


int main() {
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

    // monitor periods
    eventDates.emplace_back(
            "START: 2022-09-25\n"
            "END: 2025-09-25\n"
            "FREQ: 1W");
    events.emplace_back("IF spot() > BARRIER:0.1 THEN alive = 0 END");

    // final payoff
    eventDates.emplace_back(Date_(2025, 9, 25));
    events.emplace_back("IF spot() > BARRIER:0.1 THEN alive = 0 END uoc pays alive * MAX(spot() - STRIKE, 0.0)");

    ScriptProduct_ product(eventDates, events);
    std::cout << DebugScriptProduct(product) << std::endl;

    return 0;
}
//
// Created by wegam on 2023/6/17.
//

#include <dal/platform/platform.hpp>
#include <dal/script/event.hpp>
#include <dal/storage/globals.hpp>
#include <iomanip>
#include <iostream>

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

    ScriptProduct_ basket({Cell_("STRIKES"), Cell_("WEIGHTS"), Cell_(Date_(2022, 12, 25))},
                          {"[80, 100, 120]", "[0.2, 0.3, 0.5]", "FOR(i, 0, 3) pay PAYS WEIGHTS[i] * MAX(SPOT() - STRIKES[i], 0) END"});
    basket.PreProcess(false, false);
    AAD::Scenario_<double> path(1);
    path[0].spot_ = 130.0;
    path[0].numeraire_ = 1.0;
    auto evaluator = basket.BuildEvaluator<double>();
    basket.Evaluate(path, evaluator);
    std::cout << "basket payoff: " << std::fixed << std::setprecision(2) << evaluator.VarVals()[basket.PayOffIdx()] << std::endl;

    return 0;
}

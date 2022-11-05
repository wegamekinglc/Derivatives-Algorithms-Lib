//
// Created by wegam on 2020/12/21.
//

#include <iostream>
#include <dal/script/event.hpp>

using namespace std;
using namespace Dal;
using namespace Dal::Script;

int main() {

    ScriptProduct_ product;
    std::map<Date_, String_> events;

    events[Date_(2015, 4, 10)] = String_("alive = 1");
    events[Date_(2016, 4, 10)] = String_("if spot() > 100 then autocall pays 120 alive = 0 endIf");
    events[Date_(2017, 4, 10)] = String_("if spot() > 100 then autocall pays alive * 120 alive = 0 endIf");
    events[Date_(2018, 4, 10)] = String_("if spot() > 100 then autocall pays alive * 130 else autocall pays alive * 50 endIf");

    bool fuzzy = false;
    bool skipDoms = false;

    product.ParseEvents(events.begin(), events.end());
    int maxNestedIfs = product.PreProcess(fuzzy, skipDoms);
    product.Debug(std::cout);

    return 0;
}
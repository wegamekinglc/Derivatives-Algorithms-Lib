//
// Created by wegam on 2023/5/2.
//

#include <gtest/gtest.h>
#include <dal/script/event.hpp>

using namespace Dal;

TEST(EventTest, TestEventWithMacro) {
    Vector_<Cell_> dates;
    Vector_<String_> events;

    dates.push_back(Cell_("STRIKE"));
    events.push_back("110.0");

    dates.push_back((Cell_(Date_(2023, 12, 1))));
    events.push_back("call PAYS MAX(spot() - STRIKE, 0.0)");

    Script::ScriptProduct_ product(dates, events);

    ASSERT_EQ(product.EventDates().size(), 1);
    product.E
}

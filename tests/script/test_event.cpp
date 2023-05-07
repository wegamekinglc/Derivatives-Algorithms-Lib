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
}

TEST(EventTest, TestEventWithMacroNotFirst) {
    Vector_<Cell_> dates;
    Vector_<String_> events;

    dates.push_back((Cell_(Date_(2023, 12, 1))));
    events.push_back("call PAYS MAX(spot() - STRIKE, 0.0)");

    dates.push_back(Cell_("STRIKE"));
    events.push_back("110.0");

    ASSERT_THROW(Script::ScriptProduct_(dates, events), Dal::ScriptError_);
}

TEST(EventTest, TestEventWithMacroDuplicated) {
    Vector_<Cell_> dates;
    Vector_<String_> events;

    dates.push_back(Cell_("STRIKE"));
    events.push_back("110.0");

    dates.push_back(Cell_("STRIKE"));
    events.push_back("120.0");

    ASSERT_THROW(Script::ScriptProduct_(dates, events), Dal::ScriptError_);
}

TEST(EventTest, TestEventWithSchedule) {
    auto global = XGLOBAL::SetEvaluationDateInScope(Date_(2022, 5, 1));
    Vector_<Cell_> dates;
    Vector_<String_> events;

    dates.push_back(Cell_("START: 2022-05-07 END: 2023-05-07 FREQ: 1m CALENDAR: CN.SSE"));
    events.push_back("STRIKE = 110.0");
    Script::ScriptProduct_ product(dates, events);

    ASSERT_EQ(product.EventDates().size(), 12);
}
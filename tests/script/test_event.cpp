//
// Created by wegam on 2023/5/2.
//

#include <gtest/gtest.h>
#include <dal/script/event.hpp>
#include <dal/time/daybasis.hpp>

using namespace Dal;

TEST(ScriptTest, TestEventWithMacro) {
    Vector_<Cell_> dates;
    Vector_<String_> events;

    dates.push_back(Cell_("STRIKE"));
    events.push_back("110.0");

    dates.push_back((Cell_(Date_(2023, 12, 1))));
    events.push_back("call PAYS MAX(spot() - STRIKE, 0.0)");

    Script::ScriptProduct_ product(dates, events);

    ASSERT_EQ(product.EventDates().size(), 1);
}

TEST(ScriptTest, TestEventWithMacroNotFirst) {
    Vector_<Cell_> dates;
    Vector_<String_> events;

    dates.push_back((Cell_(Date_(2023, 12, 1))));
    events.push_back("call PAYS MAX(spot() - STRIKE, 0.0)");

    dates.push_back(Cell_("STRIKE"));
    events.push_back("110.0");

    ASSERT_THROW(Script::ScriptProduct_(dates, events), Dal::ScriptError_);
}

TEST(ScriptTest, TestEventWithMacroDuplicated) {
    Vector_<Cell_> dates;
    Vector_<String_> events;

    dates.push_back(Cell_("STRIKE"));
    events.push_back("110.0");

    dates.push_back(Cell_("STRIKE"));
    events.push_back("120.0");

    ASSERT_THROW(Script::ScriptProduct_(dates, events), Dal::ScriptError_);
}

TEST(ScriptTest, TestEventWithSchedule) {
    auto global = XGLOBAL::SetEvaluationDateInScope(Date_(2022, 5, 1));
    Vector_<Cell_> dates;
    Vector_<String_> events;

    dates.push_back(Cell_("START: 2022-05-07 END: 2023-05-07 FREQ: 1m CALENDAR: CN.SSE"));
    events.push_back("STRIKE = 110.0");
    Script::ScriptProduct_ product(dates, events);

    ASSERT_EQ(product.EventDates().size(), 12);
}

TEST(ScriptTest, TestEventWithSchedulePlaceHolder) {
    auto global = XGLOBAL::SetEvaluationDateInScope(Date_(2022, 5, 1));
    Vector_<Cell_> dates;
    Vector_<String_> events;

    dates.push_back(Cell_("START: 2022-05-07 END: 2023-05-07 FREQ: 1m CALENDAR: CN.SSE"));
    events.push_back("acc = DCF(ACT365F, 2022-05-07, PeriodEnd)");
    Script::ScriptProduct_ product(dates, events);

    ASSERT_EQ(product.EventDates().size(), 12);

    Date_ start_date(2022, 5, 7);
    Date_ end_date(2023, 5, 7);
    Holidays_ holidays("CN.SSE");
    DateGeneration_ gen_rule("Forward");
    Handle_<Date::Increment_> tenor = Date::ParseIncrement("1m");
    BizDayConvention_ biz_rule("Unadjusted");

    Vector_<Date_> schedule = Dal::MakeSchedule(start_date,
                                                Cell_(end_date),
                                                holidays,
                                                tenor,
                                                gen_rule,
                                                biz_rule);
    DayBasis_ dc("ACT365F");

    for (auto i = 0; i < product.Events().size(); ++i) {
        const auto& e = product.Events()[i];
        auto expected = dc(schedule[0], schedule[i + 1], nullptr);
        ASSERT_NEAR(dynamic_cast<Script::NodeConst_*>(e[0]->arguments_[1].get())->constVal_, expected, 1e-4);
    }
}
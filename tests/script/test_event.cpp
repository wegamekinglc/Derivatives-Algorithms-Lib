//
// Created by wegam on 2023/5/2.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/script/event.hpp>
#include <dal/time/daybasis.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/script/simulation.hpp>
#include <dal/time/holidays.hpp>
#include <dal/time/schedules.hpp>
#include <dal/time/dateincrement.hpp>
#include <dal/storage/globals.hpp>

using namespace Dal;
using namespace Dal::AAD;
using namespace Dal::Script;

TEST(ScriptTest, TestEventWithMacro) {
    Vector_<Cell_> dates;
    Vector_<String_> events;
    Global::Dates_::SetEvaluationDate(Date_(2023, 1, 1));

    dates.push_back(Cell_("STRIKE"));
    events.push_back("110.0");

    dates.push_back((Cell_(Date_(2023, 12, 1))));
    events.push_back("call PAYS MAX(spot() - STRIKE, 0.0)");

    ScriptProduct_ product(dates, events);

    ASSERT_EQ(product.EventDates().size(), 1);
}

TEST(ScriptTest, TestEventWithMacroNotFirst) {
    Vector_<Cell_> dates;
    Vector_<String_> events;
    Global::Dates_::SetEvaluationDate(Date_(2023, 1, 1));

    dates.push_back((Cell_(Date_(2023, 12, 1))));
    events.push_back("call PAYS MAX(spot() - STRIKE, 0.0)");

    dates.push_back(Cell_("STRIKE"));
    events.push_back("110.0");

    ASSERT_THROW(Script::ScriptProduct_(dates, events), Dal::ScriptError_);
}

TEST(ScriptTest, TestEventWithMacroDuplicated) {
    Vector_<Cell_> dates;
    Vector_<String_> events;
    Global::Dates_::SetEvaluationDate(Date_(2023, 1, 1));

    dates.push_back(Cell_("STRIKE"));
    events.push_back("110.0");

    dates.push_back(Cell_("STRIKE"));
    events.push_back("120.0");

    ASSERT_THROW(Script::ScriptProduct_(dates, events), Dal::ScriptError_);
}

TEST(ScriptTest, TestEventWithSchedule) {
    Global::Dates_::SetEvaluationDate(Date_(2022, 5, 1));
    Vector_<Cell_> dates;
    Vector_<String_> events;

    dates.push_back(Cell_("START: 2022-05-07 END: 2023-05-07 FREQ: 1m CALENDAR: CN.SSE"));
    events.push_back("STRIKE = 110.0");
    ScriptProduct_ product(dates, events);

    ASSERT_EQ(product.EventDates().size(), 12);
}

TEST(ScriptTest, TestEventWithFixingSchedule) {
    Global::Dates_::SetEvaluationDate(Date_(2022, 5, 1));
    Vector_<Cell_> dates;
    Vector_<String_> events;

    dates.push_back(Cell_("START: 2022-05-07 END: 2023-05-07 FREQ: 1m CALENDAR: CN.SSE FIXING: BEGIN"));
    events.push_back("STRIKE = 110.0");
    ScriptProduct_ product(dates, events);
    ASSERT_EQ(product.EventDates()[0], Date_(2022, 5, 7));

    dates.clear();
    events.clear();
    dates.push_back(Cell_("START: 2022-05-07 END: 2023-05-07 FREQ: 1m CALENDAR: CN.SSE FIXING: END"));
    events.push_back("STRIKE = 110.0");
    ScriptProduct_ product2(dates, events);
    ASSERT_EQ(product2.EventDates()[0], Date_(2022, 6, 7));
}

TEST(ScriptTest, TestEventWithSchedulePlaceHolder) {
    Global::Dates_::SetEvaluationDate(Date_(2022, 5, 1));
    Vector_<Cell_> dates;
    Vector_<String_> events;

    dates.push_back(Cell_("START: 2022-05-07 END: 2023-05-07 FREQ: 1m CALENDAR: CN.SSE"));
    events.push_back("acc = DCF(ACT365F, 2022-05-07, PeriodEnd)");
    ScriptProduct_ product(dates, events);

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

TEST(ScriptTest, TestEventWithPastDate) {
    Global::Dates_::SetEvaluationDate(Date_(2023, 8, 28));

    Vector_<Cell_> dates;
    Vector_<String_> events;

    dates.push_back((Cell_(Date_(2022, 12, 1))));
    events.push_back("x = spot()");

    Script::ScriptProduct_ product(dates, events);
    product.PreProcess(false, false);

    ASSERT_EQ(product.VarValues().size(), 1);
    ASSERT_NEAR(product.VarValues()[0], 30.0, 1e-6);
}

TEST(ScriptTest, TestScriptProductWithPastDate) {
    // we set up a touched barrier product
    Vector_<Cell_> dates;
    Vector_<String_> events;

    const Date_ date1 = Date_(2022, 9, 25);
    dates.push_back(Cell_(date1));
    events.push_back(R"(
        IF spot() >= 10 THEN
            alive = 2
        ELSE
            alive = 0
        END
    )");

    const Date_ date2 = Date_(2023, 9, 25);
    String_ event = R"(
        IF alive >= 1:0.001 THEN
            call pays 0
        ELSE
            call pays 1
        END
    )";
    dates.push_back(Cell_(date2));
    events.push_back(event);

    const double spot = 0.001;
    const double vol = 0.20;
    const double rate = 0.0;
    const double div = 0.021;
    const String_ rsg = "sobol";
    const size_t num_paths = 100;

    {
        Global::Dates_::SetEvaluationDate(Date_(2023, 8, 28));
        Script::ScriptProduct_ product(dates, events, "call");
        Handle_<AAD::ModelData_> model_data(new AAD::BSModelData_("bsmodel", spot, vol, rate, div));
        int max_nested = product.PreProcess(false, false);
        SimResults_ results = MCSimulation<double>(product, model_data, num_paths, rsg);
        ASSERT_NEAR(results.aggregated_, 0.0, 1e-8);
    }

    {
        Global::Dates_::SetEvaluationDate(Date_(2021, 8, 28));
        Script::ScriptProduct_ product(dates, events, "call");
        Handle_<AAD::ModelData_> model_data(new AAD::BSModelData_("bsmodel", spot, vol, rate, div));
        int max_nested = product.PreProcess(false, false);
        SimResults_ results = MCSimulation<double>(product, model_data, num_paths, rsg);
        ASSERT_NEAR(results.aggregated_, 100.0, 1);
    }
}
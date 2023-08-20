//
// Created by wegam on 2023/1/28.
//

#include <gtest/gtest.h>
#include <dal/script/visitor/all.hpp>
#include <dal/script/event.hpp>
#include <dal/script/parser.hpp>

using namespace Dal;
using namespace Dal::Script;

TEST(VisitorTest, TestCompile) {
    Global::Dates_().SetEvaluationDate(Date_(2023, 1, 1));
    Vector_<String_> events = {R"(
        x = 4
        y = 1
        IF x >= 2 THEN
            y = 3 + x
        END
    )"};
    Vector_<Cell_> eventDates(1, Cell_(Date_(2023, 1, 28)));

    ScriptProduct_ product(eventDates, events);
    product.PreProcess(false, true);
    product.Compile();

    EvalState_<double> eval_state(Vector_<>(product.VarNames().size(), 0.0));
    Scenario_<double> scenario(1);
    product.EvaluateCompiled(scenario, eval_state);

    ASSERT_DOUBLE_EQ(eval_state.variables_[0], 4);
    ASSERT_DOUBLE_EQ(eval_state.variables_[1], 7);
}

TEST(VisitorTest, TestCompileWithVariable) {
    Global::Dates_().SetEvaluationDate(Date_(2023, 1, 1));
    Vector_<String_> events = {R"(
        IF spot() >= 2:0.1 THEN
            y = 3 + spot()
        END
    )"};
    Vector_<Cell_> eventDates(1, Cell_(Date_(2023, 1, 28)));

    ScriptProduct_ product(eventDates, events);
    product.PreProcess(false, true);
    product.Compile();

    EvalState_<double> eval_state(Vector_<>(product.VarNames().size(), 0.0));
    Scenario_<double> scenario(1);
    scenario[0].spot_ = 4.0;
    product.EvaluateCompiled(scenario, eval_state);

    ASSERT_DOUBLE_EQ(eval_state.variables_[0], 7);
}

TEST(VisitorTest, TestCompileWithSeveralEvents) {
    Global::Dates_().SetEvaluationDate(Date_(2023, 1, 1));
    Vector_<String_> events = {R"(
        x = 4
        y = 1
    )",
    R"(
    IF x >= 2 THEN
        y = 3 + x
    END
    )"};
    Vector_<Cell_> eventDates{Cell_(Date_(2023, 1, 28)), Cell_(Date_(2023, 1, 30))};

    ScriptProduct_ product(eventDates, events);
    product.PreProcess(false, true);
    product.Compile();

    EvalState_<double> eval_state(Vector_<>(product.VarNames().size(), 0.0));
    Scenario_<double> scenario(2);
    product.EvaluateCompiled(scenario, eval_state);

    ASSERT_DOUBLE_EQ(eval_state.variables_[0], 4);
    ASSERT_DOUBLE_EQ(eval_state.variables_[1], 7);
}
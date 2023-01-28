//
// Created by wegam on 2023/1/28.
//

#include <gtest/gtest.h>
#include <dal/script/visitor/all.hpp>
#include <dal/script/event.hpp>
#include <dal/script/parser.hpp>

using namespace Dal;
using namespace Dal::Script;

TEST(CompilerTest, TestCompile) {
    Vector_<String_> events = {R"(
        x = 4
        y = 1
        IF x >= 2 THEN
            y = 3 + x
        ENDIF
    )"};
    Vector_<Date_> eventDates(1, Date_(2023, 1, 28));

    ScriptProduct_ product(eventDates, events);
    product.PreProcess(false, true);
    product.Compile();

    EvalState_<double> eval_state(product.VarNames().size());
    Scenario_<double> scenario(1);
    product.EvaluateCompiled(scenario, eval_state);

    ASSERT_DOUBLE_EQ(eval_state.variables_[0], 4);
    ASSERT_DOUBLE_EQ(eval_state.variables_[1], 7);
}

TEST(CompilerTest, TestCompileWithVariable) {
    Vector_<String_> events = {R"(
        IF spot() >= 2:0.1 THEN
            y = 3 + spot()
        ENDIF
    )"};
    Vector_<Date_> eventDates(1, Date_(2023, 1, 28));

    ScriptProduct_ product(eventDates, events);
    product.PreProcess(false, true);
    product.Compile(true);

    EvalState_<double> eval_state(product.VarNames().size());
    Scenario_<double> scenario(1);
    scenario[0].spot_ = 4.0;
    product.EvaluateCompiled(scenario, eval_state);

    ASSERT_DOUBLE_EQ(eval_state.variables_[0], 7);
}

TEST(CompilerTest, TestCompileWithSeveralEvents) {
    Vector_<String_> events = {R"(
        x = 4
        y = 1
    )",
    R"(
    IF x >= 2 THEN
        y = 3 + x
    ENDIF
    )"};
    Vector_<Date_> eventDates{Date_(2023, 1, 28), Date_(2023, 1, 30)};

    ScriptProduct_ product(eventDates, events);
    product.PreProcess(false, true);
    product.Compile();

    EvalState_<double> eval_state(product.VarNames().size());
    Scenario_<double> scenario(2);
    product.EvaluateCompiled(scenario, eval_state);

    ASSERT_DOUBLE_EQ(eval_state.variables_[0], 4);
    ASSERT_DOUBLE_EQ(eval_state.variables_[1], 7);
}
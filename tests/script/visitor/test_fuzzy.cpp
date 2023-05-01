//
// Created by wegam on 2023/4/1.
//

#include <gtest/gtest.h>
#include <dal/script/visitor/all.hpp>
#include <dal/script/event.hpp>
#include <dal/script/parser.hpp>

using namespace Dal;
using namespace Dal::Script;

TEST(VisitorTest, TestFuzzyContinuous) {
    Global::Dates_().SetEvaluationDate(Date_(2023, 3, 31));
    Vector_<String_> events = {R"(
        IF spot() > 100:0.01 THEN
            x = 1
        ELSE
            x = 0
        ENDIF
    )"};
    Vector_<Cell_> eventDates(1, Cell_(Date_(2023, 4, 1)));

    ScriptProduct_ product(eventDates, events);
    int maxNestedIfs = product.PreProcess(false, false);

    auto eval = product.BuildFuzzyEvaluator<double>(maxNestedIfs, 0.01);

    Scenario_<> path;
    AAD::AllocatePath(product.DefLine(), path);
    path[0].spot_ = 110.0;
    product.Evaluate(path, eval);
    ASSERT_NEAR(eval.VarVals()[0], 1.0, 1e-8);

    path[0].spot_ = 90.0;
    product.Evaluate(path, eval);
    ASSERT_NEAR(eval.VarVals()[0], 0.0, 1e-8);

    path[0].spot_ = 100.0;
    product.Evaluate(path, eval);
    ASSERT_NEAR(eval.VarVals()[0], 0.5, 1e-8);

    path[0].spot_ = 100.0025;
    product.Evaluate(path, eval);
    ASSERT_NEAR(eval.VarVals()[0], 0.75, 1e-8);
}
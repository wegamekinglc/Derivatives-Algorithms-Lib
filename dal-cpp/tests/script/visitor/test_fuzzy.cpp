//
// Created by wegam on 2023/4/1.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/script/visitor/all.hpp>
#include <dal/script/event.hpp>
#include <dal/script/parser.hpp>
#include <dal/storage/globals.hpp>

using namespace Dal;
using namespace Dal::Script;

TEST(ScriptTest, TestCSpr) {
    double eps = 1.0;
    ASSERT_NEAR(CSpr(0.0, eps), 0.5, 1e-4);
    ASSERT_NEAR(CSpr(0.25, eps), 0.75, 1e-4);
    ASSERT_NEAR(CSpr(-0.25, eps), 0.25, 1e-4);
}

TEST(ScriptTest, TestCSprWithLbAndUb) {
    double lb = -0.5;
    double ub = 1.0;
    ASSERT_NEAR(CSpr(0.0, lb, ub), 0.3333, 1e-4);
    ASSERT_NEAR(CSpr(0.5, lb, ub), 0.6666, 1e-4);
    ASSERT_NEAR(CSpr(-0.5, lb, ub), 0.0, 1e-4);
}

TEST(ScriptTest, TestBFly) {
    double eps = 1.0;
    ASSERT_NEAR(BFly(0.0, eps), 1.0, 1e-4);
    ASSERT_NEAR(BFly(0.25, eps), 0.5, 1e-4);
    ASSERT_NEAR(BFly(-0.25, eps), 0.5, 1e-4);
}

TEST(ScriptTest, TestBFlyWithLbAndUb) {
    double lb = -0.5;
    double ub = 1.0;
    ASSERT_NEAR(BFly(0.0, lb, ub), 1.0, 1e-4);
    ASSERT_NEAR(BFly(0.5, lb, ub), 0.5, 1e-4);
    ASSERT_NEAR(BFly(-0.5, lb, ub), 0.0, 1e-4);
}

TEST(ScriptTest, TestFuzzyContinuous) {
    Global::Dates_::SetEvaluationDate(Date_(2023, 3, 31));
    Vector_<String_> events = {R"(
        IF spot() > 100:0.01 THEN
            x = 1
        ELSE
            x = 0
        END
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
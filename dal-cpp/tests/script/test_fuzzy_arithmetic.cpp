//
// Created by Codex on 2026/9/14.
//

#include <gtest/gtest.h>

#include <cmath>

#include <dal/model/blackscholes.hpp>
#include <dal/script/simulation.hpp>
#include <dal/storage/globals.hpp>

using namespace Dal;
using namespace Dal::Script;

namespace {
    Handle_<MarketFixingSnapshot_> ArithmeticHistory(double fixing) {
        return Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_({{"EQ[DAL196_TEST]", {{DateTime_(Date_(2026, 9, 11), 0.0), fixing}}}}));
    }

    double UnoptimizedFuzzyPrice(const ScriptProductData_& product, double fixing) {
        const auto raw = PrepareScript(product, ScriptValuationSettings_(), ArithmeticHistory(fixing));
        IFProcessor_ ifs;
        for (const auto& statement : raw.Product().Events()[0])
            statement->Accept(ifs);
        FuzzyEvaluator_<double> reference(Vector_<>(raw.Product().VarNames().size(), 0.0), raw.Product().BuildEvaluator<double>().ConstVarVals(),
                                          ifs.MaxNestedIFs(), 0.01);
        AAD::Scenario_<double> path(1);
        path[0].numeraire_ = 1.0;
        reference.SetScenario(&path);
        reference.SetObservations(&raw.Plan());
        reference.SetCurEvt(0);
        for (const auto& statement : raw.Product().Events()[0])
            statement->Accept(reference);
        return reference.VarVals()[raw.PayOffIdx()];
    }

    double PreparedFuzzyPrice(const ScriptProductData_& product, double fixing, bool compiled) {
        MonteCarloSettings_ settings;
        settings.enableAad_ = true;
        settings.compiled_ = compiled;
        AAD::BlackScholes_<double> model(100.0, 0.2, 0.0, 0.0);
        const auto prepared = PrepareScript(product, &model, {}, settings, ArithmeticHistory(fixing));
        AAD::Scenario_<double> path;
        AAD::AllocatePath(prepared.DefLine(), path);
        AAD::InitializePath(path);
        model.GeneratePath(Vector_<>(model.SimDim(), 0.0), &path);
        if (compiled) {
            auto state = prepared.BuildEvalState<double>();
            prepared.Compile(true).Evaluate(path, state);
            return state.VarVals()[prepared.PayOffIdx()];
        }
        auto state = prepared.BuildFuzzyEvaluator<double>(0, settings.smooth_);
        prepared.Evaluate(path, state);
        return state.VarVals()[prepared.PayOffIdx()];
    }

    void CheckFuzzyArithmetic(const String_& expression, double fixing, double expected, double derivative) {
        const auto date = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
        const ScriptProductData_ product("", {Cell_("SCALE"), Cell_(Date_(2026, 9, 22))},
                                         {"2", "x = " + expression + " IF x > 0:0.2 THEN pay PAYS x ELSE pay PAYS 0 END"});
        ASSERT_NEAR(UnoptimizedFuzzyPrice(product, fixing), expected, 1.0e-12);
        const Handle_<ModelData_> model(new BSModelData_("", 100.0, 0.2, 0.0, 0.0));
        for (bool compiled : {false, true}) {
            SCOPED_TRACE(::testing::Message() << "compiled=" << compiled << " expression=" << expression);
            ASSERT_NEAR(PreparedFuzzyPrice(product, fixing, compiled), expected, 1.0e-12);
            MonteCarloSettings_ settings;
            settings.compiled_ = compiled;
            const auto result = MCSimulation<AAD::Number_>(product, model, 1, {}, settings, ArithmeticHistory(fixing));
            ASSERT_NEAR(result.aggregated_, expected, 1.0e-12);
            ASSERT_NEAR(result["SCALE"], derivative, 1.0e-10);
            ASSERT_NEAR(result["rate"], -10.0 / DAYS_PER_YEAR * expected, 1.0e-10);
            ASSERT_NEAR(result["spot"], 0.0, 1.0e-10);
            ASSERT_NEAR(result["vol"], 0.0, 1.0e-10);
        }
    }
} // namespace

TEST(ScriptFuzzyArithmeticTest, TestSignedSmallNonzeroDivisors) {
    ASSERT_NO_FATAL_FAILURE(CheckFuzzyArithmetic("SCALE * FIX(EQ[DAL196_TEST], 2026-09-11) / 0.00000000000001", 1.0e-14, 2.0, 1.0));
    ASSERT_NO_FATAL_FAILURE(CheckFuzzyArithmetic("SCALE * FIX(EQ[DAL196_TEST], 2026-09-11) / (-0.00000000000001)", -1.0e-14, 2.0, 1.0));
}

TEST(ScriptFuzzyArithmeticTest, TestSmallKnownDivisorsAroundDomainTolerance) {
    const String_ fixing = "FIX(EQ[DAL196_TEST], 2026-09-11)";
    for (double magnitude : {1.0e-16, 1.0e-14, std::nextafter(2.0e-14, 0.0), 2.0e-14, std::nextafter(2.0e-14, 1.0), 1.0e-12}) {
        for (double sign : {-1.0, 1.0}) {
            SCOPED_TRACE(::testing::Message() << "fixing=" << sign * magnitude);
            ASSERT_NO_FATAL_FAILURE(CheckFuzzyArithmetic("SCALE * (" + fixing + " / " + fixing + ")", sign * magnitude, 2.0, 1.0));
        }
    }
}

TEST(ScriptFuzzyArithmeticTest, TestComputedSmallNonzeroDivisors) {
    const String_ fixing = "FIX(EQ[DAL196_TEST], 2026-09-11)";
    const Vector_<String_> divisors{
        "(" + fixing + " * 1)",     "(" + fixing + " + " + fixing + " - " + fixing + ")", "(SQRT(" + fixing + ") ^ 2)",
        "EXP(LOG(" + fixing + "))", "MAX(" + fixing + ", " + fixing + " * 0.5)",          "MIN(" + fixing + ", " + fixing + " * 2)"};
    for (const auto& divisor : divisors)
        ASSERT_NO_FATAL_FAILURE(CheckFuzzyArithmetic("SCALE * " + fixing + " / " + divisor, 1.0e-14, 2.0, 1.0));
    ASSERT_NO_FATAL_FAILURE(CheckFuzzyArithmetic("SCALE / (" + fixing + " * 100000000000000)", 1.0e-14, 2.0, 1.0));
    ASSERT_NO_FATAL_FAILURE(CheckFuzzyArithmetic("SCALE * 0.00000000000001 / SQRT(" + fixing + ")", 1.0e-28, 2.0, 1.0));
    ASSERT_NO_FATAL_FAILURE(CheckFuzzyArithmetic("2 * " + fixing + " / (SCALE * " + fixing + ")", 1.0e-14, 1.0, -0.5));
}

TEST(ScriptFuzzyArithmeticTest, TestSmallDivisorsInsideSmoothingBand) {
    // x=0.05, weight=0.75, d(x*weight)/d_SCALE=(0.5+2*x/0.2)*0.025.
    ASSERT_NO_FATAL_FAILURE(CheckFuzzyArithmetic("SCALE * FIX(EQ[DAL196_TEST], 2026-09-11) / 0.00000000000001", 2.5e-16, 0.0375, 0.025));
    ASSERT_NO_FATAL_FAILURE(CheckFuzzyArithmetic("SCALE * FIX(EQ[DAL196_TEST], 2026-09-11) / (-0.00000000000001)", -2.5e-16, 0.0375, 0.025));
}

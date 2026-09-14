//
// Created by Codex on 2026/9/14.
//

#include <gtest/gtest.h>

#include <cmath>
#include <iomanip>
#include <limits>

#include <dal/model/blackscholes.hpp>
#include <dal/script/simulation.hpp>
#include <dal/storage/globals.hpp>

using namespace Dal;
using namespace Dal::Script;

namespace {
    Handle_<MarketFixingSnapshot_> History(double fixing) {
        return Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_({{"EQ[DAL196_TEST]", {{DateTime_(Date_(2026, 9, 11), 0.0), fixing}}}}));
    }

    Handle_<ModelData_> Model() { return Handle_<ModelData_>(new BSModelData_("", 100.0, 0.2, 0.0, 0.0)); }

    void CheckHardReference(const ScriptProductData_& product, double fixing, double expected) {
        const auto prepared = PrepareScript(product, ScriptValuationSettings_(), History(fixing));
        Evaluator_<double> reference(Vector_<>(prepared.Product().VarNames().size(), 0.0),
                                     prepared.Product().BuildEvaluator<double>().ConstVarVals());
        AAD::Scenario_<double> path(1);
        path[0].numeraire_ = 1.0;
        reference.SetScenario(&path);
        reference.SetObservations(&prepared.Plan());
        reference.SetCurEvt(0);
        for (const auto& statement : prepared.Product().Events()[0])
            statement->Accept(reference);
        ASSERT_DOUBLE_EQ(reference.VarVals()[prepared.PayOffIdx()], expected);
    }

    void CheckExactProduct(const ScriptProductData_& product, double fixing, double expected) {
        ASSERT_NO_FATAL_FAILURE(CheckHardReference(product, fixing, expected));
        for (bool compiled : {false, true}) {
            SCOPED_TRACE(::testing::Message() << "compiled=" << compiled << " fixing=" << std::setprecision(17) << fixing);
            MonteCarloSettings_ simulation;
            simulation.compiled_ = compiled;
            const auto result = MCSimulation<double>(product, Model(), 1, {}, simulation, History(fixing));
            ASSERT_DOUBLE_EQ(result.aggregated_, expected);
        }
    }

    void CheckExactBoundary(const String_& comparison, double fixing, double expected, const String_& threshold = "80") {
        const auto date = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
        SCOPED_TRACE(::testing::Message() << "comparison=" << comparison << " threshold=" << threshold);
        const ScriptProductData_ product(
            "", {Cell_(Date_(2026, 9, 22))},
            {"IF FIX(EQ[DAL196_TEST], 2026-09-11) " + comparison + " " + threshold + " THEN pay PAYS 160 ELSE pay PAYS 0 END"});
        ASSERT_NO_FATAL_FAILURE(CheckExactProduct(product, fixing, expected));
    }

    void CheckAdjacentComparisons(double fixing, double threshold, const String_& thresholdText) {
        ASSERT_NO_FATAL_FAILURE(CheckExactBoundary(">", fixing, fixing > threshold ? 160.0 : 0.0, thresholdText));
        ASSERT_NO_FATAL_FAILURE(CheckExactBoundary(">=", fixing, fixing >= threshold ? 160.0 : 0.0, thresholdText));
        ASSERT_NO_FATAL_FAILURE(CheckExactBoundary("<", fixing, fixing < threshold ? 160.0 : 0.0, thresholdText));
        ASSERT_NO_FATAL_FAILURE(CheckExactBoundary("<=", fixing, fixing <= threshold ? 160.0 : 0.0, thresholdText));
        ASSERT_NO_FATAL_FAILURE(CheckExactBoundary("=", fixing, fixing == threshold ? 160.0 : 0.0, thresholdText));
        ASSERT_NO_FATAL_FAILURE(CheckExactBoundary("!=", fixing, fixing != threshold ? 160.0 : 0.0, thresholdText));
    }

    void CheckParameterFactor(double fixing, const String_& scaleText, double scale) {
        const ScriptProductData_ product("", {Cell_("SCALE"), Cell_(Date_(2026, 9, 22))},
                                         {scaleText, "x = SCALE * FIX(EQ[DAL196_TEST], 2026-09-11) IF x > 0 THEN pay PAYS x ELSE pay PAYS 0 END"});
        const double expected = std::max(scale * fixing, 0.0);
        ASSERT_NO_FATAL_FAILURE(CheckExactProduct(product, fixing, expected));
        for (bool compiled : {false, true}) {
            SCOPED_TRACE(compiled);
            MonteCarloSettings_ simulation;
            simulation.compiled_ = compiled;
            const auto result = MCSimulation<AAD::Number_>(product, Model(), 1, {}, simulation, History(fixing));
            ASSERT_NEAR(result.aggregated_, expected, 1.0e-12);
            ASSERT_NEAR(result["SCALE"], scale * fixing > 0.0 ? fixing : 0.0, std::abs(fixing) * 1.0e-12);
        }
    }
} // namespace

TEST(ScriptExactFoldingTest, TestStrictPositiveUlp) {
    const double fixing = std::nextafter(80.0, std::numeric_limits<double>::infinity());
    ASSERT_GT(fixing, 80.0);
    ASSERT_NO_FATAL_FAILURE(CheckExactBoundary(">", fixing, 160.0));
}

TEST(ScriptExactFoldingTest, TestNonStrictNegativeUlp) {
    const double fixing = std::nextafter(80.0, -std::numeric_limits<double>::infinity());
    ASSERT_LT(fixing, 80.0);
    ASSERT_NO_FATAL_FAILURE(CheckExactBoundary(">=", fixing, 0.0));
}

TEST(ScriptExactFoldingTest, TestEqualityPositiveUlp) {
    const double fixing = std::nextafter(80.0, std::numeric_limits<double>::infinity());
    ASSERT_NE(fixing, 80.0);
    ASSERT_NO_FATAL_FAILURE(CheckExactBoundary("=", fixing, 0.0));
}

TEST(ScriptExactFoldingTest, TestSmallNonzeroFixingRetainsParameterDependency) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    const ScriptProductData_ product("", {Cell_("SCALE"), Cell_(Date_(2026, 9, 22))},
                                     {"100000000000000", "IF SCALE * FIX(EQ[DAL196_TEST], 2026-09-11) > 0 THEN pay PAYS 160 ELSE pay PAYS 0 END"});
    ASSERT_DOUBLE_EQ(1.0e14 * 1.0e-14, 1.0);
    ASSERT_NO_FATAL_FAILURE(CheckExactProduct(product, 1.0e-14, 160.0));
    for (bool compiled : {false, true}) {
        MonteCarloSettings_ simulation;
        simulation.compiled_ = compiled;
        const auto result = MCSimulation<AAD::Number_>(product, Model(), 1, {}, simulation, History(1.0e-14));
        ASSERT_DOUBLE_EQ(result.aggregated_, 160.0);
    }
}

TEST(ScriptExactFoldingTest, TestAdjacentSignAndEquality) {
    for (double fixing : {std::nextafter(80.0, 0.0), 80.0, std::nextafter(80.0, 160.0)})
        ASSERT_NO_FATAL_FAILURE(CheckAdjacentComparisons(fixing, 80.0, "80"));
    for (double fixing : {-1.0e-14, -std::numeric_limits<double>::denorm_min(), -0.0, 0.0, std::numeric_limits<double>::denorm_min(), 1.0e-14})
        ASSERT_NO_FATAL_FAILURE(CheckAdjacentComparisons(fixing, 0.0, "0"));
}

TEST(ScriptExactFoldingTest, TestNonzeroParameterFactorsAndRisks) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    for (double fixing : {-1.0e-14, 1.0e-14}) {
        ASSERT_NO_FATAL_FAILURE(CheckParameterFactor(fixing, "100000000000000", 1.0e14));
        ASSERT_NO_FATAL_FAILURE(CheckParameterFactor(fixing, "-100000000000000", -1.0e14));
    }
    for (double fixing : {-1.0e14, 1.0e14}) {
        ASSERT_NO_FATAL_FAILURE(CheckParameterFactor(fixing, "0.00000000000001", 1.0e-14));
        ASSERT_NO_FATAL_FAILURE(CheckParameterFactor(fixing, "-0.00000000000001", -1.0e-14));
    }
}

TEST(ScriptExactFoldingTest, TestSmallNonzeroDivisorInFutureState) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    const ScriptProductData_ product("", {Cell_(Date_(2026, 9, 22))},
                                     {"x = FIX(EQ[DAL196_TEST], 2026-09-11) / 0.00000000000001 IF x > 0 THEN pay PAYS 160 ELSE pay PAYS 0 END"});
    ASSERT_NO_FATAL_FAILURE(CheckExactProduct(product, 1.0e-14, 160.0));
    ASSERT_NO_FATAL_FAILURE(CheckExactProduct(product, -1.0e-14, 0.0));
}

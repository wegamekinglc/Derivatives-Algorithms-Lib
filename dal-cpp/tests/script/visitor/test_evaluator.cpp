//
// Created by wegam on 2022/6/3.
//

#include <gtest/gtest.h>
#include <map>
#include <dal/platform/platform.hpp>
#include <dal/script/node.hpp>
#include <dal/script/parser.hpp>
#include <dal/script/visitor/all.hpp>
#include <dal/script/visitor/evaluator.hpp>

using namespace Dal;
using namespace Dal::Script;

namespace {
    std::map<String_, double> ParseIndexEval(const String_& src) {
        Parser_ parser;
        auto event = parser.Parse(src);
        VarIndexer_ indexer;
        for (auto& stat : event)
            stat->Accept(indexer);
        Evaluator_<double> eval(Vector_<>(indexer.VarNames().size(), 0.0));
        for (auto& stat : event)
            stat->Accept(eval);
        std::map<String_, double> out;
        auto names = indexer.VarNames();
        auto vals = eval.VarVals();
        for (size_t i = 0; i < names.size(); ++i)
            out[names[i]] = vals[i];
        return out;
    }
} // namespace


TEST(ScriptTest, TestEvaluator) {
    Expression_ const1 = MakeBaseNode<NodeConst_>(20.0);
    Expression_ const2 = MakeBaseNode<NodeConst_>(30.0);

    auto plusExpr = MakeBaseBinary<NodeAdd_>(const1, const2);

    Expression_ var = MakeBaseNode<NodeVar_>("x");
    auto assignExpr = MakeBinary<NodeAssign_>(var, plusExpr);

    VarIndexer_ visitor1;
    Evaluator_<double> visitor2(Vector_<>(1, 0.0));
    assignExpr->Accept(visitor1);
    assignExpr->Accept(visitor2);
    ASSERT_DOUBLE_EQ(visitor2.VarVals()[0], 50);
}

TEST(ScriptTest, TestEvaluatorWithSqrt) {
    auto const1 = MakeBaseNode<NodeConst_>(2.0);
    auto expExpr = MakeBaseNode<NodeSqrt_>();
    Vector_<Expression_> args;
    args.push_back(std::move(const1));
    expExpr->arguments_ = std::move(args);
    auto var = MakeBaseNode<NodeVar_>("x");
    auto assignExpr = MakeBinary<NodeAssign_>(var, expExpr);

    Evaluator_<double> visitor2(Vector_<>(1, 0.0));
    VarIndexer_ visitor1;
    assignExpr->Accept(visitor1);
    assignExpr->Accept(visitor2);
    ASSERT_DOUBLE_EQ(visitor2.VarVals()[0], 1.4142135623730951);
}

TEST(ScriptTest, TestEvaluatorWithLog) {
    auto const1 = MakeBaseNode<NodeConst_>(2.0);
    auto expExpr = MakeBaseNode<NodeLog_>();
    Vector_<Expression_> args;
    args.push_back(std::move(const1));
    expExpr->arguments_ = std::move(args);
    auto var = MakeBaseNode<NodeVar_>("x");
    auto assignExpr = MakeBinary<NodeAssign_>(var, expExpr);

    Evaluator_<double> visitor2(Vector_<>(1, 0.0));
    VarIndexer_ visitor1;
    assignExpr->Accept(visitor1);
    assignExpr->Accept(visitor2);
    ASSERT_DOUBLE_EQ(visitor2.VarVals()[0], 0.69314718055994529);
}

TEST(ScriptTest, TestEvaluatorWithExp) {
    auto const1 = MakeBaseNode<NodeConst_>(2.0);
    auto expExpr = MakeBaseNode<NodeExp_>();
    Vector_<Expression_> args;
    args.push_back(std::move(const1));
    expExpr->arguments_ = std::move(args);
    auto var = MakeBaseNode<NodeVar_>("x");
    auto assignExpr = MakeBinary<NodeAssign_>(var, expExpr);

    Evaluator_<double> visitor2(Vector_<>(1, 0.0));
    VarIndexer_ visitor1;
    assignExpr->Accept(visitor1);
    assignExpr->Accept(visitor2);
    ASSERT_DOUBLE_EQ(visitor2.VarVals()[0], 7.3890560989306504);
}

TEST(ScriptTest, TestEvaluatorAdd) {
    auto vars = ParseIndexEval("x = 2 + 3");
    ASSERT_NEAR(vars["x"], 5.0, 1e-10);
}

TEST(ScriptTest, TestEvaluatorSub) {
    auto vars = ParseIndexEval("x = 10 - 4");
    ASSERT_NEAR(vars["x"], 6.0, 1e-10);
}

TEST(ScriptTest, TestEvaluatorMul) {
    auto vars = ParseIndexEval("x = 6 * 7");
    ASSERT_NEAR(vars["x"], 42.0, 1e-10);
}

TEST(ScriptTest, TestEvaluatorDiv) {
    auto vars = ParseIndexEval("x = 20 / 8");
    ASSERT_NEAR(vars["x"], 2.5, 1e-10);
}

TEST(ScriptTest, TestEvaluatorPrecedence) {
    auto vars = ParseIndexEval("x = 2 + 3 * 4");
    ASSERT_NEAR(vars["x"], 14.0, 1e-10);
}

TEST(ScriptTest, TestEvaluatorParentheses) {
    auto vars = ParseIndexEval("x = (2 + 3) * 4");
    ASSERT_NEAR(vars["x"], 20.0, 1e-10);
}

TEST(ScriptTest, TestEvaluatorPow) {
    auto vars = ParseIndexEval("x = 2 ^ 10");
    ASSERT_NEAR(vars["x"], 1024.0, 1e-10);
}


TEST(ScriptTest, TestEvaluatorMin) {
    auto vars = ParseIndexEval("x = MIN(3, 7)");
    ASSERT_NEAR(vars["x"], 3.0, 1e-10);
}

TEST(ScriptTest, TestEvaluatorMax) {
    auto vars = ParseIndexEval("x = MAX(3, 7)");
    ASSERT_NEAR(vars["x"], 7.0, 1e-10);
}

TEST(ScriptTest, TestEvaluatorMaxThreeArgs) {
    auto vars = ParseIndexEval("x = MAX(3, 7, 5)");
    ASSERT_NEAR(vars["x"], 7.0, 1e-10);
}

TEST(ScriptTest, TestEvaluatorUnaryMinus) {
    auto vars = ParseIndexEval("x = -5");
    ASSERT_NEAR(vars["x"], -5.0, 1e-10);
}

TEST(ScriptTest, TestEvaluatorAssignReassign) {
    auto vars = ParseIndexEval(R"(
        x = 2
        x = x + 10
    )");
    ASSERT_NEAR(vars["x"], 12.0, 1e-10);
}

TEST(ScriptTest, TestEvaluatorVariableReference) {
    auto vars = ParseIndexEval(R"(
        x = 4
        y = x * 3
    )");
    ASSERT_NEAR(vars["x"], 4.0, 1e-10);
    ASSERT_NEAR(vars["y"], 12.0, 1e-10);
}


TEST(ScriptTest, TestEvaluatorIfTrueBranch) {
    auto vars = ParseIndexEval(R"(
        x = 5
        y = 0
        IF x >= 2 THEN
            y = 1
        ELSE
            y = 2
        END
    )");
    ASSERT_NEAR(vars["y"], 1.0, 1e-10);
}

TEST(ScriptTest, TestEvaluatorIfFalseBranch) {
    auto vars = ParseIndexEval(R"(
        x = 1
        y = 0
        IF x >= 2 THEN
            y = 1
        ELSE
            y = 2
        END
    )");
    ASSERT_NEAR(vars["y"], 2.0, 1e-10);
}

TEST(ScriptTest, TestEvaluatorIfNoElseSkips) {
    auto vars = ParseIndexEval(R"(
        x = 1
        y = 9
        IF x >= 2 THEN
            y = 1
        END
    )");
    ASSERT_NEAR(vars["y"], 9.0, 1e-10);
}

TEST(ScriptTest, TestEvaluatorConditionAnd) {
    auto vars = ParseIndexEval(R"(
        x = 3
        y = 0
        IF x > 2 AND x < 5 THEN
            y = 1
        END
    )");
    ASSERT_NEAR(vars["y"], 1.0, 1e-10);
}


TEST(ScriptTest, TestEvaluatorConditionOr) {
    auto vars = ParseIndexEval(R"(
        x = 10
        y = 0
        IF x < 2 OR x > 5 THEN
            y = 1
        END
    )");
    ASSERT_NEAR(vars["y"], 1.0, 1e-10);
}

TEST(ScriptTest, TestEvaluatorConditionNotEqual) {
    auto vars = ParseIndexEval(R"(
        x = 3
        y = 0
        IF x != 2 THEN
            y = 1
        END
    )");
    ASSERT_NEAR(vars["y"], 1.0, 1e-10);
}

TEST(ScriptTest, TestEvaluatorConditionEqualFalse) {
    auto vars = ParseIndexEval(R"(
        x = 3
        y = 0
        IF x = 2 THEN
            y = 1
        ELSE
            y = 2
        END
    )");
    ASSERT_NEAR(vars["y"], 2.0, 1e-10);
}

TEST(ScriptTest, TestEvaluatorConditionLess) {
    auto vars = ParseIndexEval(R"(
        x = 1
        y = 0
        IF x < 2 THEN
            y = 1
        ELSE
            y = 2
        END
    )");
    ASSERT_NEAR(vars["y"], 1.0, 1e-10);
}

TEST(ScriptTest, TestEvaluatorMoveConstructorPreservesVariablesInit) {
    // Regression: EvaluatorBase_ move ctor previously omitted variablesInit_, so a
    // moved-into evaluator's Init() reset all variables to zero instead of their
    // initial values. variablesInit_ is the data Init() restores from.
    const Vector_<> initialValues = {10.0, 20.0};

    // Evaluate an assignment after the move so variables_ differ from initialValues,
    // then confirm Init() restores them (proving variablesInit_ survived the move).
    Parser_ parser;
    auto event = parser.Parse("x = 999\ny = 999\n");
    VarIndexer_ indexer;
    for (auto& stat : event)
        stat->Accept(indexer);

    Evaluator_<double> src(initialValues);
    Evaluator_<double> dst(std::move(src));
    ASSERT_EQ(dst.VarVals().size(), 2u);

    for (auto& stat : event)
        stat->Accept(dst);
    ASSERT_NEAR(dst.VarVals()[0], 999.0, 1e-10);
    ASSERT_NEAR(dst.VarVals()[1], 999.0, 1e-10);

    // Init() reads variablesInit_; if the move dropped it, variables stay at 0
    dst.Init();
    ASSERT_NEAR(dst.VarVals()[0], 10.0, 1e-10);
    ASSERT_NEAR(dst.VarVals()[1], 20.0, 1e-10);
}

TEST(ScriptTest, TestEvaluatorCopyAssignmentPreservesCurrentEvent) {
    // Regression: EvaluatorBase_ copy/move assignment copied scenario_ but not
    // curEvt_, so an assigned evaluator read spot()/numeraire_ at a stale event
    // index (default-constructed curEvt_ is size_t(-1), an out-of-bounds read).
    Parser_ parser;
    auto event = parser.Parse("x = spot()");
    VarIndexer_ indexer;
    for (auto& stat : event)
        stat->Accept(indexer);
    const Vector_<> vars(indexer.VarNames().size(), 0.0);

    AAD::Scenario_<double> scenario(2);
    scenario[0].spot_ = 100.0;
    scenario[0].numeraire_ = 1.0;
    scenario[1].spot_ = 250.0;
    scenario[1].numeraire_ = 1.0;

    Evaluator_<double> src(vars);
    src.SetScenario(&scenario);
    src.SetCurEvt(1);

    Evaluator_<double> copyDst(vars);
    copyDst = src;
    for (auto& stat : event)
        stat->Accept(copyDst);
    ASSERT_NEAR(copyDst.VarVals()[0], 250.0, 1e-10);

    Evaluator_<double> moveDst(vars);
    moveDst = std::move(src);
    for (auto& stat : event)
        stat->Accept(moveDst);
    ASSERT_NEAR(moveDst.VarVals()[0], 250.0, 1e-10);
}

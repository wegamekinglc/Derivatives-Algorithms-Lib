//
// Created by wegam on 2023/1/28.
//

#include <gtest/gtest.h>

#include <string>
#include <utility>

#include <dal/platform/platform.hpp>
#include <dal/script/visitor/all.hpp>
#include <dal/script/event.hpp>
#include <dal/script/parser.hpp>
#include <dal/storage/globals.hpp>

using namespace Dal;
using namespace Dal::Script;

TEST(ScriptTest, TestCompile) {
    Global::Dates_::SetEvaluationDate(Date_(2023, 1, 1));
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
    const ScriptCompiled_ compiled = product.Compile();

    EvalState_<double> eval_state(Vector_<>(product.VarNames().size(), 0.0));
    Scenario_<double> scenario(1);
    compiled.Evaluate(scenario, eval_state);

    ASSERT_DOUBLE_EQ(eval_state.variables_[0], 4);
    ASSERT_DOUBLE_EQ(eval_state.variables_[1], 7);
}

TEST(ScriptTest, TestCompileWithVariable) {
    Global::Dates_::SetEvaluationDate(Date_(2023, 1, 1));
    Vector_<String_> events = {R"(
        IF spot() >= 2:0.1 THEN
            y = 3 + spot()
        END
    )"};
    Vector_<Cell_> eventDates(1, Cell_(Date_(2023, 1, 28)));

    ScriptProduct_ product(eventDates, events);
    product.PreProcess(false, true);
    const ScriptCompiled_ compiled = product.Compile();

    EvalState_<double> eval_state(Vector_<>(product.VarNames().size(), 0.0));
    Scenario_<double> scenario(1);
    scenario[0].spot_ = 4.0;
    compiled.Evaluate(scenario, eval_state);

    ASSERT_DOUBLE_EQ(eval_state.variables_[0], 7);
}

TEST(ScriptTest, TestCompileWithSeveralEvents) {
    Global::Dates_::SetEvaluationDate(Date_(2023, 1, 1));
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
    const ScriptCompiled_ compiled = product.Compile();

    EvalState_<double> eval_state(Vector_<>(product.VarNames().size(), 0.0));
    Scenario_<double> scenario(2);
    compiled.Evaluate(scenario, eval_state);

    ASSERT_DOUBLE_EQ(eval_state.variables_[0], 4);
    ASSERT_DOUBLE_EQ(eval_state.variables_[1], 7);
}

namespace {
    // Parse + index + const-process a source snippet, then compile it. Mirrors the
    // pipeline ScriptProduct_::PreProcess runs before Compile(), without schedules
    // or macro handling.
    std::pair<Vector_<int>, Vector_<double>> CompileSource(const String_& src) {
        Parser_ parser;
        auto statements = parser.Parse(src);
        VarIndexer_ indexer;
        for (auto& stat : statements)
            stat->Accept(indexer);
        ConstProcessor_ constProc(indexer.VarNames().size());
        for (auto& stat : statements)
            stat->Accept(constProc);
        Compiler_ compiler;
        for (auto& stat : statements)
            stat->Accept(compiler);
        return {compiler.NodeStream(), compiler.ConstStream()};
    }

    // Per-path parity between the tree-walk evaluator and the compiled evaluator.
    void AssertCompiledParity(const ScriptProduct_& product,
                              double spot,
                              double numeraire,
                              double tol = 1e-10) {
        Scenario_<double> scenario(product.EventDates().size());
        for (auto& sample : scenario) {
            sample.spot_ = spot;
            sample.numeraire_ = numeraire;
        }

        Evaluator_<double> treeEval = product.BuildEvaluator<double>();
        product.Evaluate(scenario, treeEval);

        const ScriptCompiled_ compiled = product.Compile();
        EvalState_<double> state = product.BuildEvalState<double>();
        compiled.Evaluate(scenario, state);

        ASSERT_NEAR(state.VarVals()[product.PayOffIdx()],
                    treeEval.VarVals()[product.PayOffIdx()],
                    tol);
    }
} // namespace

TEST(CompilerTest, TestConstAssignmentEmitsAssignConst) {
    const auto [stream, consts] = CompileSource("x = 3");
    ASSERT_EQ(stream, Vector_<int>({AssignConst, 0, 0}));
    ASSERT_EQ(consts, Vector_<double>({3.0}));
}

TEST(CompilerTest, TestConstExpressionIsFullyFolded) {
    const auto [stream, consts] = CompileSource("x = 2 + 3 * 4");
    ASSERT_EQ(stream, Vector_<int>({AssignConst, 0, 0}));
    ASSERT_EQ(consts, Vector_<double>({14.0}));
}

TEST(CompilerTest, TestSpotAssignmentEmitsSpotAssign) {
    const auto [stream, consts] = CompileSource("x = spot()");
    ASSERT_EQ(stream, Vector_<int>({Spot, Assign, 0}));
    ASSERT_EQ(consts.size(), 0u);
}

TEST(CompilerTest, TestBinopWithConstOperandEmitsConstOpcodes) {
    {
        const auto [stream, consts] = CompileSource("x = spot() + 2");
        ASSERT_EQ(stream, Vector_<int>({Spot, AddConst, 0, Assign, 0}));
        ASSERT_EQ(consts, Vector_<double>({2.0}));
    }
    {
        const auto [stream, consts] = CompileSource("x = 2 + spot()");
        ASSERT_EQ(stream, Vector_<int>({Spot, AddConst, 0, Assign, 0}));
        ASSERT_EQ(consts, Vector_<double>({2.0}));
    }
    {
        const auto [stream, consts] = CompileSource("x = spot() - 2");
        ASSERT_EQ(stream, Vector_<int>({Spot, SubConst, 0, Assign, 0}));
        ASSERT_EQ(consts, Vector_<double>({2.0}));
    }
    {
        const auto [stream, consts] = CompileSource("x = 2 - spot()");
        ASSERT_EQ(stream, Vector_<int>({Spot, ConstSub, 0, Assign, 0}));
        ASSERT_EQ(consts, Vector_<double>({2.0}));
    }
    {
        const auto [stream, consts] = CompileSource("x = spot() * 2");
        ASSERT_EQ(stream, Vector_<int>({Spot, MultiConst, 0, Assign, 0}));
        ASSERT_EQ(consts, Vector_<double>({2.0}));
    }
    {
        const auto [stream, consts] = CompileSource("x = spot() / 2");
        ASSERT_EQ(stream, Vector_<int>({Spot, DivConst, 0, Assign, 0}));
        ASSERT_EQ(consts, Vector_<double>({2.0}));
    }
    {
        const auto [stream, consts] = CompileSource("x = 2 / spot()");
        ASSERT_EQ(stream, Vector_<int>({Spot, ConstDiv, 0, Assign, 0}));
        ASSERT_EQ(consts, Vector_<double>({2.0}));
    }
    {
        const auto [stream, consts] = CompileSource("x = spot() ^ 2");
        ASSERT_EQ(stream, Vector_<int>({Spot, PowConst, 0, Assign, 0}));
        ASSERT_EQ(consts, Vector_<double>({2.0}));
    }
    {
        const auto [stream, consts] = CompileSource("x = 2 ^ spot()");
        ASSERT_EQ(stream, Vector_<int>({Spot, ConstPow, 0, Assign, 0}));
        ASSERT_EQ(consts, Vector_<double>({2.0}));
    }
}

TEST(CompilerTest, TestBinopOnTwoNonConstOperandsEmitsPlainOpcodes) {
    {
        const auto [stream, consts] = CompileSource("x = spot() + spot()");
        ASSERT_EQ(stream, Vector_<int>({Spot, Spot, Add, Assign, 0}));
        ASSERT_EQ(consts.size(), 0u);
    }
    {
        const auto [stream, consts] = CompileSource("x = MAX(spot(), spot())");
        ASSERT_EQ(stream, Vector_<int>({Spot, Spot, Max2, Assign, 0}));
    }
    {
        const auto [stream, consts] = CompileSource("x = MIN(spot(), 2)");
        ASSERT_EQ(stream, Vector_<int>({Spot, Min2Const, 0, Assign, 0}));
        ASSERT_EQ(consts, Vector_<double>({2.0}));
    }
}

TEST(CompilerTest, TestUnaryEmitsUnaryOpcodes) {
    {
        const auto [stream, consts] = CompileSource("x = -spot()");
        ASSERT_EQ(stream, Vector_<int>({Spot, UMinus, Assign, 0}));
        ASSERT_EQ(consts.size(), 0u);
    }
    {
        const auto [stream, consts] = CompileSource("x = SQRT(spot())");
        ASSERT_EQ(stream, Vector_<int>({Spot, Sqrt, Assign, 0}));
    }
    {
        const auto [stream, consts] = CompileSource("x = LOG(spot())");
        ASSERT_EQ(stream, Vector_<int>({Spot, Log, Assign, 0}));
    }
    {
        const auto [stream, consts] = CompileSource("x = EXP(spot())");
        ASSERT_EQ(stream, Vector_<int>({Spot, Exp, Assign, 0}));
    }
}

TEST(CompilerTest, TestIfEmitsIfWithJumpTarget) {
    const auto [stream, consts] = CompileSource(R"(
        IF spot() > 2 THEN
            x = 1
        END
    )");
    ASSERT_EQ(stream, Vector_<int>({Spot, SubConst, 0, Sup, If, 9, AssignConst, 1, 0}));
    ASSERT_EQ(consts, Vector_<double>({2.0, 1.0}));
}

TEST(CompilerTest, TestIfElseEmitsBothJumpTargets) {
    const auto [stream, consts] = CompileSource(R"(
        IF spot() > 2 THEN
            x = 1
        ELSE
            x = 2
        END
    )");
    ASSERT_EQ(stream,
              Vector_<int>({Spot, SubConst, 0, Sup, IfElse, 10, 13, AssignConst, 1, 0, AssignConst, 2, 0}));
    ASSERT_EQ(consts, Vector_<double>({2.0, 1.0, 2.0}));
}

TEST(CompilerTest, TestConstConditionFoldsToTrueOrFalse) {
    {
        const auto [stream, consts] = CompileSource(R"(
            IF 2 > 1 THEN
                x = 1
            END
        )");
        ASSERT_EQ(stream, Vector_<int>({True, If, 6, AssignConst, 0, 0}));
        ASSERT_EQ(consts, Vector_<double>({1.0}));
    }
    {
        const auto [stream, consts] = CompileSource(R"(
            IF 1 > 2 THEN
                x = 1
            END
        )");
        ASSERT_EQ(stream, Vector_<int>({False, If, 6, AssignConst, 0, 0}));
        ASSERT_EQ(consts, Vector_<double>({1.0}));
    }
}

TEST(CompilerTest, TestAndOrNotEmitBoolOpcodes) {
    {
        const auto [stream, consts] = CompileSource(R"(
            IF spot() > 1 AND spot() < 3 THEN
                x = 1
            END
        )");
        ASSERT_EQ(stream,
                  Vector_<int>({Spot, SubConst, 0, Sup, Spot, ConstSub, 1, Sup, And, If, 14, AssignConst, 2, 0}));
        ASSERT_EQ(consts, Vector_<double>({1.0, 3.0, 1.0}));
    }
    {
        const auto [stream, consts] = CompileSource(R"(
            IF spot() > 1 OR spot() < 3 THEN
                x = 1
            END
        )");
        ASSERT_EQ(stream,
                  Vector_<int>({Spot, SubConst, 0, Sup, Spot, ConstSub, 1, Sup, Or, If, 14, AssignConst, 2, 0}));
    }
    {
        const auto [stream, consts] = CompileSource(R"(
            IF spot() != 2 THEN
                x = 1
            END
        )");
        ASSERT_EQ(stream, Vector_<int>({Spot, SubConst, 0, Dal::Script::Equal, Not, If, 10, AssignConst, 1, 0}));
        ASSERT_EQ(consts, Vector_<double>({2.0, 1.0}));
    }
}

TEST(CompilerTest, TestConstVarEmitsConstVarOpcode) {
    Global::Dates_::SetEvaluationDate(Date_(2023, 1, 1));
    Vector_<Cell_> eventDates{Cell_(String_("STRIKE")), Cell_(Date_(2023, 1, 28))};
    Vector_<String_> events{"11.0", "x = spot() - STRIKE"};

    ScriptProduct_ product(eventDates, events);
    product.PreProcess(false, true);
    const ScriptCompiled_ compiled = product.Compile();

    ASSERT_EQ(compiled.NodeStreams().size(), 1u);
    ASSERT_EQ(compiled.NodeStreams()[0], Vector_<int>({Spot, ConstVar, 0, Sub, Assign, 0}));

    Scenario_<double> scenario(1);
    scenario[0].spot_ = 13.0;
    scenario[0].numeraire_ = 1.0;
    EvalState_<double> state = product.BuildEvalState<double>();
    compiled.Evaluate(scenario, state);
    ASSERT_DOUBLE_EQ(state.VarVals()[0], 2.0);
}

TEST(CompilerTest, TestUnknownOpcodeThrows) {
    ScriptCompiled_ bad(Vector_<Vector_<int>>({Vector_<int>({999})}), Vector_<Vector_<>>({Vector_<>()}));

    Scenario_<double> scenario(1);
    scenario[0].spot_ = 1.0;
    scenario[0].numeraire_ = 1.0;
    EvalState_<double> state(Vector_<>(1, 0.0));

    ASSERT_THROW(bad.Evaluate(scenario, state), Dal::Exception_);
}

TEST(CompilerTest, TestPaysDividesByNumeraire) {
    Global::Dates_::SetEvaluationDate(Date_(2023, 1, 1));
    Vector_<Cell_> eventDates{Cell_(Date_(2023, 1, 28))};
    Vector_<String_> events{"call pays spot()"};
    ScriptProduct_ product(eventDates, events, "call");
    product.PreProcess(false, false);

    AssertCompiledParity(product, 13.0, 2.0);

    Scenario_<double> scenario(1);
    scenario[0].spot_ = 13.0;
    scenario[0].numeraire_ = 2.0;
    const ScriptCompiled_ compiled = product.Compile();
    EvalState_<double> state = product.BuildEvalState<double>();
    compiled.Evaluate(scenario, state);
    ASSERT_DOUBLE_EQ(state.VarVals()[product.PayOffIdx()], 6.5);
}

TEST(CompilerTest, TestPaysConstDividesByNumeraire) {
    Global::Dates_::SetEvaluationDate(Date_(2023, 1, 1));
    Vector_<Cell_> eventDates{Cell_(Date_(2023, 1, 28))};
    Vector_<String_> events{"call pays 3"};
    ScriptProduct_ product(eventDates, events, "call");
    product.PreProcess(false, false);

    AssertCompiledParity(product, 13.0, 4.0);

    Scenario_<double> scenario(1);
    scenario[0].spot_ = 13.0;
    scenario[0].numeraire_ = 4.0;
    const ScriptCompiled_ compiled = product.Compile();
    EvalState_<double> state = product.BuildEvalState<double>();
    compiled.Evaluate(scenario, state);
    ASSERT_DOUBLE_EQ(state.VarVals()[product.PayOffIdx()], 0.75);
}

TEST(CompilerTest, TestMultiEventPaysAccumulates) {
    Global::Dates_::SetEvaluationDate(Date_(2023, 1, 1));
    Vector_<Cell_> eventDates{Cell_(Date_(2023, 1, 28)), Cell_(Date_(2023, 2, 28))};
    Vector_<String_> events{"call pays 1", "call pays spot()"};
    ScriptProduct_ product(eventDates, events, "call");
    product.PreProcess(false, false);

    AssertCompiledParity(product, 2.0, 1.0);

    Scenario_<double> scenario(2);
    for (auto& sample : scenario) {
        sample.spot_ = 2.0;
        sample.numeraire_ = 1.0;
    }
    const ScriptCompiled_ compiled = product.Compile();
    EvalState_<double> state = product.BuildEvalState<double>();
    compiled.Evaluate(scenario, state);
    ASSERT_DOUBLE_EQ(state.VarVals()[product.PayOffIdx()], 3.0);
}

TEST(CompilerTest, TestMathFunctionsParity) {
    Global::Dates_::SetEvaluationDate(Date_(2023, 1, 1));
    Vector_<Cell_> eventDates{Cell_(Date_(2023, 1, 28))};
    Vector_<String_> events{"out pays SQRT(spot()) + LOG(spot()) - EXP(0.5)"};
    ScriptProduct_ product(eventDates, events, "out");
    product.PreProcess(false, false);

    for (const double spot : {0.5, 1.0, 7.25}) {
        SCOPED_TRACE("spot=" + std::to_string(spot));
        AssertCompiledParity(product, spot, 1.0);
    }
}

TEST(CompilerTest, TestEvalStateInitRestoresCoreState) {
    // Locks the EvalStateCore_ base-class seam: Init() must restore the initial
    // variable values, reset both stacks and the fuzzy nested-if level.
    EvalState_<double> state(Vector_<>({1.0, 2.0}), Vector_<double>(), 2, 0.5);
    ASSERT_EQ(state.varStore0_.size(), 2u);
    ASSERT_EQ(state.varStore1_.size(), 2u);
    ASSERT_EQ(state.varStore0_[0].size(), 2u);
    ASSERT_DOUBLE_EQ(state.defEps_, 0.5);

    state.variables_[0] = 99.0;
    state.nestedIfLvl_ = 1;
    state.dStack_.Push(42.0);
    state.bStack_.Push(true);

    state.Init();
    ASSERT_DOUBLE_EQ(state.VarVals()[0], 1.0);
    ASSERT_DOUBLE_EQ(state.VarVals()[1], 2.0);
    ASSERT_EQ(state.nestedIfLvl_, 0u);
    ASSERT_TRUE(state.dStack_.IsEmpty());
    ASSERT_TRUE(state.bStack_.IsEmpty());
}

//
// Created by wegam on 2023/1/28.
//

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <string>
#include <utility>

#include <dal/curve/tapeguard.hpp>
#include <dal/math/aad/statistics.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/event.hpp>
#include <dal/script/parser.hpp>
#include <dal/script/visitor/all.hpp>
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

TEST(ScriptTest, TestCompileVectorAppendEntryAndAverage) {
    Parser_ parser;
    auto event = parser.Parse("APPEND(v, 2) APPEND(v, 4) v[1] = v[1] + 2 x = AVERAGE(v)");
    VarIndexer_ indexer;
    for (auto& statement : event)
        statement->Accept(indexer);
    Compiler_ compiler;
    for (const auto& statement : event)
        statement->Accept(compiler);
    EvalState_<double> state(Vector_<>(indexer.VarNames().size(), 0.0), {}, 0, 0.0, indexer.VectorCapacities());
    EvalCompiled(compiler.NodeStream(), compiler.ConstStream(), AAD::Sample_<double>(), state);
    ASSERT_EQ(state.VectorVals()[0].size(), 2);
    ASSERT_DOUBLE_EQ(state.VectorVals()[0][1], 6.0);
    ASSERT_DOUBLE_EQ(state.VarVals()[0], 4.0);
}

TEST(ScriptTest, TestCompiledVectorIndexErrorKeepsSourceLine) {
    Parser_ parser;
    auto event = parser.Parse("APPEND(v, 1)\nx = v[2]");
    VarIndexer_ indexer;
    for (auto& statement : event)
        statement->Accept(indexer);
    Compiler_ compiler;
    for (const auto& statement : event)
        statement->Accept(compiler);
    EvalState_<double> state(Vector_<>(1, 0.0), {}, 0, 0.0, indexer.VectorCapacities());
    try {
        EvalCompiled(compiler.NodeStream(), compiler.ConstStream(), AAD::Sample_<double>(), state);
        FAIL() << "out-of-range vector access must fail";
    } catch (const ScriptError_& error) {
        ASSERT_NE(std::string(error.what()).find("VectorIndexOutOfRange"), std::string::npos);
        ASSERT_NE(std::string(error.what()).find("line=2"), std::string::npos);
    }
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
    struct LsmcPaymentCase_ {
        int opcode_;
        bool constant_;
        bool delayed_;
        bool fuzzy_;
    };

    constexpr LsmcPaymentCase_ LSMC_PAYMENTS[] = {{LsmcPays, false, false, false},      {LsmcPaysConst, true, false, false},
                                                  {LsmcPaysOn, false, true, false},     {LsmcPaysOnConst, true, true, false},
                                                  {LsmcFuzzyPays, false, false, true},  {LsmcFuzzyPaysConst, true, false, true},
                                                  {LsmcFuzzyPaysOn, false, true, true}, {LsmcFuzzyPaysOnConst, true, true, true}};

    Vector_<int> LsmcPaymentStream(const LsmcPaymentCase_& payment, int receiver) {
        Vector_<int> stream{999, payment.opcode_};
        if (payment.constant_)
            stream.push_back(1);
        if (payment.delayed_)
            stream.push_back(1);
        stream.push_back(receiver);
        stream.push_back(999);
        return stream;
    }

    void AssertLsmcPaymentRows(const LsmcPaymentCase_& payment, int receiver, const LsmcRows_& rows, const Vector_<>& fuzzyRows, int paymentCount) {
        const double amount = paymentCount * (payment.delayed_ ? 8.0 : 10.0);
        LsmcRows_ expected(3, 3);
        expected.ZeroPaths(0, 3);
        double expectedFuzzy = 0.0;
        if (receiver == 1) {
            if (payment.fuzzy_)
                expectedFuzzy = amount;
            else
                expected[0][2] = amount;
        }
        for (size_t row = 0; row < 3; ++row)
            for (size_t path = 0; path < 3; ++path)
                ASSERT_DOUBLE_EQ(rows[row][path], expected[row][path]);
        ASSERT_DOUBLE_EQ(fuzzyRows[0], 17.0);
        ASSERT_DOUBLE_EQ(fuzzyRows[1], expectedFuzzy);
        ASSERT_DOUBLE_EQ(fuzzyRows[2], 19.0);
    }

    void AssertLsmcReceiverAdjoints(const LsmcPaymentCase_& payment,
                                    const AAD::Number_& amount,
                                    const AAD::Number_& discount,
                                    const AAD::Number_& numeraire) {
        ASSERT_NEAR(AAD::Adjoint(amount), payment.constant_ ? 0.0 : payment.delayed_ ? 0.4 : 0.5, 1e-10);
        ASSERT_NEAR(AAD::Adjoint(discount), payment.delayed_ ? 5.0 : 0.0, 1e-10);
        ASSERT_NEAR(AAD::Adjoint(numeraire), payment.delayed_ ? -2.0 : -2.5, 1e-10);
    }

    void AssertLsmcFuzzyRowAdjoints(const LsmcPaymentCase_& payment,
                                    const AAD::Number_& amount,
                                    const AAD::Number_& discount,
                                    const AAD::Number_& numeraire) {
        ASSERT_NEAR(AAD::Adjoint(amount), payment.constant_ ? 0.0 : payment.delayed_ ? 0.8 : 1.0, 1e-10);
        ASSERT_NEAR(AAD::Adjoint(discount), payment.delayed_ ? 10.0 : 0.0, 1e-10);
        ASSERT_NEAR(AAD::Adjoint(numeraire), 0.0, 1e-10);
    }

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

TEST(CompilerTest, TestLsmcPaymentOpcodesRecordAndAdvance) {
    for (const auto& payment : LSMC_PAYMENTS) {
        for (const int receiver : {0, 1}) {
            SCOPED_TRACE(payment.opcode_);
            SCOPED_TRACE(receiver);
            EvalState_<double> state(Vector_<>{3.0, 3.0});
            state.dStack_.Push(-7.0);
            if (!payment.constant_)
                state.dStack_.Push(10.0);
            state.bStack_.Push(true);
            AAD::Sample_<double> sample{};
            sample.numeraire_ = 2.0;
            sample.discounts_ = {-9.0, 0.8};
            const auto stream = LsmcPaymentStream(payment, receiver);
            const Vector_<> constants{-99.0, 10.0};
            LsmcRows_ rows(3, 3);
            rows.ZeroPaths(0, 3);
            const Vector_<size_t> eventToPays{2, 0};
            LsmcSinks_ hard;
            hard.pays_ = &rows;
            hard.eventToPays_ = &eventToPays;
            hard.eventOrdinal_ = 1;
            hard.pathSlot_ = 2;
            hard.payoffIdx_ = 1;
            Vector_<> fuzzyRows{17.0, 0.0, 19.0};
            LsmcFuzzySinks_<double> fuzzy;
            fuzzy.pays_ = &fuzzyRows;
            fuzzy.eventOrdinal_ = 1;
            fuzzy.payoffIdx_ = 1;
            state.lsmcSinks_ = &hard;
            state.lsmcFuzzySinks_ = &fuzzy;
            const Script::Detail::CompiledEventView_<double> event{stream, constants, sample};

            ASSERT_EQ(Script::Detail::EvalCompiledLsmcOp(event, 1, &state), stream.size() - 1);
            ASSERT_DOUBLE_EQ(state.VarVals()[receiver], payment.delayed_ ? 7.0 : 8.0);
            ASSERT_DOUBLE_EQ(state.VarVals()[1 - receiver], 3.0);
            ASSERT_NO_FATAL_FAILURE(AssertLsmcPaymentRows(payment, receiver, rows, fuzzyRows, 1));
            ASSERT_EQ(state.dStack_.Size(), 1u);
            ASSERT_DOUBLE_EQ(state.dStack_.Top(), -7.0);
            ASSERT_EQ(state.bStack_.Size(), 1u);
            ASSERT_TRUE(state.bStack_.Top());
            if (!payment.constant_)
                state.dStack_.Push(10.0);
            ASSERT_EQ(Script::Detail::EvalCompiledLsmcOp(event, 1, &state), stream.size() - 1);
            ASSERT_DOUBLE_EQ(state.VarVals()[receiver], payment.delayed_ ? 11.0 : 13.0);
            ASSERT_NO_FATAL_FAILURE(AssertLsmcPaymentRows(payment, receiver, rows, fuzzyRows, 2));
        }
    }
}

TEST(CompilerTest, TestLsmcPaymentOpcodesPreserveAnalyticAdjoints) {
    for (const auto& payment : LSMC_PAYMENTS) {
        SCOPED_TRACE(payment.opcode_);
        auto* tape = AAD::Tape();
        TapeGuard_ guard(tape);
        AAD::Number_ amount = 10.0, discount = 0.8, numeraire = 2.0;
        AAD::PutOnTape(amount);
        AAD::PutOnTape(discount);
        AAD::PutOnTape(numeraire);
        AAD::NewRecording(*tape);
        EvalState_<AAD::Number_> state(Vector_<>{3.0});
        if (!payment.constant_)
            state.dStack_.Push(amount);
        AAD::Sample_<AAD::Number_> sample{};
        sample.numeraire_ = numeraire;
        sample.discounts_ = {AAD::Number_(-9.0), discount};
        LsmcRows_ rows(1, 1);
        rows.ZeroPaths(0, 1);
        const Vector_<size_t> eventToPays{0};
        LsmcSinks_ hard;
        hard.pays_ = &rows;
        hard.eventToPays_ = &eventToPays;
        hard.payoffIdx_ = 0;
        Vector_<AAD::Number_> fuzzyRows{AAD::Number_(0.0)};
        LsmcFuzzySinks_<AAD::Number_> fuzzy;
        fuzzy.pays_ = &fuzzyRows;
        fuzzy.payoffIdx_ = 0;
        state.lsmcSinks_ = &hard;
        state.lsmcFuzzySinks_ = &fuzzy;
        const auto stream = LsmcPaymentStream(payment, 0);
        Script::Detail::EvalCompiledRange<true, true>(stream, Vector_<>{-99.0, 10.0}, sample, state, 1, stream.size() - 1, false);
        const auto stats = AAD::MeasureTape(*tape);
        const std::string prefix = "opcode_" + std::to_string(payment.opcode_);
        RecordProperty(prefix + "_nodes", static_cast<int>(stats.nodes_));
        RecordProperty(prefix + "_edges", static_cast<int>(stats.edges_));
        RecordProperty(prefix + "_bytes", static_cast<int>(stats.liveBytes_));
        ASSERT_DOUBLE_EQ(AAD::Value(state.VarVals()[0]), payment.delayed_ ? 7.0 : 8.0);
        ASSERT_DOUBLE_EQ(rows[0][0], payment.fuzzy_ ? 0.0 : payment.delayed_ ? 8.0 : 10.0);
        ASSERT_TRUE(state.dStack_.IsEmpty());
        AAD::ZeroAdjoints(*tape);
        AAD::Adjoint(state.variables_[0]) = 1.0;
        AAD::PropagateToStart(*tape);
        ASSERT_NO_FATAL_FAILURE(AssertLsmcReceiverAdjoints(payment, amount, discount, numeraire));
        if (payment.fuzzy_) {
            ASSERT_DOUBLE_EQ(AAD::Value(fuzzyRows[0]), payment.delayed_ ? 8.0 : 10.0);
            AAD::ZeroAdjoints(*tape);
            AAD::Adjoint(fuzzyRows[0]) = 1.0;
            AAD::PropagateToStart(*tape);
            ASSERT_NO_FATAL_FAILURE(AssertLsmcFuzzyRowAdjoints(payment, amount, discount, numeraire));
        }
    }
}

TEST(CompilerTest, TestLsmcPaymentMissingSinksPreserveErrorAndMutationOrder) {
    for (const auto& payment : LSMC_PAYMENTS) {
        for (const int receiver : {0, 1}) {
            SCOPED_TRACE(payment.opcode_);
            SCOPED_TRACE(receiver);
            EvalState_<double> state(Vector_<>{3.0, 3.0});
            state.dStack_.Push(-7.0);
            if (!payment.constant_)
                state.dStack_.Push(10.0);
            AAD::Sample_<double> sample{};
            sample.numeraire_ = 2.0;
            sample.discounts_ = {-9.0, 0.8};
            const auto stream = LsmcPaymentStream(payment, receiver);
            const Vector_<> constants{-99.0, 10.0};
            const Script::Detail::CompiledEventView_<double> event{stream, constants, sample};
            try {
                Script::Detail::EvalCompiledLsmcOp(event, 1, &state);
                FAIL() << "missing LSMC sinks must fail";
            } catch (const ScriptError_& error) {
                const std::string message = payment.fuzzy_ ? "UnsupportedExecutionMode: the LSMC fuzzy replay requires the LSMC driver's sinks"
                                                           : "UnsupportedExecutionMode: the LSMC recording stream requires the LSMC driver's sinks";
                ASSERT_NE(std::string(error.what()).find(message), std::string::npos);
            }
            ASSERT_EQ(state.VarVals(), Vector_<>({3.0, 3.0}));
            ASSERT_EQ(state.dStack_.Size(), 1u);
            ASSERT_DOUBLE_EQ(state.dStack_.Top(), -7.0);
        }
    }
}

TEST(CompilerTest, TestLsmcHardPaymentWithoutRecordingRowsStillAccumulates) {
    for (const auto& payment : LSMC_PAYMENTS) {
        if (payment.fuzzy_)
            continue;
        SCOPED_TRACE(payment.opcode_);
        EvalState_<double> state(Vector_<>{3.0});
        if (!payment.constant_)
            state.dStack_.Push(10.0);
        AAD::Sample_<double> sample{};
        sample.numeraire_ = 2.0;
        sample.discounts_ = {-9.0, 0.8};
        LsmcSinks_ sinks;
        sinks.payoffIdx_ = 0;
        state.lsmcSinks_ = &sinks;
        const auto stream = LsmcPaymentStream(payment, 0);
        Script::Detail::EvalCompiledRange<true, true>(stream, Vector_<>{-99.0, 10.0}, sample, state, 1, stream.size() - 1, false);
        ASSERT_DOUBLE_EQ(state.VarVals()[0], payment.delayed_ ? 7.0 : 8.0);
        ASSERT_TRUE(state.dStack_.IsEmpty());
    }
}

TEST(CompilerTest, TestLsmcDelayedPaymentMaterializesBeforeNumeraireDivision) {
    for (const auto& payment : LSMC_PAYMENTS) {
        if (!payment.delayed_)
            continue;
        SCOPED_TRACE(payment.opcode_);
        EvalState_<double> state(Vector_<>{0.0});
        if (!payment.constant_)
            state.dStack_.Push(1e308);
        AAD::Sample_<double> sample{};
        sample.numeraire_ = 1e308;
        sample.discounts_ = {0.0, 1e-308};
        LsmcRows_ rows(1, 1);
        rows.ZeroPaths(0, 1);
        const Vector_<size_t> eventToPays{0};
        LsmcSinks_ hard;
        hard.pays_ = &rows;
        hard.eventToPays_ = &eventToPays;
        hard.payoffIdx_ = 0;
        Vector_<> fuzzyRows{0.0};
        LsmcFuzzySinks_<double> fuzzy;
        fuzzy.pays_ = &fuzzyRows;
        fuzzy.payoffIdx_ = 0;
        state.lsmcSinks_ = &hard;
        state.lsmcFuzzySinks_ = &fuzzy;
        const auto stream = LsmcPaymentStream(payment, 0);
        Script::Detail::EvalCompiledRange<true, true>(stream, Vector_<>{0.0, 1e308}, sample, state, 1, stream.size() - 1, false);
        ASSERT_DOUBLE_EQ(state.VarVals()[0], 1e-308);
        ASSERT_NEAR(payment.fuzzy_ ? fuzzyRows[0] : rows[0][0], 1.0, 1e-15);
    }
}

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

TEST(CompilerTest, TestDirectExtremaPreserveNanAndSignedZeroOrder) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const AAD::Sample_<double> sample{};
    for (const int op : {Max2, Min2, Max2Const, Min2Const}) {
        const bool constant = op == Max2Const || op == Min2Const;
        const Vector_<int> stream = constant ? Vector_<int>{Const, 0, op, 1, Assign, 0} : Vector_<int>{Const, 0, Const, 1, op, Assign, 0};
        for (const auto& values : {Vector_<>{nan, 2.0}, Vector_<>{2.0, nan}, Vector_<>{-0.0, 0.0}, Vector_<>{0.0, -0.0}}) {
            SCOPED_TRACE(op);
            EvalState_<double> state(Vector_<>{0.0});
            EvalCompiled(stream, values, sample, state);
            if (std::isnan(values[0]))
                ASSERT_TRUE(std::isnan(state.VarVals()[0]));
            else {
                ASSERT_DOUBLE_EQ(state.VarVals()[0], values[0]);
                ASSERT_EQ(std::signbit(state.VarVals()[0]), std::signbit(values[0]));
            }
            ASSERT_TRUE(state.dStack_.IsEmpty());
        }
    }
}

TEST(CompilerTest, TestDirectAadExtremaKeepFirstOperandAtEquality) {
    for (const int op : {Max2, Min2, Max2Const, Min2Const}) {
        SCOPED_TRACE(op);
        auto* tape = AAD::Tape();
        TapeGuard_ guard(tape);
        AAD::Number_ first = 2.0;
        AAD::Number_ second = 2.0;
        AAD::PutOnTape(first);
        AAD::PutOnTape(second);
        AAD::NewRecording(*tape);
        const AAD::Sample_<AAD::Number_> sample{};
        EvalState_<AAD::Number_> state(Vector_<>{0.0}, Vector_<AAD::Number_>{first, second});
        const bool constant = op == Max2Const || op == Min2Const;
        const Vector_<int> stream = constant ? Vector_<int>{ConstVar, 0, op, 0, Assign, 0} : Vector_<int>{ConstVar, 0, ConstVar, 1, op, Assign, 0};
        EvalCompiled(stream, Vector_<>{2.0}, sample, state);
        AAD::ZeroAdjoints(*tape);
        AAD::Adjoint(state.variables_[0]) = 1.0;
        AAD::PropagateToStart(*tape);
        ASSERT_DOUBLE_EQ(AAD::Value(state.VarVals()[0]), 2.0);
        ASSERT_NEAR(AAD::Adjoint(first), 1.0, 1e-10);
        ASSERT_NEAR(AAD::Adjoint(second), 0.0, 1e-10);
    }
}

TEST(CompilerTest, TestDirectSlicePreservesStacksAndVariablesWithoutReset) {
    EvalState_<double> state(Vector_<>{3.0});
    state.dStack_.Push(5.0);
    state.bStack_.Push(true);
    const Vector_<int> stream{999, Const, 0, Add, Assign, 0, 999};
    const AAD::Sample_<double> sample{};
    EvalCompiled(stream, Vector_<>{7.0}, sample, state, 1, 6, false);
    ASSERT_DOUBLE_EQ(state.VarVals()[0], 12.0);
    ASSERT_TRUE(state.dStack_.IsEmpty());
    ASSERT_EQ(state.bStack_.Size(), 1u);
    ASSERT_TRUE(state.bStack_.Top());
    EvalCompiled(Vector_<int>{Var, 0, AddConst, 0, Assign, 0}, Vector_<>{1.0}, sample, state);
    ASSERT_DOUBLE_EQ(state.VarVals()[0], 13.0);
    ASSERT_TRUE(state.bStack_.IsEmpty());
}

TEST(CompilerTest, TestCompiledEventsResetStacksAndPreservePathVariables) {
    Vector_<int> first{Var, 0, AddConst, 0, Assign, 0};
    for (size_t i = 0; i < 128; ++i) {
        first.push_back(Spot);
        first.push_back(True);
    }
    ScriptCompiled_ compiled(Vector_<Vector_<int>>{first, {Var, 0, AddConst, 0, Assign, 0}}, Vector_<Vector_<>>{{7.0}, {1.0}});
    Scenario_<double> scenario(2);
    scenario[0].spot_ = 2.0;
    EvalState_<double> state(Vector_<>{3.0});
    for (int path = 0; path < 2; ++path) {
        compiled.Evaluate(scenario, state);
        ASSERT_DOUBLE_EQ(state.VarVals()[0], 11.0);
        ASSERT_TRUE(state.dStack_.IsEmpty());
        ASSERT_TRUE(state.bStack_.IsEmpty());
    }
}

TEST(CompilerTest, TestDirectOperandStackBoundsThrow) {
    const AAD::Sample_<double> sample{};
    for (const auto& stream : {Vector_<int>{Add}, Vector_<int>{Spot, Max2}, Vector_<int>{True, And}, Vector_<int>{Assign, 0}, Vector_<int>(129, Spot),
                               Vector_<int>(129, True)}) {
        EvalState_<double> state(Vector_<>{0.0});
        ASSERT_THROW(EvalCompiled(stream, Vector_<>{}, sample, state), Dal::Exception_);
    }
}

TEST(CompilerTest, TestDirectFuzzyComparisonUsesDefaultEpsilon) {
    AAD::Sample_<double> sample;
    sample.spot_ = 0.25;
    for (const int op : {FuzzyEqual, FuzzyComp}) {
        EvalState_<double> state(Vector_<>{0.0}, Vector_<>{}, 0, 2.0);
        const Vector_<int> stream{Spot, op, 0, Assign, 0};
        EvalCompiled(stream, Vector_<>{-1.0}, sample, state);
        ASSERT_DOUBLE_EQ(state.VarVals()[0], op == FuzzyEqual ? 0.75 : 0.625);
        EvalCompiled(stream, Vector_<>{1.0}, sample, state);
        ASSERT_DOUBLE_EQ(state.VarVals()[0], op == FuzzyEqual ? 0.5 : 0.75);
        ASSERT_TRUE(state.dStack_.IsEmpty());
    }
}

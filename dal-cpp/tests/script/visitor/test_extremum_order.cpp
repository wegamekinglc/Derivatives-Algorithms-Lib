#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include <dal/math/aad/recording.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/detail/segmentedtrace.hpp>
#include <dal/script/event.hpp>
#include <dal/script/parser.hpp>
#include <dal/storage/globals.hpp>

using Dal::Vector_;
using Dal::AAD::Number_;

namespace {
    struct ChoiceCase_ {
        double left_, right_;
        bool maxRight_, minRight_;
    };

    Vector_<ChoiceCase_> ChoiceCases() {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        const double inf = std::numeric_limits<double>::infinity();
        return {{1.0, 2.0, true, false},  {2.0, 1.0, false, true},  {1.0, 1.0, false, false},  {-0.0, 0.0, false, false}, {0.0, -0.0, false, false},
                {nan, 1.0, false, false}, {1.0, nan, false, false}, {inf, 1.0, false, true},   {-inf, 1.0, true, false},  {1.0, inf, true, false},
                {1.0, -inf, false, true}, {inf, inf, false, false}, {-inf, -inf, false, false}};
    }

    void AssertIeeeValue(double actual, double expected) {
        if (std::isnan(expected))
            ASSERT_TRUE(std::isnan(actual));
        else
            ASSERT_DOUBLE_EQ(actual, expected);
        if (expected == 0.0)
            ASSERT_EQ(std::signbit(actual), std::signbit(expected));
    }

    Dal::Script::Event_ ParseChoice(const ChoiceCase_& data, bool maximum, bool literalLeft, bool historical = false) {
        Dal::Script::Parser_ parser({{"K", 1.0}});
        const Dal::String_ input = historical ? "K" : "SPOT()";
        const Dal::String_ operands = literalLeft ? "1, " + input : input + ", 1";
        auto event = parser.Parse("pay = " + Dal::String_(maximum ? "MAX(" : "MIN(") + operands + ")");
        Dal::Script::VarIndexer_ indexer;
        event[0]->Accept(indexer);
        Dal::Script::ConstProcessor_ constants(indexer.VarNames().size());
        event[0]->Accept(constants);
        const auto& expression = event[0]->arguments_[1];
        auto* literal = Dal::Script::Downcast<Dal::Script::NodeConst_>(expression->arguments_[literalLeft ? 0 : 1]);
        literal->constVal_ = literalLeft ? data.left_ : data.right_;
        return event;
    }

    bool RightSelected(const ChoiceCase_& data, bool maximum) { return maximum ? data.maxRight_ : data.minRight_; }

    double SelectedValue(const ChoiceCase_& data, bool maximum) { return RightSelected(data, maximum) ? data.right_ : data.left_; }

    void CheckDoubleChoice(const ChoiceCase_& data, bool maximum, bool literalLeft, bool historical = false) {
        const auto event = ParseChoice(data, maximum, literalLeft, historical);
        Dal::Script::Compiler_ compiler(false, nullptr, historical);
        event[0]->Accept(compiler);
        Dal::AAD::Scenario_<double> path(1);
        path[0].spot_ = literalLeft ? data.right_ : data.left_;
        Dal::Script::Evaluator_<double> tree(Vector_<>{0.0}, Vector_<>{path[0].spot_});
        tree.SetScenario(&path);
        tree.SetCurEvt(0);
        event[0]->Accept(tree);
        Dal::Script::EvalState_<double> state(Vector_<>{0.0}, Vector_<>{path[0].spot_});
        Dal::Script::EvalCompiled(compiler.NodeStream(), compiler.ConstStream(), path[0], state);
        ASSERT_NO_FATAL_FAILURE(AssertIeeeValue(tree.VarVals()[0], SelectedValue(data, maximum)));
        ASSERT_NO_FATAL_FAILURE(AssertIeeeValue(state.VarVals()[0], SelectedValue(data, maximum)));
    }

    size_t LeavesSince(Dal::AAD::Tape_::Iterator_ first) {
        size_t leaves = 0;
        for (; first != Dal::AAD::Tape()->nodes_.End(); ++first)
            leaves += first->NumArguments() == 0;
        return leaves;
    }

    void CheckAadChoice(const ChoiceCase_& data, bool maximum, bool literalLeft, bool fuzzy, bool traced) {
        const auto event = ParseChoice(data, maximum, literalLeft);
        Dal::Script::Compiler_ compiler(fuzzy);
        event[0]->Accept(compiler);
        const Dal::Script::ScriptCompiled_ program({compiler.NodeStream()}, {compiler.ConstStream()});
        const Dal::Script::Detail::SegmentedTracePlan_ plan(program, {}, {0}, 1);
        ASSERT_EQ(plan.MaxWords(), 1U);
        Vector_<std::uint64_t> trace(plan.MaxWords(), 0);
        const Vector_<Number_> observations;
        using TracePolicy_ = Dal::Script::Detail::SegmentedCompiledPolicy_<Number_>;
        const TracePolicy_ policy(nullptr, observations, plan.View(0, &trace));
        Dal::Script::EvalState_<Number_> state(Vector_<>{0.0});
        Dal::AAD::Sample_<Number_> sample;
        Dal::AAD::RecordingScope_ recording;
        recording.RegisterInput(sample.spot_, literalLeft ? data.right_ : data.left_);
        recording.StartRecording();
        const auto first = Dal::AAD::Tape()->nodes_.End();
        if (traced) {
            Dal::Script::Detail::EvalCompiledEvents<false>(
                1,
                [&](size_t) {
                    return Dal::Script::Detail::CompiledEventView_<Number_, TracePolicy_>{
                        compiler.NodeStream(), compiler.ConstStream(), sample, 0, 0, true, policy};
                },
                &state);
            const auto choice = data.left_ == data.right_ ? 3U : (RightSelected(data, maximum) ? 2U : 1U);
            ASSERT_EQ(trace[0], choice);
        } else {
            Dal::Script::EvalCompiled(compiler.NodeStream(), compiler.ConstStream(), sample, state);
        }
        const bool active = RightSelected(data, maximum) == literalLeft;
        ASSERT_EQ(LeavesSince(first), active ? 0U : 1U);
        ASSERT_NO_FATAL_FAILURE(AssertIeeeValue(Dal::AAD::Value(state.VarVals()[0]), SelectedValue(data, maximum)));
        Dal::AAD::Adjoint(state.VarVals()[0]) = 1.0;
        recording.FinishRecording();
        recording.Reverse();
        ASSERT_DOUBLE_EQ(Dal::AAD::Adjoint(sample.spot_), active ? 1.0 : 0.0);
    }

    void CheckBoundTie(bool maximum, bool boundLeft) {
        const auto date = Dal::XGLOBAL::SetEvaluationDateInScope(Dal::Date_(2026, 10, 12));
        const Dal::String_ operands = boundLeft ? "K, SPOT()" : "SPOT(), K";
        Dal::Script::ScriptProduct_ product({Dal::Cell_("K"), Dal::Cell_(Dal::Date_(2026, 10, 22))},
                                            {"3", "pay = " + Dal::String_(maximum ? "MAX(" : "MIN(") + operands + ")"});
        product.PreProcess(true, false);
        const auto program = product.Compile(true);
        auto state = product.BuildEvalState<Number_>();
        Dal::AAD::Sample_<Number_> sample;
        Dal::AAD::RecordingScope_ recording;
        recording.RegisterInput(sample.spot_, 10.0);
        recording.RegisterInput(state.ConstVarVals()[0], 10.0);
        recording.StartRecording();
        const auto first = Dal::AAD::Tape()->nodes_.End();
        Dal::Script::EvalCompiled(program.NodeStreams()[0], program.ConstStreams()[0], sample, state);
        ASSERT_EQ(LeavesSince(first), 0U);
        ASSERT_DOUBLE_EQ(Dal::AAD::Value(state.VarVals()[0]), 10.0);
        Dal::AAD::Adjoint(state.VarVals()[0]) = 1.0;
        recording.FinishRecording();
        recording.Reverse();
        ASSERT_DOUBLE_EQ(Dal::AAD::Adjoint(sample.spot_), boundLeft ? 0.0 : 1.0);
        ASSERT_DOUBLE_EQ(Dal::AAD::Adjoint(state.ConstVarVals()[0]), boundLeft ? 1.0 : 0.0);
    }

    void CheckExpressionTie(const Dal::String_& expression, double input, double slope) {
        Dal::Script::Parser_ parser;
        const auto event = parser.Parse(expression);
        Dal::Script::VarIndexer_ indexer;
        Dal::Script::ConstProcessor_ constants(2);
        Dal::Script::Compiler_ compiler;
        for (const auto& statement : event) {
            statement->Accept(indexer);
            statement->Accept(constants);
            statement->Accept(compiler);
        }
        Dal::Script::EvalState_<Number_> state(Vector_<>(indexer.VarNames().size(), 0.0));
        Dal::AAD::Sample_<Number_> sample;
        Dal::AAD::RecordingScope_ recording;
        recording.RegisterInput(sample.spot_, input);
        recording.StartRecording();
        Dal::Script::EvalCompiled(compiler.NodeStream(), compiler.ConstStream(), sample, state);
        Dal::AAD::Adjoint(state.VarVals().back()) = 1.0;
        recording.FinishRecording();
        recording.Reverse();
        ASSERT_DOUBLE_EQ(Dal::AAD::Adjoint(sample.spot_), slope);
    }

    void CheckCheckpointReuse(bool maximum) {
        Dal::Script::EvalState_<Number_> original(Vector_<>{0.0});
        Dal::Script::EvalState_<Number_> copied(original);
        auto state = std::move(copied);
        for (int generation = 0; generation < 2; ++generation) {
            const auto event = ParseChoice({10.0, 10.0, false, false}, maximum, true);
            Dal::Script::Compiler_ compiler;
            event[0]->Accept(compiler);
            Dal::AAD::Sample_<Number_> sample;
            Number_ input;
            Dal::AAD::RecordingScope_ recording;
            recording.RegisterInput(input, 10.0);
            recording.StartRecording();
            const auto checkpoint = recording.MakeCheckpoint();
            for (const double shift : {-5.0, 0.0, 5.0}) {
                recording.Restore(checkpoint);
                recording.ClearAdjoints();
                sample.spot_ = input + shift;
                Dal::Script::EvalCompiled(compiler.NodeStream(), compiler.ConstStream(), sample, state);
                const bool active = maximum ? shift > 0.0 : shift < 0.0;
                ASSERT_DOUBLE_EQ(Dal::AAD::Value(state.VarVals()[0]), active ? 10.0 + shift : 10.0);
                recording.FinishRecording();
                Dal::AAD::Adjoint(state.VarVals()[0]) = 1.0;
                recording.ReverseSuffix(checkpoint);
                recording.ReversePrefix(checkpoint);
                ASSERT_DOUBLE_EQ(Dal::AAD::Adjoint(input), active ? 1.0 : 0.0);
            }
        }
    }
} // namespace

TEST(ExtremumOrderTest, TestConstantLeftTieUsesSourceLeftDerivative) {
    Dal::Script::Parser_ parser;
    auto event = parser.Parse("pay = MAX(10, SPOT())");
    Dal::Script::VarIndexer_ indexer;
    event[0]->Accept(indexer);
    Dal::Script::ConstProcessor_ constants(indexer.VarNames().size());
    event[0]->Accept(constants);
    Dal::Script::Compiler_ compiler;
    event[0]->Accept(compiler);
    Dal::Script::EvalState_<Number_> state(Vector_<>{0.0});
    Dal::AAD::Sample_<Number_> sample;
    Dal::AAD::RecordingScope_ recording;
    recording.RegisterInput(sample.spot_, 10.0);
    recording.StartRecording();
    Dal::Script::EvalCompiled(compiler.NodeStream(), compiler.ConstStream(), sample, state);
    ASSERT_DOUBLE_EQ(Dal::AAD::Value(state.VarVals()[0]), 10.0);
    Dal::AAD::Adjoint(state.VarVals()[0]) = 1.0;
    recording.FinishRecording();
    recording.Reverse();
    ASSERT_DOUBLE_EQ(Dal::AAD::Adjoint(sample.spot_), 0.0);
}

TEST(ExtremumOrderTest, TestDoubleIeeeChoicesMatchSourceLeftTree) {
    for (const auto& data : ChoiceCases())
        for (const bool maximum : {false, true})
            for (const bool literalLeft : {false, true})
                ASSERT_NO_FATAL_FAILURE(CheckDoubleChoice(data, maximum, literalLeft));
}

TEST(ExtremumOrderTest, TestAadIeeeChoicesAndTraceKeepOnlySelectedRisk) {
    for (const auto& data : ChoiceCases())
        for (const bool maximum : {false, true})
            for (const bool literalLeft : {false, true})
                for (const bool fuzzy : {false, true})
                    for (const bool traced : {false, true})
                        ASSERT_NO_FATAL_FAILURE(CheckAadChoice(data, maximum, literalLeft, fuzzy, traced));
}

TEST(ExtremumOrderTest, TestBoundConstantTieRetainsSourceLeftInputRisk) {
    for (const bool maximum : {false, true})
        for (const bool boundLeft : {false, true})
            ASSERT_NO_FATAL_FAILURE(CheckBoundTie(maximum, boundLeft));
}

TEST(ExtremumOrderTest, TestAssignedVariableTieRetainsLiteralOrder) {
    ASSERT_NO_FATAL_FAILURE(CheckExpressionTie("x = SPOT() pay = MAX(10, x)", 10.0, 0.0));
    ASSERT_NO_FATAL_FAILURE(CheckExpressionTie("x = SPOT() pay = MIN(10, x)", 10.0, 0.0));
    ASSERT_NO_FATAL_FAILURE(CheckExpressionTie("x = SPOT() pay = MAX(x, 10)", 10.0, 1.0));
    ASSERT_NO_FATAL_FAILURE(CheckExpressionTie("x = SPOT() pay = MIN(x, 10)", 10.0, 1.0));
}

TEST(ExtremumOrderTest, TestActiveOperandTieRetainsSourceLeftRisk) {
    ASSERT_NO_FATAL_FAILURE(CheckExpressionTie("pay = MAX(SPOT(), 2 * SPOT())", 0.0, 1.0));
    ASSERT_NO_FATAL_FAILURE(CheckExpressionTie("pay = MIN(SPOT(), 2 * SPOT())", 0.0, 1.0));
    ASSERT_NO_FATAL_FAILURE(CheckExpressionTie("pay = MAX(2 * SPOT(), SPOT())", 0.0, 2.0));
    ASSERT_NO_FATAL_FAILURE(CheckExpressionTie("pay = MIN(2 * SPOT(), SPOT())", 0.0, 2.0));
}

TEST(ExtremumOrderTest, TestHistoricalCompilationRetainsLiteralOrder) {
    for (const auto& data : ChoiceCases())
        for (const bool maximum : {false, true})
            for (const bool literalLeft : {false, true})
                ASSERT_NO_FATAL_FAILURE(CheckDoubleChoice(data, maximum, literalLeft, true));
}

TEST(ExtremumOrderTest, TestCopiedAndMovedStatesReuseSelectedConstantsAcrossRecordings) {
    ASSERT_NO_FATAL_FAILURE(CheckCheckpointReuse(false));
    ASSERT_NO_FATAL_FAILURE(CheckCheckpointReuse(true));
}

TEST(ExtremumOrderTest, TestVariadicExtremaKeepSourceLeftRisk) {
    // Fold left means the earlier operand wins every tie. These cases also prove
    // that AAD reaches the operand selected at the innermost comparison.
    ASSERT_NO_FATAL_FAILURE(CheckExpressionTie("pay = MAX(SPOT(), 1, 2)", 1.5, 0.0));
    ASSERT_NO_FATAL_FAILURE(CheckExpressionTie("pay = MAX(0, SPOT(), 2)", 3.0, 1.0));
    ASSERT_NO_FATAL_FAILURE(CheckExpressionTie("pay = MAX(1, 2, SPOT())", 3.0, 1.0));
    ASSERT_NO_FATAL_FAILURE(CheckExpressionTie("pay = MIN(SPOT(), 3, 2)", 2.5, 0.0));
    ASSERT_NO_FATAL_FAILURE(CheckExpressionTie("pay = MAX(SPOT(), 2, 2)", 2.0, 1.0));
    ASSERT_NO_FATAL_FAILURE(CheckExpressionTie("pay = MAX(2, SPOT(), 2)", 2.0, 0.0));
}

TEST(ExtremumOrderTest, TestVariadicExtremaFoldIntoBinaryNodes) {
    // The trailing operand decides the result, so a flat binary-only read of the
    // first two operands (the old defect) fails this test
    for (const bool maximum : {false, true}) {
        Dal::Script::Parser_ parser;
        const Dal::String_ expression = maximum ? "pay = MAX(SPOT(), 1, 3)" : "pay = MIN(SPOT(), 3, 1)";
        auto event = parser.Parse(expression);
        Dal::Script::VarIndexer_ indexer;
        event[0]->Accept(indexer);
        Dal::Script::ConstProcessor_ constants(indexer.VarNames().size());
        event[0]->Accept(constants);
        Dal::Script::Compiler_ compiler(false, nullptr);
        event[0]->Accept(compiler);
        Dal::AAD::Scenario_<double> path(1);
        path[0].spot_ = 2.0;
        Dal::Script::Evaluator_<double> tree(Vector_<>{0.0}, Vector_<>{path[0].spot_});
        tree.SetScenario(&path);
        tree.SetCurEvt(0);
        event[0]->Accept(tree);
        Dal::Script::EvalState_<double> state(Vector_<>{0.0}, Vector_<>{path[0].spot_});
        Dal::Script::EvalCompiled(compiler.NodeStream(), compiler.ConstStream(), path[0], state);
        const double expected = maximum ? 3.0 : 1.0;
        ASSERT_NO_FATAL_FAILURE(AssertIeeeValue(tree.VarVals()[0], expected));
        ASSERT_NO_FATAL_FAILURE(AssertIeeeValue(state.VarVals()[0], expected));
    }
}

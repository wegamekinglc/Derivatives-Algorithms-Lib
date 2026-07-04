//
// Created by wegam on 2023/1/28.
//

#include <gtest/gtest.h>
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
    product.Compile();

    EvalState_<double> eval_state(Vector_<>(product.VarNames().size(), 0.0));
    Scenario_<double> scenario(1);
    product.EvaluateCompiled(scenario, eval_state);

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
    product.Compile();

    EvalState_<double> eval_state(Vector_<>(product.VarNames().size(), 0.0));
    Scenario_<double> scenario(1);
    scenario[0].spot_ = 4.0;
    product.EvaluateCompiled(scenario, eval_state);

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
    product.Compile();

    EvalState_<double> eval_state(Vector_<>(product.VarNames().size(), 0.0));
    Scenario_<double> scenario(2);
    product.EvaluateCompiled(scenario, eval_state);

    ASSERT_DOUBLE_EQ(eval_state.variables_[0], 4);
    ASSERT_DOUBLE_EQ(eval_state.variables_[1], 7);
}

// ============================================================================
// Phase 1b direct Compiler_ unit tests (primary verification for #2 + #5).
// The parity pins for these two defects pass even on the unfixed code (see the
// long comments on TestParity_And_OrShortCircuit_SideEffectingRHS and
// TestParity_SupEqual_ConstFold_TinyNegative), so the regression-proof check
// is driven here, at the visitor / opcode-stream level.
// ============================================================================

namespace {
    //  Tree-building helpers. We hand-build the AST (instead of going through
    //  the parser) for two reasons:
    //    - the #5 SupEqual const-fold is unreachable through the public
    //      PreProcess -> Compile pipeline (ConstCondProcess crisps fully-const
    //      conditions first); only a direct visitor-level test reaches it;
    //    - the #2 short-circuit observability needs an RHS whose execution is
    //      detectable, which we get from an assignment whose marker variable
    //      we read back after EvalCompiled.
    std::unique_ptr<Node_> MakeConstExpr(double v) {
        return MakeBaseNode<NodeConst_>(v);
    }

    //  A bare variable read at index idx. NodeVar_'s ctor leaves isConst_=true
    //  (because some downstream visitors expect it); flip it so the compiler
    //  emits a real Var read instead of a Const fold.
    std::unique_ptr<Node_> MakeVarExpr(int idx) {
        auto v = MakeBaseNode<NodeVar_>(String_("v"));
        Downcast<NodeVar_>(v)->index_ = idx;
        Downcast<ExprNode_>(v)->isConst_ = false;
        return v;
    }

    //  (lhsExpr >= 0): controllable LHS condition for And tests.
    std::unique_ptr<Node_> MakeSupEqualZero(std::unique_ptr<Node_> lhsExpr) {
        auto sup = MakeBaseNode<NodeSupEqual_>();
        sup->arguments_.Resize(2);
        sup->arguments_[0] = std::move(lhsExpr);
        sup->arguments_[1] = MakeConstExpr(0.0);
        return sup;
    }

    //  RHS that, when executed, writes 5 into variable index rhsIdx. The
    //  marker is the observable side effect proving whether the RHS sub-stream
    //  was evaluated.
    std::unique_ptr<Node_> MakeRhsAssign(int rhsIdx) {
        auto assign = MakeBaseNode<NodeAssign_>();
        assign->arguments_.Resize(2);
        auto var = MakeBaseNode<NodeVar_>(String_("rhs_marker"));
        Downcast<NodeVar_>(var)->index_ = rhsIdx;
        assign->arguments_[0] = std::move(var);
        assign->arguments_[1] = MakeConstExpr(5.0);
        return assign;
    }

    //  Build a binary node of type NodeT taking ownership of lhs and rhs.
    template <class NodeT>
    std::unique_ptr<Node_> MakeBinaryNode(std::unique_ptr<Node_> lhs, std::unique_ptr<Node_> rhs) {
        auto top = MakeBaseNode<NodeT>();
        top->arguments_.Resize(2);
        top->arguments_[0] = std::move(lhs);
        top->arguments_[1] = std::move(rhs);
        return top;
    }
} // namespace

//  #2 And short-circuit. The compiled stream must skip the RHS sub-stream when
//  the LHS is false; the RHS (an assignment) must not mutate state when
//  skipped. We assert two things:
//    (a) by inspecting nodeStream_: the AndIfFalse opcode is emitted with a
//        non-trivial jump target (RHS sub-stream length is the gap);
//    (b) by running EvalCompiled with two scenarios: LHS false (RHS skipped,
//        marker unchanged) and LHS true (RHS executed, marker = 5).
TEST(ScriptTest, TestAndShortCircuit_SkipsRhsWhenLhsFalse) {
    constexpr int kLhsSrc = 0;
    constexpr int kRhsMarker = 1;

    //  --- (a) stream inspection ---
    auto andA = MakeBinaryNode<NodeAnd_>(MakeSupEqualZero(MakeVarExpr(kLhsSrc)), MakeRhsAssign(kRhsMarker));
    Compiler_ compilerA;
    andA->Accept(compilerA);
    const Vector_<int>& stream = compilerA.NodeStream();

    bool sawAndIfFalse = false;
    int jumpSlot = -1;
    for (size_t i = 0; i + 1 < stream.size(); ++i) {
        if (stream[i] == NodeType_::AndIfFalse) {
            sawAndIfFalse = true;
            jumpSlot = stream[i + 1];
            break;
        }
    }
    ASSERT_TRUE(sawAndIfFalse) << "expected AndIfFalse opcode in nodeStream";
    ASSERT_GT(jumpSlot, 0) << "AndIfFalse jump target must be non-trivial";

    Scenario_<double> scenario(1);
    scenario[0].spot_ = 0.0;
    scenario[0].numeraire_ = 1.0;

    //  --- (b1) LHS false -> RHS skipped, marker unchanged ---
    auto andB1 = MakeBinaryNode<NodeAnd_>(MakeSupEqualZero(MakeVarExpr(kLhsSrc)), MakeRhsAssign(kRhsMarker));
    Compiler_ compilerB1;
    andB1->Accept(compilerB1);

    EvalState_<double> stateFalse(Vector_<>{0.0, 0.0});
    stateFalse.variables_[kLhsSrc] = -1.0; // (v >= 0) FALSE
    stateFalse.variables_[kRhsMarker] = 0.0;
    EvalCompiled(compilerB1.NodeStream(), compilerB1.ConstStream(), compilerB1.DataStream(), scenario[0], stateFalse);
    ASSERT_DOUBLE_EQ(stateFalse.variables_[kRhsMarker], 0.0)
        << "RHS assignment executed despite LHS being false - short-circuit lost";

    //  --- (b2) LHS true -> RHS executed, marker = 5 ---
    auto andB2 = MakeBinaryNode<NodeAnd_>(MakeSupEqualZero(MakeVarExpr(kLhsSrc)), MakeRhsAssign(kRhsMarker));
    Compiler_ compilerB2;
    andB2->Accept(compilerB2);

    EvalState_<double> stateTrue(Vector_<>{0.0, 0.0});
    stateTrue.variables_[kLhsSrc] = 1.0; // (v >= 0) TRUE
    stateTrue.variables_[kRhsMarker] = 0.0;
    EvalCompiled(compilerB2.NodeStream(), compilerB2.ConstStream(), compilerB2.DataStream(), scenario[0], stateTrue);
    ASSERT_DOUBLE_EQ(stateTrue.variables_[kRhsMarker], 5.0)
        << "RHS assignment did NOT execute when LHS was true - over-aggressive short-circuit";
}

//  #2 Or short-circuit, symmetric: when LHS is true the RHS must be skipped.
TEST(ScriptTest, TestOrShortCircuit_SkipsRhsWhenLhsTrue) {
    constexpr int kLhsSrc = 0;
    constexpr int kRhsMarker = 1;

    //  --- (a) stream inspection ---
    auto orA = MakeBinaryNode<NodeOr_>(MakeSupEqualZero(MakeVarExpr(kLhsSrc)), MakeRhsAssign(kRhsMarker));
    Compiler_ compilerA;
    orA->Accept(compilerA);
    const Vector_<int>& stream = compilerA.NodeStream();

    bool sawOrIfTrue = false;
    int jumpSlot = -1;
    for (size_t i = 0; i + 1 < stream.size(); ++i) {
        if (stream[i] == NodeType_::OrIfTrue) {
            sawOrIfTrue = true;
            jumpSlot = stream[i + 1];
            break;
        }
    }
    ASSERT_TRUE(sawOrIfTrue) << "expected OrIfTrue opcode in nodeStream";
    ASSERT_GT(jumpSlot, 0) << "OrIfTrue jump target must be non-trivial";

    Scenario_<double> scenario(1);
    scenario[0].spot_ = 0.0;
    scenario[0].numeraire_ = 1.0;

    //  --- (b1) LHS true -> RHS skipped, marker unchanged ---
    auto orB1 = MakeBinaryNode<NodeOr_>(MakeSupEqualZero(MakeVarExpr(kLhsSrc)), MakeRhsAssign(kRhsMarker));
    Compiler_ compilerB1;
    orB1->Accept(compilerB1);

    EvalState_<double> stateTrue(Vector_<>{0.0, 0.0});
    stateTrue.variables_[kLhsSrc] = 1.0; // (v >= 0) TRUE
    stateTrue.variables_[kRhsMarker] = 0.0;
    EvalCompiled(compilerB1.NodeStream(), compilerB1.ConstStream(), compilerB1.DataStream(), scenario[0], stateTrue);
    ASSERT_DOUBLE_EQ(stateTrue.variables_[kRhsMarker], 0.0)
        << "RHS assignment executed despite LHS being true - short-circuit lost";

    //  --- (b2) LHS false -> RHS executed, marker = 5 ---
    auto orB2 = MakeBinaryNode<NodeOr_>(MakeSupEqualZero(MakeVarExpr(kLhsSrc)), MakeRhsAssign(kRhsMarker));
    Compiler_ compilerB2;
    orB2->Accept(compilerB2);

    EvalState_<double> stateFalse(Vector_<>{0.0, 0.0});
    stateFalse.variables_[kLhsSrc] = -1.0; // (v >= 0) FALSE
    stateFalse.variables_[kRhsMarker] = 0.0;
    EvalCompiled(compilerB2.NodeStream(), compilerB2.ConstStream(), compilerB2.DataStream(), scenario[0], stateFalse);
    ASSERT_DOUBLE_EQ(stateFalse.variables_[kRhsMarker], 5.0)
        << "RHS assignment did NOT execute when LHS was false - over-aggressive short-circuit";
}

//  #5 SupEqual const-fold. VisitCondition<SupEqual> must fold to the SAME
//  boolean as the runtime opcode (x >= 0), not the historical (x > -EPSILON).
//  We test the predicate at the visitor level because PreProcess crisps
//  fully-constant conditions before Compile() emits a stream, so the fold is
//  not reachable through the public pipeline. The visit emits True/False
//  directly into nodeStream_; we read it back to recover the folded boolean.
TEST(ScriptTest, TestSupEqualConstFold_TinyNegativeIsFalse_TinyPositiveIsTrue) {
    auto FoldSupEqual = [](double constVal) {
        auto sup = MakeBaseNode<NodeSupEqual_>();
        sup->arguments_.Resize(2);
        sup->arguments_[0] = MakeConstExpr(constVal);
        sup->arguments_[1] = MakeConstExpr(0.0);
        Compiler_ compiler;
        sup->Accept(compiler);
        REQUIRE(!compiler.NodeStream().empty(), "expected non-empty folded stream");
        return compiler.NodeStream()[0];
    };

    //  Tiny-negative (in (-EPSILON, 0)) must fold to FALSE under x >= 0.
    //  Historically it folded to TRUE under x > -EPSILON - the defect.
    ASSERT_EQ(FoldSupEqual(-1e-16), static_cast<int>(NodeType_::False))
        << "tiny-negative const SupEqual folded to True (x > -EPSILON); expected False (x >= 0)";
    //  Tiny-positive must still fold to TRUE.
    ASSERT_EQ(FoldSupEqual(1e-16), static_cast<int>(NodeType_::True));
    //  Boundary: exactly zero must fold to TRUE (x >= 0 at x == 0).
    ASSERT_EQ(FoldSupEqual(0.0), static_cast<int>(NodeType_::True));
    //  Clearly-negative must fold to FALSE.
    ASSERT_EQ(FoldSupEqual(-1.0), static_cast<int>(NodeType_::False));
    //  Clearly-positive must fold to TRUE.
    ASSERT_EQ(FoldSupEqual(1.0), static_cast<int>(NodeType_::True));
}

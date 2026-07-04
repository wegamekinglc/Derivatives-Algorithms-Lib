//
// Created by wegame on 2026/07/04.
//
// Phase 0 parity harness for the compiled vs tree-walk script evaluators.
// Pins the defect register from
// .claude/specs/script-compiled-evaluator-alignment.md. Tests confirmed RED
// at runtime are marked DISABLED_ so the merge leaves CI green; each later
// fix phase removes the prefix as it turns its pin green.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/math/aad/aad.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/storage/globals.hpp>
#include <dal/script/event.hpp>
#include <dal/script/simulation.hpp>

using namespace Dal;
using namespace Dal::AAD;
using namespace Dal::Script;

namespace {
    // Build a vanilla product from a single payoff string at one future event.
    // The STRIKE const variable mirrors the test_blackscholes convention so
    // const-variable seams are exercised even in the baseline.
    struct VanillaProduct_ {
        Date_ exerciseDate;
        double strike;
        String_ payoffBody;
        Vector_<Cell_> eventDates;
        Vector_<String_> events;

        VanillaProduct_(const Date_& ex, double k, const String_& body)
        : exerciseDate(ex), strike(k), payoffBody(body) {
            eventDates.push_back(Cell_(String_("STRIKE")));
            events.push_back(ToString(k));
            eventDates.push_back(Cell_(exerciseDate));
            events.push_back(payoffBody);
        }

        ScriptProduct_ Build() const { return ScriptProduct_(eventDates, events); }
    };

    // Per-path parity (strictest): drive Evaluate (tree-walk) and
    // EvaluateCompiled on the SAME deterministic scenario, compare the payoff
    // slot. Sobol is irrelevant here — the only input is the scenario we hand
    // to both evaluators, so any divergence is a pure evaluator difference.
    void AssertPerPathParity(const ScriptProduct_& product, double spot, double tol = 1e-8) {
        Scenario_<double> scenario(product.EventDates().size());
        for (auto& s : scenario) {
            s.spot_ = spot;
            s.numeraire_ = 1.0;
        }

        // Tree-walk uses Evaluator_<double>; compiled uses EvalState_<double>.
        // We invoke both via the product's two evaluation entry points so the
        // comparison reflects the actual MCSimulation code paths.
        Evaluator_<double> treeEval = product.BuildEvaluator<double>();
        product.Evaluate(scenario, treeEval);
        const double treePayoff = treeEval.VarVals()[product.PayOffIdx()];

        EvalState_<double> compiledState = product.BuildEvalState<double>();
        product.EvaluateCompiled(scenario, compiledState);
        const double compiledPayoff = compiledState.VarVals()[product.PayOffIdx()];

        ASSERT_NEAR(compiledPayoff, treePayoff, tol)
            << "per-path payoff divergence at spot=" << spot;
    }

    // Aggregated parity: same Sobol seed and path count flow into both arms
    // (MCSimulation's per-batch SkipTo(firstPath) guarantees identical deviates
    // given the same rsg string). Compares aggregated PV only.
    void AssertAggregatedParityDouble(const ScriptProduct_& product,
                                      const Handle_<ModelData_>& model,
                                      size_t nPaths,
                                      double tol = 1e-8) {
        const SimResults_ treeWalk = MCSimulation<double>(product, model, nPaths, "sobol", false, false);
        const SimResults_ compiled = MCSimulation<double>(product, model, nPaths, "sobol", false, true);
        ASSERT_NEAR(compiled.aggregated_, treeWalk.aggregated_, tol)
            << "aggregated PV divergence (treeWalk=" << treeWalk.aggregated_ << ")";
    }

    Handle_<ModelData_> StandardBSModel(double spot, double vol, double rate, double div) {
        return Handle_<ModelData_>(new BSModelData_("bsmodel", spot, vol, rate, div));
    }
} // namespace

// ============================================================================
// LIVE baselines (must pass today)
// ============================================================================

TEST(ScriptCompiledParityTest, TestParity_VanillaCall) {
    Global::Dates_::SetEvaluationDate(Date_(2022, 6, 22));
    Date_ exerciseDate(2024, 6, 21);
    VanillaProduct_ vanilla(exerciseDate, 11.0, "call pays MAX(spot() - STRIKE, 0.0)");
    ScriptProduct_ product = vanilla.Build();
    product.PreProcess(false, false);
    product.Compile();

    // Per-path: in-the-money, at-the-money, out-of-the-money.
    {
        SCOPED_TRACE("ITM");
        AssertPerPathParity(product, 13.0);
    }
    {
        SCOPED_TRACE("ATM");
        AssertPerPathParity(product, 11.0);
    }
    {
        SCOPED_TRACE("OTM");
        AssertPerPathParity(product, 9.0);
    }

    // Aggregated PV parity over a Sobol Monte Carlo.
    auto model = StandardBSModel(10.0, 0.20, 0.034, 0.021);
    AssertAggregatedParityDouble(product, model, 4096);
}

// Pins #13: tree-walk PV must be identical before vs after Compile() mutates
// the shared AST (Compile() runs ConstProcess first). If this turns RED the
// Compile() mutation has broken tree-walk semantics.
TEST(ScriptCompiledParityTest, TestParity_TreeWalkUnchangedAfterCompile) {
    Global::Dates_::SetEvaluationDate(Date_(2022, 6, 22));
    Date_ exerciseDate(2024, 6, 21);
    VanillaProduct_ vanilla(exerciseDate, 11.0, "call pays MAX(spot() - STRIKE, 0.0)");
    ScriptProduct_ product = vanilla.Build();
    product.PreProcess(false, false);

    auto model = StandardBSModel(10.0, 0.20, 0.034, 0.021);
    const SimResults_ before = MCSimulation<double>(product, model, 2048, "sobol", false, false);

    product.Compile();

    // Compile() does not touch variableValues_ / timeLine_; the tree-walk
    // evaluator ignores isConst_.
    const SimResults_ after = MCSimulation<double>(product, model, 2048, "sobol", false, false);

    ASSERT_NEAR(after.aggregated_, before.aggregated_, 1e-8)
        << "tree-walk PV changed after Compile(); Compile() must not mutate the AST in a tree-walk-visible way";
}

// Pins past-events init seeding: evaluation date mid-schedule so PastEvaluate()
// seeds variables_ on both evaluators identically. Today this is a guard.
TEST(ScriptCompiledParityTest, TestParity_PastEvents_InitSeeding) {
    // Two past fixings + one future event. The past fixings seed the initial
    // variable values; both evaluators must use them identically.
    Global::Dates_::SetEvaluationDate(Date_(2022, 6, 22));
    Vector_<Cell_> eventDates;
    Vector_<String_> events;
    eventDates.push_back(Cell_(String_("K"))); events.push_back("11.0");
    eventDates.push_back(Cell_(Date_(2022, 6, 20))); events.push_back("prevA = 5.0");
    eventDates.push_back(Cell_(Date_(2022, 6, 21))); events.push_back("prevB = 7.0");
    eventDates.push_back(Cell_(Date_(2024, 6, 21)));
    events.push_back("call pays MAX(spot() + prevA + prevB - K, 0.0)");

    ScriptProduct_ product(eventDates, events);
    product.PreProcess(false, false);
    product.Compile();

    // Per-path: the past-seeded prevA/prevB (5, 7) must read identically.
    AssertPerPathParity(product, 1.0);

    auto model = StandardBSModel(10.0, 0.20, 0.034, 0.021);
    AssertAggregatedParityDouble(product, model, 2048);
}

// ============================================================================
// DISABLED_ defect-pin tests (RED today; one per defect register row)
// ============================================================================

// #1 IfElse true-branch bStack underflow. EvalCompiled resets the thread_local
// bStack on the recursive call for the true branch, then the parent's
// bStack.Pop() underflows sp_. A condition that is TRUE triggers it. Confirmed
// RED under UBSAN: dal-cpp/dal/math/stacks.hpp:133,135 "index -1 out of bounds
// for type 'bool [128]'" (the parent Pop after the recursive true branch).
// UB today; enabled by Phase 1a.
TEST(ScriptCompiledParityTest, DISABLED_TestParity_IfElse_ConsecutiveBothTrue) {
    Global::Dates_::SetEvaluationDate(Date_(2023, 1, 1));
    Vector_<Cell_> eventDates{Cell_(Date_(2023, 1, 28))};
    Vector_<String_> events{R"(
        y = 0
        IF spot() >= 0 THEN
            y = 1
        ELSE
            y = 2
        END
        z = 0
        IF spot() >= 0 THEN
            z = 3
        ELSE
            z = 4
        END
        out pays y + z
    )"};
    ScriptProduct_ product(eventDates, events);
    product.PreProcess(false, false);
    product.Compile();
    AssertPerPathParity(product, 5.0);
}

// #1 nested: the recursive EvalCompiled in the true branch shares the parent's
// thread_local dStack/bStack; a nested IfElse whose outer condition is true
// stresses the same shared-stack underflow via recursion depth.
TEST(ScriptCompiledParityTest, DISABLED_TestParity_IfElse_NestedInTrueBranch) {
    Global::Dates_::SetEvaluationDate(Date_(2023, 1, 1));
    Vector_<Cell_> eventDates{Cell_(Date_(2023, 1, 28))};
    Vector_<String_> events{R"(
        IF spot() >= 0 THEN
            IF spot() >= 1 THEN
                y = 10
            ELSE
                y = 20
            END
        ELSE
            y = 30
        END
        out pays y
    )"};
    ScriptProduct_ product(eventDates, events);
    product.PreProcess(false, false);
    product.Compile();
    AssertPerPathParity(product, 5.0);
}

// #2 short-circuit loss in compiled. Tree-walk short-circuits &&/|| so the RHS
// is never evaluated; compiled emits both sub-streams unconditionally. The
// divergence is observable only when the RHS has a detectable side effect
// (throw or state mutation). The script grammar's && RHS is a pure condition,
// and C++ std::log of a non-positive returns NaN/-inf without throwing, so the
// &&/|| result converges on the same boolean on both evaluators — confirmed
// LIVE (PASSED) at spot=5.0 in both arms. The per-path harness cannot cleanly
// construct a divergence. Phase 1b's fix (jump opcodes so the RHS sub-stream
// is skipped when the LHS decides the boolean) is verified by a direct
// Compiler_ unit test in that phase; this pin guards against regression once
// the fix lands and the fuzz layer (test_compile_parity_fuzz.cpp) hits it.
TEST(ScriptCompiledParityTest, DISABLED_TestParity_And_OrShortCircuit_SideEffectingRHS) {
    Global::Dates_::SetEvaluationDate(Date_(2023, 1, 1));
    Vector_<Cell_> eventDates{Cell_(Date_(2023, 1, 28))};
    Vector_<String_> events{R"(
        IF spot() < 0 AND log(spot()) > 1 THEN
            y = 1
        ELSE
            y = 2
        END
        out pays y
    )"};
    ScriptProduct_ product(eventDates, events);
    product.PreProcess(false, false);
    product.Compile();
    // spot > 0 → LHS false. Both arms yield y=2 today (RHS converges to a
    // boolean even when evaluated). This guard catches regressions but does
    // not by itself prove short-circuit equivalence — see the comment above.
    AssertPerPathParity(product, 5.0);
}

// #5 SupEqual const-fold edge. The compiler's VisitCondition<SupEqual>
// const-folds via (x > -EPSILON) while the runtime SupEqual opcode uses
// (x >= 0) — divergent for x in (-EPSILON, 0). Constructing a per-path
// divergence is delicate: PreProcess(false, false) runs ConstCondProcess
// which collapses a fully-constant condition (using domain.hpp's
// IsPositive/IsNegative, also EPSILON-based) BEFORE Compile() emits the
// opcode stream, so the compiler's VisitCondition<SupEqual> fold is only
// reachable when both children appear const to the compiler but the
// surrounding IF survives ConstCondProcess. The script tokenizer
// ([\w.]+|[/-]) rejects scientific notation. Enabled by Phase 1b; the fix
// (x > -EPSILON -> x >= 0) is verified directly by a Compiler_ unit test
// in that phase. This pin guards against regression once green.
TEST(ScriptCompiledParityTest, DISABLED_TestParity_SupEqual_ConstFold_TinyNegative) {
    Global::Dates_::SetEvaluationDate(Date_(2023, 1, 1));
    Vector_<Cell_> eventDates{Cell_(Date_(2023, 1, 28))};
    Vector_<String_> events{R"(
        x = -0.000000000000001
        IF x >= 0 THEN
            y = 1
        ELSE
            y = 2
        END
        out pays y
    )"};
    ScriptProduct_ product(eventDates, events);
    product.PreProcess(false, false);
    product.Compile();
    AssertPerPathParity(product, 5.0);
}

// #4 NodeNot_ / visitNot typo on the <Number_> fuzzy path. The lower-case
// visitNot does not override the CRTP Visit; falls through to base which pops
// an empty bStack_. A != in a fuzzy-evaluated product breaks. Confirmed RED
// under UBSAN: dal-cpp/dal/math/stacks.hpp:135 "index -1 out of bounds for
// type 'bool [128]'" plus a load of an invalid bool (the popped garbage)
// at dal-cpp/dal/model/blackscholes.hpp:26. <Number_> only. Enabled by
// Phase 2.
TEST(ScriptCompiledParityTest, DISABLED_TestParity_Number_NotEqual_Fuzzy) {
    Global::Dates_::SetEvaluationDate(Date_(2022, 6, 22));
    Date_ exerciseDate(2024, 6, 21);
    Vector_<Cell_> eventDates{Cell_(String_("K")), Cell_(exerciseDate)};
    Vector_<String_> events{"11.0", R"(
        v = 0.0
        IF spot() != K THEN
            v = MAX(spot() - K, 0.0)
        ELSE
            v = 0.0
        END
        out pays v
    )"};
    ScriptProduct_ product(eventDates, events);
    int maxNested = product.PreProcess(true, false);
    product.Compile();

    auto model = StandardBSModel(10.0, 0.20, 0.034, 0.021);
    const SimResults_ treeWalk = MCSimulation<Number_>(product, model, 2048, "sobol", false, false, maxNested);
    const SimResults_ compiled = MCSimulation<Number_>(product, model, 2048, "sobol", false, true, maxNested);
    ASSERT_NEAR(compiled.aggregated_, treeWalk.aggregated_, 1e-8);
}

// #6 / #3 guard. compiled=true without Compile() indexes the empty
// nodeStreams_ — out-of-bounds UB today. We assert the THROW guard that does
// not exist yet; the test is DISABLED because today's behavior is UB, never
// run it un-sanitized. Guard added in Phase 3.
TEST(ScriptCompiledParityTest, DISABLED_TestParity_CompileNotCalled_GuardsThrow) {
    Global::Dates_::SetEvaluationDate(Date_(2022, 6, 22));
    Date_ exerciseDate(2024, 6, 21);
    VanillaProduct_ vanilla(exerciseDate, 11.0, "call pays MAX(spot() - STRIKE, 0.0)");
    ScriptProduct_ product = vanilla.Build();
    product.PreProcess(false, false);
    // NOTE: Compile() deliberately NOT called.

    auto model = StandardBSModel(10.0, 0.20, 0.034, 0.021);
    // Today: indexes empty nodeStreams_, UB. Phase 3 will add the guard that
    // makes this THROW cleanly.
    ASSERT_THROW(MCSimulation<double>(product, model, 64, "sobol", false, true), Dal::Exception_);
}

// #11 const variables dead in compiled path. The compiler bakes constVar
// values into constStream_ as plain doubles; the ConstVar opcode falls through
// to Const; VisitBinary const-folds whole sub-expressions containing const
// vars away. Result: const-var adjoints are always exactly zero on the AAD
// compiled path while tree-walk records them. Confirmed RED: compiled
// risks_[nParams]=0, tree-walk risks_[nParams]=-0.326 (dP/dSTRIKE). Enabled
// by Phase 1c.
TEST(ScriptCompiledParityTest, DISABLED_TestParity_Number_ConstVarRisks) {
    Global::Dates_::SetEvaluationDate(Date_(2022, 6, 22));
    Date_ exerciseDate(2024, 6, 21);
    Vector_<Cell_> eventDates{Cell_(String_("STRIKE")), Cell_(exerciseDate)};
    Vector_<String_> events{"11.0", "call pays MAX(spot() - STRIKE, 0.0)"};
    ScriptProduct_ product(eventDates, events);
    int maxNested = product.PreProcess(true, false);
    product.Compile();

    auto model = StandardBSModel(10.0, 0.20, 0.034, 0.021);
    const SimResults_ treeWalk = MCSimulation<Number_>(product, model, 4096, "sobol", false, false, maxNested);
    const SimResults_ compiled = MCSimulation<Number_>(product, model, 4096, "sobol", false, true, maxNested);

    ASSERT_NEAR(compiled.aggregated_, treeWalk.aggregated_, 1e-6);

    // Const-var risk (STRIKE) is risks_[nParams] (nParams = 4 for BS:
    // spot, vol, rate, div). Tree-walk records dP/dSTRIKE; compiled is zero.
    const size_t nParams = 4;
    ASSERT_NEAR(compiled.risks_[nParams], treeWalk.risks_[nParams], 1e-8)
        << "const-var STRIKE greek: compiled (" << compiled.risks_[nParams]
        << ") vs tree-walk (" << treeWalk.risks_[nParams] << ")";
}

// #11 mutation seam. EvalState_::ConstVarVals() is meant to be mutable so
// callers can tweak const vars post-build; on the compiled path it is a
// no-op (the value is baked into constStream_). Confirmed RED at spot=13.0:
// compiled payoff=2 (uses baked STRIKE=11 → max(13-11,0)=2), tree-walk
// payoff=8 (respects mutated STRIKE=5 → max(13-5,0)=8). Enabled by Phase 1c.
TEST(ScriptCompiledParityTest, DISABLED_TestParity_ConstVarVals_MutationSeam) {
    Global::Dates_::SetEvaluationDate(Date_(2022, 6, 22));
    Date_ exerciseDate(2024, 6, 21);
    Vector_<Cell_> eventDates{Cell_(String_("STRIKE")), Cell_(exerciseDate)};
    Vector_<String_> events{"11.0", "call pays MAX(spot() - STRIKE, 0.0)"};
    ScriptProduct_ product(eventDates, events);
    product.PreProcess(false, false);
    product.Compile();

    // Build both states, mutate ConstVarVals on each, assert the payoff
    // responds identically. On the compiled path the mutation is invisible
    // today (baked into constStream_).
    Scenario_<double> scenario(1);
    scenario[0].spot_ = 13.0;
    scenario[0].numeraire_ = 1.0;

    Evaluator_<double> treeEval = product.BuildEvaluator<double>();
    treeEval.ConstVarVals()[0] = 5.0; // STRIKE 11 -> 5
    product.Evaluate(scenario, treeEval);
    const double treePayoff = treeEval.VarVals()[product.PayOffIdx()];

    EvalState_<double> compiledState = product.BuildEvalState<double>();
    compiledState.ConstVarVals()[0] = 5.0;
    product.EvaluateCompiled(scenario, compiledState);
    const double compiledPayoff = compiledState.VarVals()[product.PayOffIdx()];

    ASSERT_NEAR(compiledPayoff, treePayoff, 1e-8);
}

// #10 NodeCollect_. PreProcess(false, false) runs ConstCondProcess which
// collapses always-true conditions into NodeCollect_; the compiled product
// routinely contains NodeCollect_ at Compile() time. Both Compiler_ and
// Evaluator_ handle it only via the accidental ConstVisitor_ catch-all
// traversal. Confirmed LIVE (PASSES today) — the catch-all handles it
// correctly. Kept live as a regression guard against future pruning.
TEST(ScriptCompiledParityTest, TestParity_ConstCondProcessed_Collect) {
    Global::Dates_::SetEvaluationDate(Date_(2023, 1, 1));
    Vector_<Cell_> eventDates{Cell_(Date_(2023, 1, 28))};
    // Always-true condition collapses to NodeCollect_ during PreProcess.
    Vector_<String_> events{R"(
        IF 2 >= 1 THEN
            x = spot() + 3
        END
    )"};
    // The default payoff slot is the last variable; the ConstCondProcessor
    // replaces the always-true IF with a NodeCollect_ around the assignment,
    // which both evaluators handle via the ConstVisitor_ catch-all traversal.
    ScriptProduct_ product(eventDates, events, "x");
    product.PreProcess(false, false);
    product.Compile();
    AssertPerPathParity(product, 5.0);
}

// #12 const-folded conditions vs fuzzy (5b gate). Compiler_::VisitCondition
// folds constant conditions to hard True/False; FuzzyEvaluator_ would push
// a fractional dt (CSpr/BFly) when |const| < eps/2. Requires the compiled
// fuzzy path which does not exist yet (no Smooth/CSpr/BFly opcodes); cannot
// be constructed without Phase 5b. Left disabled.
TEST(ScriptCompiledParityTest, DISABLED_TestParity_Number_ConstCondition_WithinEps) {
    // Construction needs compiled-fuzzy opcodes (CSpr/BFly/FuzzyIf) that do
    // not ship until Phase 5b. The pin is staged here so 5b can enable it.
    GTEST_SKIP() << "Phase 5b blocker: requires compiled-fuzzy opcodes (CSpr/BFly/FuzzyIf)";
}

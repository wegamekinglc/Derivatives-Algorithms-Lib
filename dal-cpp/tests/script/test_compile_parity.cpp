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
#include <dal/script/visitor/compiler.hpp>

#include <set>
#include <string>

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

        // Tree-walk uses Evaluator_<double>; compiled uses EvalState_<double>
        // over the ScriptCompiled_ artifact. We invoke both via the actual
        // MCSimulation code paths' entry points.
        Evaluator_<double> treeEval = product.BuildEvaluator<double>();
        product.Evaluate(scenario, treeEval);
        const double treePayoff = treeEval.VarVals()[product.PayOffIdx()];

        const ScriptCompiled_ compiled = product.Compile();
        EvalState_<double> compiledState = product.BuildEvalState<double>();
        compiled.Evaluate(scenario, compiledState);
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

    // Aggregated <Number_> parity: compiled-fuzzy vs tree-walk-fuzzy under the
    // same Sobol seed, path count, eps and maxNestedIfs. PV and every AAD
    // risk (model params + const variables) must agree to tol.
    void AssertAggregatedParityNumber(const ScriptProduct_& product,
                                      const Handle_<ModelData_>& model,
                                      size_t nPaths,
                                      int maxNestedIfs,
                                      double eps,
                                      double tol = 1e-8) {
        const SimResults_ treeWalk = MCSimulation<Number_>(product, model, nPaths, "sobol", false, false, maxNestedIfs, eps);
        const SimResults_ compiled = MCSimulation<Number_>(product, model, nPaths, "sobol", false, true, maxNestedIfs, eps);
        ASSERT_NEAR(compiled.aggregated_, treeWalk.aggregated_, tol)
            << "aggregated <Number_> PV divergence (treeWalk=" << treeWalk.aggregated_ << ")";
        ASSERT_EQ(compiled.risks_.size(), treeWalk.risks_.size());
        for (size_t j = 0; j < treeWalk.risks_.size(); ++j)
            ASSERT_NEAR(compiled.risks_[j], treeWalk.risks_[j], tol)
                << "risk divergence at index " << j << " (treeWalk=" << treeWalk.risks_[j] << ")";
    }

    // A discretely monitored up-and-out call: two const variables, an init
    // event, two monitoring events with knock-out IFs, and a final payoff.
    // Shared by the barrier parity test and the golden pin.
    ScriptProduct_ FixedBarrierProduct() {
        Vector_<Cell_> eventDates;
        Vector_<String_> events;
        eventDates.push_back(Cell_(String_("STRIKE"))); events.push_back("11.0");
        eventDates.push_back(Cell_(String_("BARRIER"))); events.push_back("15.0");
        eventDates.push_back(Cell_(Date_(2022, 9, 21))); events.push_back("alive = 1");
        eventDates.push_back(Cell_(Date_(2023, 3, 21)));
        events.push_back("IF spot() >= BARRIER THEN alive = 0 END");
        eventDates.push_back(Cell_(Date_(2023, 9, 21)));
        events.push_back("IF spot() >= BARRIER THEN alive = 0 END");
        eventDates.push_back(Cell_(Date_(2024, 6, 21)));
        events.push_back("call pays alive * MAX(spot() - STRIKE, 0.0)");
        return ScriptProduct_(eventDates, events, "call");
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

// Pins #13: tree-walk PV must be identical before vs after Compile() (and
// after PreProcess's ConstProcess pass, which marks isConst_/constVal_ on the
// shared AST). If this turns RED, compilation has broken tree-walk semantics.
TEST(ScriptCompiledParityTest, TestParity_TreeWalkUnchangedAfterCompile) {
    Global::Dates_::SetEvaluationDate(Date_(2022, 6, 22));
    Date_ exerciseDate(2024, 6, 21);
    VanillaProduct_ vanilla(exerciseDate, 11.0, "call pays MAX(spot() - STRIKE, 0.0)");
    ScriptProduct_ product = vanilla.Build();
    product.PreProcess(false, false);

    auto model = StandardBSModel(10.0, 0.20, 0.034, 0.021);
    const SimResults_ before = MCSimulation<double>(product, model, 2048, "sobol", false, false);

    const ScriptCompiled_ compiled = product.Compile();

    // Compile() is const and produces a separate artifact; the tree-walk
    // evaluator ignores isConst_.
    const SimResults_ after = MCSimulation<double>(product, model, 2048, "sobol", false, false);

    ASSERT_NEAR(after.aggregated_, before.aggregated_, 1e-8)
        << "tree-walk PV changed after Compile(); Compile() must not mutate the AST in a tree-walk-visible way";
}

// #13 const-correct compilation. Compile() is const and produces a separate
// ScriptCompiled_ artifact (no member streams), so MCSimulation's
// const ScriptProduct_& compiles internally and `compiled` is a pure
// performance flag. Also pins the <double> default flip: an omitted flag
// must run compiled and match the explicit tree-walk PV.
TEST(ScriptCompiledParityTest, TestParity_ConstCompile_Artifact) {
    Global::Dates_::SetEvaluationDate(Date_(2022, 6, 22));
    Date_ exerciseDate(2024, 6, 21);
    VanillaProduct_ vanilla(exerciseDate, 11.0, "call pays MAX(spot() - STRIKE, 0.0)");
    ScriptProduct_ product = vanilla.Build();
    product.PreProcess(false, false);

    const ScriptProduct_& constRef = product;
    const ScriptCompiled_ compiled = constRef.Compile();

    Scenario_<double> scenario(1);
    scenario[0].spot_ = 13.0;
    scenario[0].numeraire_ = 1.0;

    EvalState_<double> state = constRef.BuildEvalState<double>();
    compiled.Evaluate(scenario, state);

    Evaluator_<double> treeEval = constRef.BuildEvaluator<double>();
    constRef.Evaluate(scenario, treeEval);
    ASSERT_NEAR(state.VarVals()[constRef.PayOffIdx()], treeEval.VarVals()[constRef.PayOffIdx()], 1e-8);

    auto model = StandardBSModel(10.0, 0.20, 0.034, 0.021);
    const SimResults_ defaulted = MCSimulation<double>(product, model, 2048, "sobol", false);
    const SimResults_ treeWalk = MCSimulation<double>(product, model, 2048, "sobol", false, false);
    ASSERT_NEAR(defaulted.aggregated_, treeWalk.aggregated_, 1e-8)
        << "<double> default (compiled) diverges from tree-walk";
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
TEST(ScriptCompiledParityTest, TestParity_IfElse_ConsecutiveBothTrue) {
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
    AssertPerPathParity(product, 5.0);
}

// #1 nested: the recursive EvalCompiled in the true branch shares the parent's
// thread_local dStack/bStack; a nested IfElse whose outer condition is true
// stresses the same shared-stack underflow via recursion depth.
TEST(ScriptCompiledParityTest, TestParity_IfElse_NestedInTrueBranch) {
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
    AssertPerPathParity(product, 5.0);
}

// #2 boolean-combinator semantics. Resolution (user-mandated): NO jump/skip
// opcodes in the compiled stream; instead the tree-walk Evaluator_ becomes
// EAGER on And/Or (both operands always evaluated), matching the compiled
// stream and FuzzyEvaluator_'s already-eager probability combinators
// (a*b, a+b-a*b). One eager semantics across all three evaluators; scripts
// must not rely on short-circuit (documented in docs/methodology/
// script_engine.md by Phase 1).
//
// This test is a GREEN guard, not a red pin: on the hard <double> path eager
// and short-circuit evaluation are observationally equivalent, because the
// grammar's conditions are pure and C++ LOG/SQRT of a non-positive quietly
// return NaN/-inf whose comparisons are false — the combined boolean absorbs
// the poisoned side (false AND NaN-cmp == false; true OR NaN-cmp == true).
// The test pins that equivalence across both combinators with a poisoned
// RHS at spots that decide the LHS both ways.
TEST(ScriptCompiledParityTest, TestParity_And_Or_EagerBothSides) {
    Global::Dates_::SetEvaluationDate(Date_(2023, 1, 1));
    Vector_<Cell_> eventDates{Cell_(Date_(2023, 1, 28))};
    Vector_<String_> events{R"(
        w = 0
        IF spot() > 3 AND spot() < 8 THEN
            w = 1
        ELSE
            w = 2
        END
        y = 0
        IF spot() < 1 AND LOG(spot() - 2) > 0 THEN
            y = 1
        ELSE
            y = 2
        END
        z = 0
        IF spot() > 1 OR LOG(spot() - 2) > 0 THEN
            z = 3
        ELSE
            z = 4
        END
        out pays w + y + z
    )"};
    ScriptProduct_ product(eventDates, events, "out");
    product.PreProcess(false, false);
    // spot=0.5: AND rhs poisoned (LOG(-1.5)); OR lhs false, rhs poisoned.
    // spot=5.0: AND both true; OR lhs true, rhs live.
    // spot=10.0: AND lhs true rhs false; OR lhs true.
    for (const double spot : {0.5, 5.0, 10.0}) {
        SCOPED_TRACE("spot=" + std::to_string(spot));
        AssertPerPathParity(product, spot);
    }
}

// #5 SupEqual const-fold edge. The compiler's VisitCondition<SupEqual>
// const-folds via (x > -EPSILON) while the runtime SupEqual opcode uses
// (x >= 0) — divergent for x in (-EPSILON, 0). PreProcess(false, true) skips
// ConstCondProcess (which would itself collapse the constant IF, via
// domain.hpp's equally EPSILON-based IsPositive/IsNegative, before Compile()
// ever emits opcodes) so the stream genuinely reaches the compiler's fold.
// Confirmed RED at head: x = -1e-15 lies inside (-EPSILON, 0), so the fold
// takes the TRUE branch (y=1) while the tree-walk computes -1e-15 >= 0 ==
// false (y=2). The script tokenizer rejects scientific notation, hence the
// long literal. Fixed in Phase 1 (fold is now x >= 0.0, matching the runtime
// opcode and the tree-walk).
TEST(ScriptCompiledParityTest, TestParity_SupEqual_ConstFold_TinyNegative) {
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
    ScriptProduct_ product(eventDates, events, "out");
    product.PreProcess(false, true);
    AssertPerPathParity(product, 5.0);
}

// #4 NodeNot_ / visitNot typo on the <Number_> fuzzy path. The lower-case
// visitNot does not override the CRTP Visit; falls through to base which pops
// an empty bStack_. A != in a fuzzy-evaluated product breaks. Confirmed RED
// under UBSAN: dal-cpp/dal/math/stacks.hpp:135 "index -1 out of bounds for
// type 'bool [128]'" plus a load of an invalid bool (the popped garbage)
// at dal-cpp/dal/model/blackscholes.hpp:26. <Number_> only. Enabled by
// Phase 2.
TEST(ScriptCompiledParityTest, TestParity_Number_NotEqual_Fuzzy) {
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

    auto model = StandardBSModel(10.0, 0.20, 0.034, 0.021);
    const SimResults_ treeWalk = MCSimulation<Number_>(product, model, 2048, "sobol", false, false, maxNested);
    const SimResults_ compiled = MCSimulation<Number_>(product, model, 2048, "sobol", false, true, maxNested);
    ASSERT_NEAR(compiled.aggregated_, treeWalk.aggregated_, 1e-8);
    ASSERT_EQ(compiled.risks_.size(), treeWalk.risks_.size());
    for (size_t j = 0; j < treeWalk.risks_.size(); ++j)
        ASSERT_NEAR(compiled.risks_[j], treeWalk.risks_[j], 1e-8) << "risk index " << j;
}

// #6 / #3 guard. Compile() on a product that was never PreProcessed (no
// variable indexing, no ConstProcess) must THROW rather than emit a garbage
// stream, both directly and inside MCSimulation (main thread; the pool tasks
// swallow exceptions). The guard predates this test's enablement, so the
// unguarded path was never executed.
TEST(ScriptCompiledParityTest, TestParity_CompileNotCalled_GuardsThrow) {
    Global::Dates_::SetEvaluationDate(Date_(2022, 6, 22));
    Date_ exerciseDate(2024, 6, 21);
    VanillaProduct_ vanilla(exerciseDate, 11.0, "call pays MAX(spot() - STRIKE, 0.0)");
    ScriptProduct_ product = vanilla.Build();
    // NOTE: PreProcess() deliberately NOT called.

    ASSERT_THROW(product.Compile(), Dal::Exception_);

    auto model = StandardBSModel(10.0, 0.20, 0.034, 0.021);
    ASSERT_THROW(MCSimulation<double>(product, model, 64, "sobol", false, true), Dal::Exception_);
}

// #11 const variables dead in compiled path. The compiler bakes constVar
// values into constStream_ as plain doubles; the ConstVar opcode falls through
// to Const; VisitBinary const-folds whole sub-expressions containing const
// vars away. Result: const-var adjoints are always exactly zero on the AAD
// compiled path while tree-walk records them. Confirmed RED: compiled
// risks_[nParams]=0, tree-walk risks_[nParams]=-0.326 (dP/dSTRIKE). Fixed in
// Phase 1: ConstVar is a real opcode reading state.constVariables_[idx],
// which InitModel4ParallelAAD puts on tape.
TEST(ScriptCompiledParityTest, TestParity_Number_ConstVarRisks) {
    Global::Dates_::SetEvaluationDate(Date_(2022, 6, 22));
    Date_ exerciseDate(2024, 6, 21);
    Vector_<Cell_> eventDates{Cell_(String_("STRIKE")), Cell_(exerciseDate)};
    Vector_<String_> events{"11.0", "call pays MAX(spot() - STRIKE, 0.0)"};
    ScriptProduct_ product(eventDates, events);
    int maxNested = product.PreProcess(true, false);

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
// payoff=8 (respects mutated STRIKE=5 → max(13-5,0)=8). Fixed in Phase 1.
TEST(ScriptCompiledParityTest, TestParity_ConstVarVals_MutationSeam) {
    Global::Dates_::SetEvaluationDate(Date_(2022, 6, 22));
    Date_ exerciseDate(2024, 6, 21);
    Vector_<Cell_> eventDates{Cell_(String_("STRIKE")), Cell_(exerciseDate)};
    Vector_<String_> events{"11.0", "call pays MAX(spot() - STRIKE, 0.0)"};
    ScriptProduct_ product(eventDates, events);
    product.PreProcess(false, false);
    const ScriptCompiled_ compiled = product.Compile();

    // Build both states, mutate ConstVarVals on each, assert the payoff
    // responds identically. On the compiled path the mutation used to be
    // invisible (baked into constStream_).
    Scenario_<double> scenario(1);
    scenario[0].spot_ = 13.0;
    scenario[0].numeraire_ = 1.0;

    Evaluator_<double> treeEval = product.BuildEvaluator<double>();
    treeEval.ConstVarVals()[0] = 5.0; // STRIKE 11 -> 5
    product.Evaluate(scenario, treeEval);
    const double treePayoff = treeEval.VarVals()[product.PayOffIdx()];

    EvalState_<double> compiledState = product.BuildEvalState<double>();
    compiledState.ConstVarVals()[0] = 5.0;
    compiled.Evaluate(scenario, compiledState);
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
    AssertPerPathParity(product, 5.0);
}

// #12 const conditions on the fuzzy path. Two layers, both pinned here:
// (a) pipeline: DomainProcessor_ is eps-blind — it marks domain-provable
//     const conditions alwaysTrue_/alwaysFalse_ and ConstCondProcessor_
//     rewrites them to NodeTrue_/NodeFalse_ in the SHARED AST, so both
//     evaluators see the same fold even when the constant (0.001 here) lies
//     inside the smoothing band (eps=0.5). That is the pipeline contract.
// (b) compiler: the surviving NodeTrue_/NodeFalse_ inside the combinators
//     must compile to fuzzy 1.0/0.0 pushes on the VALUE stack feeding
//     FuzzyAnd/FuzzyOr — never to the hard bool opcodes — and
//     Compiler_::VisitCondition must not hard-fold const condition args at
//     all in fuzzy mode (it emits the arg plus a smoothed comparison, which
//     is exactly what FuzzyEvaluator_ computes).
TEST(ScriptCompiledParityTest, TestParity_Number_ConstCondition_WithinEps) {
    Global::Dates_::SetEvaluationDate(Date_(2022, 6, 22));
    Vector_<Cell_> eventDates{Cell_(String_("K")), Cell_(Date_(2024, 6, 21))};
    Vector_<String_> events{"11.0", R"(
        v = 0.0
        IF 0.001 > 0 AND spot() > K THEN
            v = 1.0
        ELSE
            v = 2.0
        END
        IF 0 > 1 OR spot() > K THEN
            v = v + 0.5
        END
        out pays v
    )"};
    ScriptProduct_ product(eventDates, events, "out");
    const int maxNested = static_cast<int>(product.PreProcess(true, false));

    auto model = StandardBSModel(10.0, 0.20, 0.034, 0.021);
    AssertAggregatedParityNumber(product, model, 2048, maxNested, 0.5);
}

// ============================================================================
// LIVE conditional-surface parity (<double>; green since the #1 fix). The
// <Number_> fuzzy arms of these products are Phase 5 scope.
// ============================================================================

// Conditional surface via SupEqual knock-outs plus const variables. Exercises
// If (no else) with state mutation across events.
TEST(ScriptCompiledParityTest, TestParity_Barrier_Sup) {
    Global::Dates_::SetEvaluationDate(Date_(2022, 6, 22));
    ScriptProduct_ product = FixedBarrierProduct();
    product.PreProcess(false, false);

    // Below barrier / ITM path / above barrier (knocked out on every event).
    for (const double spot : {10.0, 13.0, 16.0}) {
        SCOPED_TRACE("spot=" + std::to_string(spot));
        AssertPerPathParity(product, spot);
    }

    auto model = StandardBSModel(10.0, 0.20, 0.034, 0.021);
    AssertAggregatedParityDouble(product, model, 4096);
}

// Digital payoff on strict equality (measure-zero on the hard path but the
// Equal opcode must still agree per-path, including exactly AT the strike).
TEST(ScriptCompiledParityTest, TestParity_Digital_Equal) {
    Global::Dates_::SetEvaluationDate(Date_(2022, 6, 22));
    Vector_<Cell_> eventDates{Cell_(String_("K")), Cell_(Date_(2024, 6, 21))};
    Vector_<String_> events{"11.0", R"(
        v = 0
        IF spot() = K THEN
            v = 1
        ELSE
            v = 0
        END
        out pays v
    )"};
    ScriptProduct_ product(eventDates, events, "out");
    product.PreProcess(false, false);
    for (const double spot : {11.0, 11.5, 9.0}) {
        SCOPED_TRACE("spot=" + std::to_string(spot));
        AssertPerPathParity(product, spot);
    }
}

// Nested IF/IfElse in both branches — stresses the recursive IfElse
// evaluation sharing the parent's thread_local stacks (#1 regression guard,
// deeper shape than the two dedicated #1 pins).
TEST(ScriptCompiledParityTest, TestParity_NestedIf) {
    Global::Dates_::SetEvaluationDate(Date_(2023, 1, 1));
    Vector_<Cell_> eventDates{Cell_(Date_(2023, 1, 28))};
    Vector_<String_> events{R"(
        y = 0
        IF spot() >= 2 THEN
            IF spot() >= 8 THEN
                y = 1
            ELSE
                IF spot() >= 4 THEN
                    y = 2
                ELSE
                    y = 3
                END
            END
        ELSE
            IF spot() >= 1 THEN
                y = 4
            ELSE
                y = 5
            END
        END
        out pays y
    )"};
    ScriptProduct_ product(eventDates, events, "out");
    product.PreProcess(false, false);
    for (const double spot : {0.5, 1.5, 3.0, 5.0, 9.0}) {
        SCOPED_TRACE("spot=" + std::to_string(spot));
        AssertPerPathParity(product, spot);
    }
}

// Multi-event accumulation with conditionals: running state (acc) carried
// across three future events, conditionally updated, then paid.
TEST(ScriptCompiledParityTest, TestParity_MultiEvent) {
    Global::Dates_::SetEvaluationDate(Date_(2022, 6, 22));
    Vector_<Cell_> eventDates;
    Vector_<String_> events;
    eventDates.push_back(Cell_(String_("K"))); events.push_back("10.0");
    eventDates.push_back(Cell_(Date_(2023, 6, 21)));
    events.push_back("acc = MAX(spot() - K, 0.0)");
    eventDates.push_back(Cell_(Date_(2023, 12, 21)));
    events.push_back("IF spot() > K THEN acc = acc + spot() - K END");
    eventDates.push_back(Cell_(Date_(2024, 6, 21)));
    events.push_back(R"(
        IF acc > 1 OR spot() > K THEN
            acc = acc + 1
        END
        out pays acc
    )");
    ScriptProduct_ product(eventDates, events, "out");
    product.PreProcess(false, false);

    for (const double spot : {8.0, 10.5, 14.0}) {
        SCOPED_TRACE("spot=" + std::to_string(spot));
        AssertPerPathParity(product, spot);
    }

    auto model = StandardBSModel(10.0, 0.20, 0.034, 0.021);
    AssertAggregatedParityDouble(product, model, 2048);
}

// ============================================================================
// <Number_> fuzzy-surface parity (Phase 5): the same conditional products as
// the <double> tests above, but preprocessed fuzzy and run through
// MCSimulation<Number_> — compiled-fuzzy vs FuzzyEvaluator_, PV + all risks.
// ============================================================================

TEST(ScriptCompiledParityTest, TestParity_Number_Barrier_Fuzzy) {
    Global::Dates_::SetEvaluationDate(Date_(2022, 6, 22));
    ScriptProduct_ product = FixedBarrierProduct();
    const int maxNested = static_cast<int>(product.PreProcess(true, false));

    auto model = StandardBSModel(10.0, 0.20, 0.034, 0.021);
    AssertAggregatedParityNumber(product, model, 4096, maxNested, 0.01);
}

TEST(ScriptCompiledParityTest, TestParity_Number_Digital_Fuzzy) {
    Global::Dates_::SetEvaluationDate(Date_(2022, 6, 22));
    Vector_<Cell_> eventDates{Cell_(String_("K")), Cell_(Date_(2024, 6, 21))};
    Vector_<String_> events{"11.0", R"(
        v = 0
        IF spot() = K THEN
            v = 1
        ELSE
            v = 0
        END
        out pays v
    )"};
    ScriptProduct_ product(eventDates, events, "out");
    const int maxNested = static_cast<int>(product.PreProcess(true, false));

    auto model = StandardBSModel(10.0, 0.20, 0.034, 0.021);
    // Butterfly-smoothed equality (BFly): exercise both a tight and a fat eps.
    AssertAggregatedParityNumber(product, model, 2048, maxNested, 0.05);
    AssertAggregatedParityNumber(product, model, 2048, maxNested, 1.0);
}

TEST(ScriptCompiledParityTest, TestParity_Number_NestedIf_Fuzzy) {
    Global::Dates_::SetEvaluationDate(Date_(2022, 6, 22));
    Vector_<Cell_> eventDates{Cell_(Date_(2023, 6, 21))};
    Vector_<String_> events{R"(
        y = 0
        IF spot() >= 8 THEN
            IF spot() >= 12 THEN
                y = 1
            ELSE
                y = 2
            END
        ELSE
            IF spot() >= 4 THEN
                y = 3
            ELSE
                y = 4
            END
        END
        out pays y
    )"};
    ScriptProduct_ product(eventDates, events, "out");
    const int maxNested = static_cast<int>(product.PreProcess(true, false));
    ASSERT_EQ(maxNested, 2);

    auto model = StandardBSModel(10.0, 0.20, 0.034, 0.021);
    // Fat eps keeps inner conditions fuzzy so the nested dt-blend
    // (varStore levels) is genuinely exercised.
    AssertAggregatedParityNumber(product, model, 2048, maxNested, 2.0);
}

TEST(ScriptCompiledParityTest, TestParity_Number_MultiEvent_Fuzzy) {
    Global::Dates_::SetEvaluationDate(Date_(2022, 6, 22));
    Vector_<Cell_> eventDates;
    Vector_<String_> events;
    eventDates.push_back(Cell_(String_("K"))); events.push_back("10.0");
    eventDates.push_back(Cell_(Date_(2023, 6, 21)));
    events.push_back("acc = MAX(spot() - K, 0.0)");
    eventDates.push_back(Cell_(Date_(2023, 12, 21)));
    events.push_back("IF spot() > K THEN acc = acc + spot() - K END");
    eventDates.push_back(Cell_(Date_(2024, 6, 21)));
    events.push_back(R"(
        IF acc > 1 OR spot() > K THEN
            acc = acc + 1
        END
        out pays acc
    )");
    ScriptProduct_ product(eventDates, events, "out");
    const int maxNested = static_cast<int>(product.PreProcess(true, false));

    auto model = StandardBSModel(10.0, 0.20, 0.034, 0.021);
    AssertAggregatedParityNumber(product, model, 2048, maxNested, 0.5);
}

// ============================================================================
// Golden pin: fixed barrier PV + risks through the tree-walk <Number_> arm.
// Values generated by running THIS machine's tree-walk (AADET framework,
// sobol, 4096 paths, default smoothing) — the goal is to freeze tree-walk
// numbers so the later default-flip phases can prove the numbers never move.
// Tolerance 1e-6 (goldens are machine-generated literals, not analytics).
// ============================================================================
TEST(ScriptCompiledParityTest, TestGolden_FixedBarrier_PV_Risks) {
    Global::Dates_::SetEvaluationDate(Date_(2022, 6, 22));
    ScriptProduct_ product = FixedBarrierProduct();
    const int maxNested = static_cast<int>(product.PreProcess(true, false));

    auto model = StandardBSModel(10.0, 0.20, 0.034, 0.021);
    const SimResults_ results = MCSimulation<Number_>(product, model, 4096, "sobol", false, false, maxNested);

    ASSERT_NEAR(results.aggregated_, 2616.68830169999, 1e-6);

    // risks_[0..3]: BS params (spot, vol, rate, div); risks_[4..5]: const
    // variables in alphabetical order (BARRIER, STRIKE).
    ASSERT_EQ(results.risks_.size(), 6u);
    ASSERT_NEAR(results.risks_[0], 0.392061230359593, 1e-6);
    ASSERT_NEAR(results.risks_[1], 4.25364478488146, 1e-6);
    ASSERT_NEAR(results.risks_[2], 6.56354477237741, 1e-6);
    ASSERT_NEAR(results.risks_[3], -7.84122460719187, 1e-6);
    ASSERT_NEAR(results.risks_[4], 0.0, 1e-6);
    ASSERT_NEAR(results.risks_[5], -0.298342944198973, 1e-6);
}

// ============================================================================
// Opcode coverage: the union of compiled node streams across a battery of
// products must cover every reachable opcode, so no compiled case is left
// untested by the parity suite. Reachability today (verified empirically):
//   - Const (19) is dead-by-construction: Compile() always runs ConstProcess
//     first, and every visitor that could reach a fully-const sub-expression
//     bakes it into a *Const variant (AddConst, AssignConst, ...) without
//     visiting it, so the bare Const opcode is never emitted. (Fuzzy mode
//     COULD emit it for a const condition argument, but the fuzzy pipeline
//     collapses domain-provable const conditions before Compile().)
//   - Slot 31 (the retired Smooth opcode, defect #7) is a hole in the enum:
//     Phase 5 replaced it with the dedicated fuzzy opcodes 40+.
//   - ConstVar (39) is REQUIRED since the Phase 1 #11 fix: NodeConstVar_ is
//     no longer born isConst_=true, so parents stop folding it away and the
//     const-var battery below emits a live ConstVar opcode.
//   - FuzzyEqual..FuzzyIf (40-49) are REQUIRED since Phase 5: the fuzzy
//     battery compiles a fuzzy-preprocessed product with Compile(true).
// ============================================================================
namespace {
    // Walk a compiled node stream, skipping operands, collecting opcodes.
    void CollectOpcodes(const Vector_<int>& stream, std::set<int>* out) {
        size_t i = 0;
        while (i < stream.size()) {
            const int op = stream[i];
            out->insert(op);
            switch (op) {
            case AddConst: case SubConst: case ConstSub: case MultiConst:
            case DivConst: case ConstDiv: case PowConst: case ConstPow:
            case Max2Const: case Min2Const: case Var: case Const:
            case ConstVar: case Assign: case Pays: case If:
            case FuzzyEqual: case FuzzyComp:
                i += 2;
                break;
            case AssignConst: case PaysConst: case IfElse:
            case FuzzyEqualDiscrete: case FuzzyCompDiscrete:
                i += 3;
                break;
            case FuzzyIf:
                //  FuzzyIf lastTrue lastFalse nAff aff... — the true/false
                //  statement code follows inline and is walked normally.
                i += 4 + stream[i + 3];
                break;
            default:
                i += 1;
                break;
            }
        }
    }

    // Compile a one-event product body and merge its opcodes into `out`.
    // PreProcess(false, true) keeps conditions alive down to Compile() so
    // condition opcodes (Equal/Sup/SupEqual/And/Or/Not/True/False) survive.
    void MergeProductOpcodes(const String_& body, std::set<int>* out) {
        Vector_<Cell_> eventDates{Cell_(Date_(2023, 1, 28))};
        Vector_<String_> events{body};
        ScriptProduct_ product(eventDates, events);
        product.PreProcess(false, true);
        const ScriptCompiled_ compiled = product.Compile();
        for (const auto& stream : compiled.NodeStreams())
            CollectOpcodes(stream, out);
    }

    // Compile a product with a live const variable and merge its opcodes.
    // Drives the ConstVar opcode (#11): reachable since Phase 1.
    void MergeConstVarProductOpcodes(std::set<int>* out) {
        Vector_<Cell_> eventDates;
        Vector_<String_> events;
        eventDates.push_back(Cell_(String_("K"))); events.push_back("11.0");
        eventDates.push_back(Cell_(Date_(2023, 1, 28)));
        events.push_back("out pays MAX(spot() - K, 0.0)");
        ScriptProduct_ product(eventDates, events, "out");
        product.PreProcess(false, true);
        const ScriptCompiled_ compiled = product.Compile();
        for (const auto& stream : compiled.NodeStreams())
            CollectOpcodes(stream, out);
    }

    // Fuzzy battery: compile a fuzzy-preprocessed product with Compile(true)
    // so every fuzzy opcode is emitted — continuous and discrete comparisons,
    // combinators, Not, collapsed True/False conditions, and FuzzyIf with and
    // without else.
    void MergeFuzzyProductOpcodes(std::set<int>* out) {
        Vector_<Cell_> eventDates{Cell_(Date_(2023, 1, 28))};
        Vector_<String_> events{R"(
            d = 0
            IF spot() > 5 THEN
                d = 1
            END
            IF spot() = 10 THEN
                y = 1
            ELSE
                y = 2
            END
            IF d = 1 THEN
                y = y + 1
            END
            IF d > 0 THEN
                y = y + 2
            END
            IF spot() != 3 THEN
                y = y + 3
            END
            IF 1 > 0 AND spot() > 2 THEN
                y = y + 4
            END
            IF 0 > 1 OR spot() > 4 THEN
                y = y + 5
            END
            out pays y
        )"};
        ScriptProduct_ product(eventDates, events, "out");
        product.PreProcess(true, false);
        const ScriptCompiled_ compiled = product.Compile(true);
        for (const auto& stream : compiled.NodeStreams())
            CollectOpcodes(stream, out);
    }
} // namespace

TEST(ScriptCompiledParityTest, TestOpcodeCoverage_AllReachableOpcodesExercised) {
    Global::Dates_::SetEvaluationDate(Date_(2023, 1, 1));
    std::set<int> seen;

    // Arithmetic battery: every binary/unary arithmetic opcode incl. the
    // *Const / Const* left-right variants, Assign/AssignConst, Pays/PaysConst.
    MergeProductOpcodes(R"(
        x = spot()
        a = x + x
        a = x + 1
        a = x - x
        a = x - 1
        a = 1 - x
        a = x * x
        a = x * 2
        a = x / x
        a = x / 2
        a = 2 / x
        a = x ^ x
        a = x ^ 2
        a = 2 ^ x
        a = MAX(x, x + 1)
        a = MAX(x, 2)
        a = MIN(x, x + 1)
        a = MIN(x, 2)
        a = SQRT(x)
        a = LOG(x)
        a = EXP(x)
        a = -x
        a = 3
        out pays a
        out pays 3
    )", &seen);

    // Conditional battery: If/IfElse, all comparison opcodes, boolean
    // combinators, Not (via !=), and const-folded True/False conditions.
    MergeProductOpcodes(R"(
        x = spot()
        IF x > 1 THEN
            y = 1
        END
        IF x >= 1 THEN
            y = 2
        ELSE
            y = 3
        END
        IF x = 2 THEN
            y = 4
        END
        IF x != 2 THEN
            y = 5
        END
        IF x > 1 AND x < 3 THEN
            y = 6
        END
        IF x > 1 OR x < 0 THEN
            y = 7
        END
        IF 1 > 0 THEN
            y = 8
        END
        IF 0 > 1 THEN
            y = 9
        END
        out pays y
    )", &seen);

    // Const-var battery: a live const variable must emit the ConstVar opcode.
    MergeConstVarProductOpcodes(&seen);

    // Fuzzy battery: every fuzzy opcode must be emitted by Compile(true).
    MergeFuzzyProductOpcodes(&seen);

    const std::set<int> unreachable = {Const, 31 /* retired Smooth slot (#7) */};
    for (int op = Add; op <= FuzzyIf; ++op) {
        if (unreachable.count(op)) {
            ASSERT_EQ(seen.count(op), 0u)
                << "opcode " << op << " was believed unreachable but was emitted; "
                   "update the coverage contract in this test";
        } else {
            ASSERT_EQ(seen.count(op), 1u)
                << "reachable opcode " << op << " is not covered by any compiled stream in this suite";
        }
    }
}

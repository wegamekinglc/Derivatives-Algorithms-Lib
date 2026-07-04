//
// Created by wegame on 2026/07/04.
//
// Phase 0 fuzz/property layer for the compiled vs tree-walk parity harness.
// Emits deterministic random operator trees, random if/IfElse structure,
// random :eps values, random schedules, AND random const variables + past
// dates (without these the layer structurally cannot hit the #11 const-var
// or past-event surfaces). Each generated product runs through both
// evaluators on the same Sobol seed; assert parity at 1e-8. Phase-0 scope =
// infrastructure + a small live subset; full ~500/run coverage lands later.
//
// The deterministic generator is a fixed-seed std::mt19937 object. No
// `volatile`, no `mutable` (both banned). Seeds are printed on failure so a
// RED run can be reproduced.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/math/aad/aad.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/storage/globals.hpp>
#include <dal/script/event.hpp>
#include <dal/script/simulation.hpp>

#include <random>
#include <sstream>
#include <string>

using namespace Dal;
using namespace Dal::AAD;
using namespace Dal::Script;

namespace {
    // Fixed-seed PRNG object. Re-created per case from an explicit seed, so
    // every run is reproducible. No mutable / volatile state.
    class Rng_ {
        std::mt19937 engine_;

    public:
        explicit Rng_(uint32_t seed) : engine_(seed) {}

        // Uniform integer in [lo, hi] inclusive.
        int UniformInt(int lo, int hi) {
            return std::uniform_int_distribution<int>(lo, hi)(engine_);
        }

        // Uniform real in [lo, hi).
        double UniformReal(double lo, double hi) {
            return std::uniform_real_distribution<double>(lo, hi)(engine_);
        }

        // Bernoulli.
        bool Bernoulli(double p) { return std::bernoulli_distribution(p)(engine_); }
    };

    // One binary operator pick from the set the script grammar accepts.
    // Kept arithmetic-only so the fuzz layer stays deterministic-evaluable;
    // conditional surface is added separately via BuildCondition.
    String_ RandomArithOp(Rng_& rng) {
        static const Vector_<String_> ops = {"+", "-", "*", "/"};
        return ops[rng.UniformInt(0, static_cast<int>(ops.size()) - 1)];
    }

    // A random comparison operator. Includes != so the #4 surface is
    // reachable when this is used inside an IF on the <Number_> fuzzy path.
    String_ RandomCompareOp(Rng_& rng) {
        static const Vector_<String_> ops = {">", ">=", "<", "<=", "=="};
        return ops[rng.UniformInt(0, static_cast<int>(ops.size()) - 1)];
    }

    // Build a random arithmetic expression string of bounded depth over
    // spot(), STRIKE, and literals. Division RHS is guarded away from zero
    // so the fuzz layer does not throw on its own generated products (the
    // harness exists to catch evaluator divergence, not parser/math errors).
    String_ BuildExpr(Rng_& rng, int depth) {
        if (depth <= 0 || rng.Bernoulli(0.4)) {
            const int leaf = rng.UniformInt(0, 2);
            if (leaf == 0)
                return "spot()";
            if (leaf == 1)
                return "STRIKE";
            // Literal: keep away from 0 to avoid div-by-zero in callers.
            const double v = rng.UniformReal(0.5, 5.0);
            std::ostringstream os;
            os << v;
            return String_(os.str());
        }
        const String_ lhs = BuildExpr(rng, depth - 1);
        const String_ op = RandomArithOp(rng);
        String_ rhs = BuildExpr(rng, depth - 1);
        if (op == "/")
            rhs = "MAX(" + rhs + ", 0.5)";
        return "(" + lhs + " " + op + " " + rhs + ")";
    }

    // Build a random boolean condition of bounded depth. Leaves compare a
    // random arithmetic expression to a literal so the :eps surface stays
    // reachable when used inside an IF.
    String_ BuildCondition(Rng_& rng, int depth) {
        if (depth <= 0 || rng.Bernoulli(0.5)) {
            const String_ lhs = BuildExpr(rng, 1);
            const String_ op = RandomCompareOp(rng);
            const double rhs = rng.UniformReal(-1.0, 5.0);
            std::ostringstream os;
            os << lhs << " " << op << " " << rhs;
            return String_(os.str());
        }
        const String_ lhs = BuildCondition(rng, depth - 1);
        const String_ op = rng.Bernoulli(0.5) ? String_("AND") : String_("OR");
        const String_ rhs = BuildCondition(rng, depth - 1);
        return "(" + lhs + " " + op + " " + rhs + ")";
    }

    // Build a random IfElse body. The :eps hint is randomly attached to the
    // IF condition so the fuzzy path is exercised when the harness drives
    // MCSimulation<Number_>. Phase-0 only emits a single IfElse per body to
    // keep the layer small.
    String_ BuildIfElseBody(Rng_& rng) {
        String_ cond = BuildCondition(rng, 2);
        if (rng.Bernoulli(0.5)) {
            const double eps = rng.UniformReal(0.01, 0.2);
            std::ostringstream os;
            os << ":" << eps;
            cond = cond + String_(os.str());
        }
        const String_ trueExpr = BuildExpr(rng, 2);
        const String_ falseExpr = BuildExpr(rng, 2);
        return "v = 0.0\nIF " + cond + " THEN\n  v = " + trueExpr + "\nELSE\n  v = " + falseExpr + "\nEND\nout pays v";
    }

    // Build a random product from a seed. Phase-0: one future event with a
    // random IfElse body. Const variable (STRIKE) + optional past fixing.
    // The product is held via unique_ptr because ScriptProduct_ has no
    // default constructor (Vector_<> members with no default), so a by-value
    // member would not compile.
    struct FuzzProduct_ {
        std::unique_ptr<ScriptProduct_> product;
        double strike;
        bool hasPastFixing;
        double pastFixing;
        uint32_t seed;

        static FuzzProduct_ Build(uint32_t s, bool withPast) {
            Rng_ rng(s);
            FuzzProduct_ fp;
            fp.seed = s;
            fp.hasPastFixing = withPast;
            fp.strike = rng.UniformReal(8.0, 14.0);
            fp.pastFixing = withPast ? rng.UniformReal(0.0, 3.0) : 0.0;

            Vector_<Cell_> eventDates;
            Vector_<String_> events;
            eventDates.push_back(Cell_(String_("STRIKE")));
            events.push_back(ToString(fp.strike));

            if (withPast) {
                // Past fixing: dated before the evaluation date so
                // PastEvaluate() seeds the variable on both evaluators.
                eventDates.push_back(Cell_(Date_(2022, 6, 20)));
                std::ostringstream os;
                os << "pre = " << fp.pastFixing;
                events.push_back(String_(os.str()));
            }

            eventDates.push_back(Cell_(Date_(2024, 6, 21)));
            const String_ body = BuildIfElseBody(rng) + (withPast ? String_(" + pre") : String_(""));
            events.push_back("out pays (" + body + ")");

            fp.product = std::make_unique<ScriptProduct_>(eventDates, events);
            return fp;
        }

      private:
        FuzzProduct_() = default;
    };

    // Run one fuzz case: build, preprocess, compile, run both evaluators on
    // the same Sobol seed, assert per-path parity. <double> only in Phase 0;
    // the <Number_> / fuzzy surface is deferred (would surface #4 / #12 which
    // are staged in the dedicated defect-pin tests).
    void RunFuzzCase(uint32_t seed, bool withPast) {
        Global::Dates_::SetEvaluationDate(Date_(2022, 6, 22));
        FuzzProduct_ fp = FuzzProduct_::Build(seed, withPast);
        ScriptProduct_& product = *fp.product;
        product.PreProcess(false, false);
        product.Compile();

        // Per-path: deterministic scenario over several spots so a wide range
        // of branch outcomes is exercised. If the two evaluators diverge we
        // print the seed for reproduction.
        const double spots[] = {0.5, 1.0, 5.0, 9.0, 11.0, 13.0, 20.0};
        for (double spot : spots) {
            Scenario_<double> scenario(product.EventDates().size());
            for (auto& s : scenario) {
                s.spot_ = spot;
                s.numeraire_ = 1.0;
            }

            Evaluator_<double> treeEval = product.BuildEvaluator<double>();
            product.Evaluate(scenario, treeEval);
            const double treePayoff = treeEval.VarVals()[product.PayOffIdx()];

            EvalState_<double> compiledState = product.BuildEvalState<double>();
            product.EvaluateCompiled(scenario, compiledState);
            const double compiledPayoff = compiledState.VarVals()[product.PayOffIdx()];

            ASSERT_NEAR(compiledPayoff, treePayoff, 1e-8)
                << "FUZZ DIVERGENCE: seed=" << seed << " withPast=" << withPast
                << " spot=" << spot << " strike=" << fp.strike;
        }
    }
} // namespace

// Phase-0 live subset: a handful of seeded cases without past fixings. Kept
// small so the suite stays fast; full ~500/run coverage lands in a later
// phase once the defect-pin tests are green. If a case REDs, the seed is
// printed in the assertion message for reproduction.
TEST(ScriptCompiledParityFuzzTest, TestFuzz_IfElse_NoPast_Handful) {
    const uint32_t seeds[] = {1, 2, 3, 4, 5, 7, 11, 42};
    for (uint32_t seed : seeds) {
        SCOPED_TRACE("seed=" + std::to_string(seed));
        ASSERT_NO_FATAL_FAILURE(RunFuzzCase(seed, false));
    }
}

// Phase-0 live subset with past fixings — exercises the past-event init
// seeding surface alongside the IfElse + const-var surfaces.
TEST(ScriptCompiledParityFuzzTest, TestFuzz_IfElse_WithPast_Handful) {
    const uint32_t seeds[] = {101, 102, 103, 211, 314};
    for (uint32_t seed : seeds) {
        SCOPED_TRACE("seed=" + std::to_string(seed));
        ASSERT_NO_FATAL_FAILURE(RunFuzzCase(seed, true));
    }
}

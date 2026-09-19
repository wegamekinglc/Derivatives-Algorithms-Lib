//
// Created by wegame on 2026/07/04.
//
// Phase 0 fuzz/property layer for the compiled vs tree-walk parity harness.
// Emits deterministic random operator trees, random if/IfElse structure
// (including one level of nesting), random :eps hints, random schedules,
// random const variables (macro events) and random past dates. Each
// generated product runs through both evaluators on the same deterministic
// scenarios; assert per-path parity at 1e-8. ~500 products per run.
//
// The deterministic generator is a fixed-seed std::mt19937 object. No
// `volatile`, no `mutable` (both banned). Seeds are printed on failure so a
// RED run can be reproduced. Only existing grammar constructs are emitted
// (note: equality is the single '=' comparator; '==' is not in the lexer).
//

#include <gtest/gtest.h>

#include <random>
#include <sstream>
#include <string>

#include <dal/platform/platform.hpp>
#include <dal/math/aad/aad.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/storage/globals.hpp>
#include <dal/script/event.hpp>
#include <dal/script/simulation.hpp>
#include <dal/time/date.hpp>

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

    String_ FormatLiteral(double v) {
        std::ostringstream os;
        os << v; // never scientific for the ranges drawn here
        return String_(os.str());
    }

    String_ RandomArithOp(Rng_& rng) {
        static const Vector_<String_> ops = {"+", "-", "*", "/"};
        return ops[rng.UniformInt(0, static_cast<int>(ops.size()) - 1)];
    }

    // The grammar's comparators: equality is a single '='; '!=' surfaces
    // NodeNot_ over NodeEqual_.
    String_ RandomCompareOp(Rng_& rng) {
        static const Vector_<String_> ops = {">", ">=", "<", "<=", "=", "!="};
        return ops[rng.UniformInt(0, static_cast<int>(ops.size()) - 1)];
    }

    String_ BuildExpr(Rng_& rng, int depth, const Vector_<String_>& leaves);

    String_ BuildExprLeaf(Rng_& rng, const Vector_<String_>& leaves) {
        const int pick = rng.UniformInt(0, static_cast<int>(leaves.size()));
        if (pick < static_cast<int>(leaves.size()))
            return leaves[pick];
        return FormatLiteral(rng.UniformReal(0.5, 5.0));
    }

    String_ BuildFuncExpr(Rng_& rng, int depth, const Vector_<String_>& leaves, int shape) {
        if (shape == 0)
            return "MAX(" + BuildExpr(rng, depth - 1, leaves) + ", " + BuildExpr(rng, depth - 1, leaves) + ")";
        if (shape == 1)
            return "MIN(" + BuildExpr(rng, depth - 1, leaves) + ", " + BuildExpr(rng, depth - 1, leaves) + ")";
        if (shape == 2)
            return "SQRT(MAX(" + BuildExpr(rng, depth - 1, leaves) + ", 0.25))";
        return "-(" + BuildExpr(rng, depth - 1, leaves) + ")";
    }

    String_ BuildBinaryExpr(Rng_& rng, int depth, const Vector_<String_>& leaves) {
        const String_ lhs = BuildExpr(rng, depth - 1, leaves);
        const String_ op = RandomArithOp(rng);
        String_ rhs = BuildExpr(rng, depth - 1, leaves);
        if (op == "/")
            rhs = "MAX(" + rhs + ", 0.5)";
        return "(" + lhs + " " + op + " " + rhs + ")";
    }

    // Build a random arithmetic expression of bounded depth over spot(),
    // const variables, prior variables and literals. Division RHS is guarded
    // away from zero so the layer does not manufacture NaN/inf on its own
    // (the harness exists to catch evaluator divergence, not math errors;
    // ASSERT_NEAR(NaN, NaN) fails even when both arms agree).
    String_ BuildExpr(Rng_& rng, int depth, const Vector_<String_>& leaves) {
        if (depth <= 0 || rng.Bernoulli(0.35))
            return BuildExprLeaf(rng, leaves);
        const int shape = rng.UniformInt(0, 5);
        return shape < 4 ? BuildFuncExpr(rng, depth, leaves, shape)
                         : BuildBinaryExpr(rng, depth, leaves);
    }

    // Build a random boolean condition of bounded depth, optionally with a
    // random :eps hint on a leaf comparison. Grammar quirk: in condition
    // position a leading '(' is parsed as CONDITION grouping, so a comparison
    // lhs must not start with '('; prefix such expressions with "0 + ".
    String_ BuildCondition(Rng_& rng, int depth, const Vector_<String_>& leaves) {
        if (depth <= 0 || rng.Bernoulli(0.5)) {
            String_ lhs = BuildExpr(rng, 1, leaves);
            if (!lhs.empty() && lhs.front() == '(')
                lhs = "0 + " + lhs;
            const String_ op = RandomCompareOp(rng);
            String_ leaf = lhs + " " + op + " " + FormatLiteral(rng.UniformReal(-1.0, 5.0));
            if (rng.Bernoulli(0.3))
                leaf = leaf + ":" + FormatLiteral(rng.UniformReal(0.01, 0.2));
            return leaf;
        }
        const String_ lhs = BuildCondition(rng, depth - 1, leaves);
        const String_ op = rng.Bernoulli(0.5) ? String_("AND") : String_("OR");
        const String_ rhs = BuildCondition(rng, depth - 1, leaves);
        return "(" + lhs + " " + op + " " + rhs + ")";
    }

    // Build one random statement block assigning `target`: plain assignment,
    // IF, or IF/ELSE, optionally with one nested IF in a branch. Conditions
    // may read the running variable (condLeaves); assignment RHS must not
    // (exprLeaves): a self-referential `v = v + v` replicated over many
    // schedule events makes DomainProcessor's discrete-set arithmetic grow
    // super-exponentially (pre-existing behavior, not an evaluator defect).
    // Accumulator-style self-reference is covered by the named
    // TestParity_MultiEvent instead.
    String_ BuildBlock(Rng_& rng, const String_& target, const Vector_<String_>& condLeaves,
                       const Vector_<String_>& exprLeaves) {
        const int shape = rng.UniformInt(0, 3);
        if (shape == 0)
            return target + " = " + BuildExpr(rng, 2, exprLeaves) + "\n";
        std::ostringstream os;
        os << "IF " << BuildCondition(rng, 2, condLeaves) << " THEN\n";
        if (shape == 3 && rng.Bernoulli(0.5)) {
            os << "IF " << BuildCondition(rng, 1, condLeaves) << " THEN\n"
               << target << " = " << BuildExpr(rng, 1, exprLeaves) << "\n"
               << "ELSE\n"
               << target << " = " << BuildExpr(rng, 1, exprLeaves) << "\n"
               << "END\n";
        } else {
            os << target << " = " << BuildExpr(rng, 2, exprLeaves) << "\n";
        }
        if (shape >= 2) {
            os << "ELSE\n";
            os << target << " = " << BuildExpr(rng, 1, exprLeaves) << "\n";
        }
        os << "END\n";
        return String_(os.str());
    }

    void AddConstVariables(Rng_& rng, Vector_<Cell_>& eventDates,
                           Vector_<String_>& events, Vector_<String_>& leaves) {
        leaves.push_back("spot()");
        eventDates.push_back(Cell_(String_("STRIKE")));
        events.push_back(FormatLiteral(rng.UniformReal(8.0, 14.0)));
        leaves.push_back("STRIKE");
        if (rng.Bernoulli(0.5)) {
            eventDates.push_back(Cell_(String_("BARRIER")));
            events.push_back(FormatLiteral(rng.UniformReal(12.0, 20.0)));
            leaves.push_back("BARRIER");
        }
    }

    void AddPastFixings(Rng_& rng, Vector_<Cell_>& eventDates,
                        Vector_<String_>& events, Vector_<String_>& leaves) {
        if (!rng.Bernoulli(0.4))
            return;
        eventDates.push_back(Cell_(Date_(2022, 6, 20)));
        events.push_back("pre = " + FormatLiteral(rng.UniformReal(0.5, 3.0)));
        leaves.push_back("pre");
        if (rng.Bernoulli(0.3)) {
            eventDates.push_back(Cell_(Date_(2022, 6, 21)));
            events.push_back("pre2 = " + BuildExpr(rng, 1, {String_("pre")}));
            leaves.push_back("pre2");
        }
    }

    void AddScheduleStructure(Rng_& rng, const Vector_<String_>& leaves,
                              Vector_<Cell_>& eventDates, Vector_<String_>& events) {
        eventDates.push_back(Cell_(Date_(2023, 1, 4)));
        events.push_back("v = " + BuildExpr(rng, 1, leaves) + "\n");
        Vector_<String_> vLeaves = leaves;
        vLeaves.push_back("v");
        eventDates.push_back(Cell_(String_("START: " + Date::ToString(Date_(2023, 1, 4)) +
                                           " END: " + Date::ToString(Date_(2023, rng.UniformInt(2, 4), 15)) +
                                           " FREQ: 1W")));
        events.push_back(BuildBlock(rng, "v", vLeaves, leaves));
        eventDates.push_back(Cell_(Date_(2024, 6, 21)));
        events.push_back("out pays " + BuildExpr(rng, 1, vLeaves));
    }

    void AddDatedStructure(Rng_& rng, int nEvents, const Vector_<String_>& leaves,
                           Vector_<Cell_>& eventDates, Vector_<String_>& events) {
        Vector_<String_> vLeaves = leaves;
        for (int e = 0; e < nEvents; ++e) {
            eventDates.push_back(Cell_(Date_(2023, 1, 4).AddDays(91 * e)));
            std::ostringstream body;
            if (e == 0)
                body << "v = " << BuildExpr(rng, 1, leaves) << "\n";
            else
                body << BuildBlock(rng, "v", vLeaves, leaves);
            if (e == 0)
                vLeaves.push_back("v");
            if (e == nEvents - 1) {
                body << BuildBlock(rng, "v", vLeaves, leaves);
                body << "out pays " << BuildExpr(rng, 1, vLeaves) << "\n";
            }
            events.push_back(String_(body.str()));
        }
    }

    // A generated product plus its reproduction info. Held via unique_ptr
    // because ScriptProduct_ has no default constructor.
    struct FuzzProduct_ {
        std::unique_ptr<ScriptProduct_> product;
        uint32_t seed = 0;

        // structure: 0 = single event, 1 = multi event, 2 = schedule
        static FuzzProduct_ Build(uint32_t s, int structure) {
            Rng_ rng(s);
            FuzzProduct_ fp;
            fp.seed = s;

            Vector_<Cell_> eventDates;
            Vector_<String_> events;

            Vector_<String_> leaves;
            AddConstVariables(rng, eventDates, events, leaves);
            AddPastFixings(rng, eventDates, events, leaves);

            if (structure == 2)
                AddScheduleStructure(rng, leaves, eventDates, events);
            else
                AddDatedStructure(rng, structure == 1 ? rng.UniformInt(2, 4) : 1,
                                  leaves, eventDates, events);

            fp.product = std::make_unique<ScriptProduct_>(eventDates, events, "out");
            return fp;
        }

    private:
        FuzzProduct_() = default;
    };

    // Run one fuzz case: build, preprocess, compile, run both evaluators on
    // the same deterministic scenarios, assert per-path parity. The hard arm
    // compares Evaluator_ vs Compile(); the fuzzy arm rebuilds the product
    // with fuzzy preprocessing and compares FuzzyEvaluator_ vs Compile(true)
    // across two smoothing widths.
    void RunFuzzCase(uint32_t seed, int structure) {
        Global::Dates_::SetEvaluationDate(Date_(2022, 6, 22));
        FuzzProduct_ fp = FuzzProduct_::Build(seed, structure);
        ScriptProduct_& product = *fp.product;
        product.PreProcess(false, false);
        const ScriptCompiled_ compiled = product.Compile();

        const double spots[] = {0.5, 1.0, 5.0, 9.0, 11.0, 13.0, 20.0};
        for (const double spot : spots) {
            Scenario_<double> scenario(product.EventDates().size());
            for (auto& s : scenario) {
                s.spot_ = spot;
                s.numeraire_ = 1.0;
            }

            Evaluator_<double> treeEval = product.BuildEvaluator<double>();
            product.Evaluate(scenario, treeEval);
            const double treePayoff = treeEval.VarVals()[product.PayOffIdx()];

            EvalState_<double> compiledState = product.BuildEvalState<double>();
            compiled.Evaluate(scenario, compiledState);
            const double compiledPayoff = compiledState.VarVals()[product.PayOffIdx()];

            ASSERT_NEAR(compiledPayoff, treePayoff, 1e-8)
                << "FUZZ DIVERGENCE: seed=" << seed << " structure=" << structure
                << " spot=" << spot;
        }

        //  Fuzzy arm: fresh build (PreProcess mutates the AST), fuzzy
        //  preprocessing, FuzzyEvaluator_ vs the fuzzy-compiled stream.
        FuzzProduct_ fzp = FuzzProduct_::Build(seed, structure);
        ScriptProduct_& fuzzyProduct = *fzp.product;
        const int maxNested = static_cast<int>(fuzzyProduct.PreProcess(true, false));
        const ScriptCompiled_ fuzzyCompiled = fuzzyProduct.Compile(true);

        const double epsValues[] = {0.05, 0.5};
        for (const double eps : epsValues) {
            for (const double spot : spots) {
                Scenario_<double> scenario(fuzzyProduct.EventDates().size());
                for (auto& s : scenario) {
                    s.spot_ = spot;
                    s.numeraire_ = 1.0;
                }

                FuzzyEvaluator_<double> fuzzyEval = fuzzyProduct.BuildFuzzyEvaluator<double>(maxNested, eps);
                fuzzyProduct.Evaluate(scenario, fuzzyEval);
                const double treePayoff = fuzzyEval.VarVals()[fuzzyProduct.PayOffIdx()];

                EvalState_<double> compiledState =
                    fuzzyProduct.BuildEvalState<double>(static_cast<size_t>(maxNested), eps);
                fuzzyCompiled.Evaluate(scenario, compiledState);
                const double compiledPayoff = compiledState.VarVals()[fuzzyProduct.PayOffIdx()];

                ASSERT_NEAR(compiledPayoff, treePayoff, 1e-8)
                    << "FUZZY FUZZ DIVERGENCE: seed=" << seed << " structure=" << structure
                    << " spot=" << spot << " eps=" << eps;
            }
        }
    }
    //  Random EXERCISE statement for the LSMC parity fuzz: a safe exercise value
    //  expression (no LOG/POW, guarded division) with an optional condition. The
    //  statement is appended last in its event so the greedy IF binding never
    //  captures a following statement's condition keyword.
    String_ BuildExerciseStatement(Rng_& rng, const Vector_<String_>& leaves) {
        String_ statement = "EXERCISE " + BuildExpr(rng, 2, leaves);
        if (rng.Bernoulli(0.5))
            statement += " IF " + BuildCondition(rng, 2, leaves);
        return statement + "\n";
    }

    //  Random EXERCISE product on the prepared pipeline (S12): coupons and exercise
    //  dates over the existing expression generator. At least one EXERCISE statement
    //  per product so the valuation diverts to the LSMC driver.
    struct ExerciseFuzzProduct_ {
        std::unique_ptr<ScriptProductData_> data;
        uint32_t seed = 0;

        // structure: 0 = single event, 1 = multi event, 2 = schedule
        static ExerciseFuzzProduct_ Build(uint32_t s, int structure) {
            Rng_ rng(s);
            ExerciseFuzzProduct_ fp;
            fp.seed = s;

            Vector_<Cell_> eventDates;
            Vector_<String_> events;
            Vector_<String_> leaves;
            AddConstVariables(rng, eventDates, events, leaves);
            AddPastFixings(rng, eventDates, events, leaves);

            size_t exerciseCount = 0;
            if (structure == 2) {
                eventDates.push_back(Cell_(Date_(2023, 1, 4)));
                events.push_back("v = " + BuildExpr(rng, 1, leaves) + "\n");
                Vector_<String_> vLeaves = leaves;
                vLeaves.push_back("v");
                eventDates.push_back(Cell_(String_("START: " + Date::ToString(Date_(2023, 1, 4)) +
                                                   " END: " + Date::ToString(Date_(2023, rng.UniformInt(2, 4), 15)) +
                                                   " FREQ: 1W")));
                events.push_back(BuildBlock(rng, "v", vLeaves, leaves) + BuildExerciseStatement(rng, vLeaves));
                eventDates.push_back(Cell_(Date_(2024, 6, 21)));
                events.push_back("out pays " + BuildExpr(rng, 1, vLeaves) + "\n" + BuildExerciseStatement(rng, vLeaves));
                exerciseCount = 2;
            } else {
                const int nEvents = structure == 1 ? rng.UniformInt(2, 4) : 1;
                Vector_<String_> vLeaves = leaves;
                for (int e = 0; e < nEvents; ++e) {
                    eventDates.push_back(Cell_(Date_(2023, 1, 4).AddDays(91 * e)));
                    std::ostringstream body;
                    if (e == 0) {
                        body << "v = " << BuildExpr(rng, 1, leaves) << "\n";
                        if (rng.Bernoulli(0.5))
                            body << "coupon PAYS " << FormatLiteral(rng.UniformReal(0.5, 2.0)) << "\n";
                    } else
                        body << BuildBlock(rng, "v", vLeaves, leaves);
                    if (e == 0)
                        vLeaves.push_back("v");
                    if (e == nEvents - 1)
                        body << "out pays " << BuildExpr(rng, 1, vLeaves) << "\n";
                    //  the last event always exercises, so the product diverts to the LSMC driver
                    if (e == nEvents - 1 || rng.Bernoulli(0.6)) {
                        body << BuildExerciseStatement(rng, vLeaves);
                        ++exerciseCount;
                    }
                    events.push_back(String_(body.str()));
                }
            }
            REQUIRE(exerciseCount > 0, "exercise fuzz generator must emit at least one EXERCISE statement");
            fp.data = std::make_unique<ScriptProductData_>("", eventDates, events);
            return fp;
        }

    private:
        ExerciseFuzzProduct_() = default;
    };

    //  One LSMC parity case: the same generated product through the tree-walk and
    //  compiled engines of the LSMC driver (same Sobol paths, frozen per-engine
    //  regressions); S11 tolerance semantics on the aggregated PV.
    void RunExerciseFuzzCase(uint32_t seed, int structure) {
        Global::Dates_::SetEvaluationDate(Date_(2022, 6, 22));
        const ExerciseFuzzProduct_ fp = ExerciseFuzzProduct_::Build(seed, structure);
        auto model = Handle_<ModelData_>(new BSModelData_("bsmodel", 10.0, 0.20, 0.034, 0.021));
        MonteCarloSettings_ compiled;
        compiled.compiled_ = true;
        const SimResults_ treeWalk = MCSimulation<double>(*fp.data, model, 4096, ScriptValuationSettings_(), MonteCarloSettings_());
        const SimResults_ compiledResults = MCSimulation<double>(*fp.data, model, 4096, ScriptValuationSettings_(), compiled);
        ASSERT_NEAR(compiledResults.aggregated_, treeWalk.aggregated_, 1e-8)
            << "EXERCISE FUZZ DIVERGENCE: seed=" << seed << " structure=" << structure
            << " (treeWalk=" << treeWalk.aggregated_ << ")";
    }
} // namespace

// ~500 products per run across the three structural axes. Each case prints
// its seed on failure for standalone reproduction.

TEST(ScriptCompiledParityFuzzTest, TestFuzz_SingleEvent_300) {
    for (uint32_t seed = 1; seed <= 300; ++seed) {
        SCOPED_TRACE("seed=" + std::to_string(seed));
        ASSERT_NO_FATAL_FAILURE(RunFuzzCase(seed, 0));
    }
}

TEST(ScriptCompiledParityFuzzTest, TestFuzz_MultiEvent_120) {
    for (uint32_t seed = 1001; seed <= 1120; ++seed) {
        SCOPED_TRACE("seed=" + std::to_string(seed));
        ASSERT_NO_FATAL_FAILURE(RunFuzzCase(seed, 1));
    }
}

TEST(ScriptCompiledParityFuzzTest, TestFuzz_Schedule_80) {
    for (uint32_t seed = 2001; seed <= 2080; ++seed) {
        SCOPED_TRACE("seed=" + std::to_string(seed));
        ASSERT_NO_FATAL_FAILURE(RunFuzzCase(seed, 2));
    }
}

//  EXERCISE parity fuzz (T3): the three structural axes with random exercise
//  statements valued through the LSMC driver, tree-walk vs compiled engine.

TEST(ScriptCompiledParityFuzzTest, TestFuzz_Exercise_SingleEvent_100) {
    for (uint32_t seed = 3001; seed <= 3100; ++seed) {
        SCOPED_TRACE("seed=" + std::to_string(seed));
        ASSERT_NO_FATAL_FAILURE(RunExerciseFuzzCase(seed, 0));
    }
}

TEST(ScriptCompiledParityFuzzTest, TestFuzz_Exercise_MultiEvent_60) {
    for (uint32_t seed = 4001; seed <= 4060; ++seed) {
        SCOPED_TRACE("seed=" + std::to_string(seed));
        ASSERT_NO_FATAL_FAILURE(RunExerciseFuzzCase(seed, 1));
    }
}

TEST(ScriptCompiledParityFuzzTest, TestFuzz_Exercise_Schedule_40) {
    for (uint32_t seed = 5001; seed <= 5040; ++seed) {
        SCOPED_TRACE("seed=" + std::to_string(seed));
        ASSERT_NO_FATAL_FAILURE(RunExerciseFuzzCase(seed, 2));
    }
}

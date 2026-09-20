//
// Created by dal-implementer on 2026/9/19.
//

#include <gtest/gtest.h>

#include <cmath>
#include <dal/model/blackscholes.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/diagnostics.hpp>
#include <dal/script/event.hpp>
#include <dal/script/parser.hpp>
#include <dal/script/preparation.hpp>
#include <dal/script/simulation.hpp>
#include <dal/script/visitor/all.hpp>
#include <dal/storage/globals.hpp>

#include "script_test_observers.hpp"

using namespace Dal;
using namespace Dal::Script;
using namespace Dal::Script::TestSupport;

namespace {
    template <class F_> void AssertScriptError(F_ action, const Vector_<String_>& expected) {
        try {
            action();
            FAIL() << "expected a script error";
        } catch (const ScriptError_& error) {
            for (const auto& field : expected)
                ASSERT_NE(std::string(error.what()).find(field.c_str()), std::string::npos) << error.what();
        }
    }

    const NodeExercise_* ExerciseOf(const Event_& event) { return dynamic_cast<const NodeExercise_*>(event[0].get()); }

    // The payoffIdx_ sentinel: no receiver variable (EXERCISE without PAYS)
    constexpr size_t NO_PAYOFF_SLOT = static_cast<size_t>(-1);
} // namespace

TEST(ScriptExerciseParseTest, TestParseExerciseValueOnly) {
    Parser_ parser;
    const auto event = parser.Parse("EXERCISE 1.5");
    ASSERT_EQ(event.size(), 1u);
    const auto* exercise = ExerciseOf(event);
    ASSERT_NE(exercise, nullptr);
    ASSERT_EQ(exercise->arguments_.size(), 1u);
    const auto* value = dynamic_cast<const NodeConst_*>(exercise->arguments_[0].get());
    ASSERT_NE(value, nullptr);
    ASSERT_NEAR(value->constVal_, 1.5, 1e-12);
    ASSERT_EQ(exercise->eps_, -1.0);
}

TEST(ScriptExerciseParseTest, TestParseExerciseWithCondition) {
    Parser_ parser;
    const auto event = parser.Parse("EXERCISE MAX(spot() - 100, 0) IF spot() > 100");
    const auto* exercise = ExerciseOf(event);
    ASSERT_NE(exercise, nullptr);
    ASSERT_EQ(exercise->arguments_.size(), 2u);
    ASSERT_NE(dynamic_cast<const NodeMax_*>(exercise->arguments_[0].get()), nullptr);
    ASSERT_NE(dynamic_cast<const NodeSup_*>(exercise->arguments_[1].get()), nullptr);
    ASSERT_EQ(exercise->eps_, -1.0);
}

TEST(ScriptExerciseParseTest, TestParseExerciseSharesConditionEps) {
    Parser_ parser;
    const auto event = parser.Parse("EXERCISE 1.0 IF spot() > 100; 0.02");
    const auto* exercise = ExerciseOf(event);
    ASSERT_NE(exercise, nullptr);
    ASSERT_EQ(exercise->eps_, 0.02);
    const auto* condition = dynamic_cast<const CompNode_*>(exercise->arguments_[1].get());
    ASSERT_NE(condition, nullptr);
    ASSERT_EQ(condition->eps_, 0.02);

    { // without the ;eps option the exercise falls back to the simulation default
        const auto plain = parser.Parse("EXERCISE 1.0 IF spot() > 100");
        ASSERT_EQ(ExerciseOf(plain)->eps_, -1.0);
    }
}

TEST(ScriptExerciseParseTest, TestParseConditionRejectsNonPositiveEps) {
    //  a zero width parses into CSpr(x, 0) and divides by zero at the kink; the
    //  same suffix feeds fuzzy IF conditions, so both statements reject it
    Parser_ parser;
    AssertScriptError([&] { parser.Parse("EXERCISE 1.0 IF spot() > 100; 0"); }, {"InvalidSmoothing", ";eps", "line="});
    AssertScriptError([&] { parser.Parse("EXERCISE 1.0 IF spot() > 100; 0.0"); }, {"InvalidSmoothing"});
    AssertScriptError([&] { parser.Parse("EXERCISE 1.0 IF spot() > 100: 0"); }, {"InvalidSmoothing"});
    AssertScriptError([&] { parser.Parse("IF spot() > 100; 0 THEN y = 2 END"); }, {"InvalidSmoothing"});
    { //  a positive width still parses and flows to the exercise decision
        const auto event = parser.Parse("EXERCISE 1.0 IF spot() > 100; 0.02");
        ASSERT_EQ(ExerciseOf(event)->eps_, 0.02);
    }
}

TEST(ScriptExerciseParseTest, TestParseExerciseConditionEpsFollowsFirstComparison) {
    //  Multi-comparison conditions: the decision width follows the FIRST comparison's
    //  ;eps option (the simulation default when it carries none); later comparisons
    //  keep their own width for the condition degree itself
    Parser_ parser;
    {
        const auto event = parser.Parse("EXERCISE 1.0 IF spot() > 100 AND spot() < 200; 0.05");
        const auto* exercise = ExerciseOf(event);
        ASSERT_NE(exercise, nullptr);
        ASSERT_EQ(exercise->eps_, -1.0); //  the first comparison carries no ;eps
        const auto* condition = dynamic_cast<const NodeAnd_*>(exercise->arguments_[1].get());
        ASSERT_NE(condition, nullptr);
        const auto* first = dynamic_cast<const CompNode_*>(condition->arguments_[0].get());
        const auto* second = dynamic_cast<const CompNode_*>(condition->arguments_[1].get());
        ASSERT_NE(first, nullptr);
        ASSERT_NE(second, nullptr);
        ASSERT_EQ(first->eps_, -1.0);
        ASSERT_EQ(second->eps_, 0.05); //  the condition degree still smooths at its own width
    }
    {
        const auto event = parser.Parse("EXERCISE 1.0 IF spot() > 100; 0.02 AND spot() < 200");
        ASSERT_EQ(ExerciseOf(event)->eps_, 0.02);
    }
}

TEST(ScriptExerciseParseTest, TestParseExerciseKeywordIsCaseInsensitive) {
    Parser_ parser;
    for (const String_& text : {String_("exercise 1.5"), String_("Exercise 1.5"), String_("ExErCiSe 1.5")}) {
        SCOPED_TRACE(text.c_str());
        ASSERT_NE(ExerciseOf(parser.Parse(text)), nullptr);
    }
}

TEST(ScriptExerciseParseTest, TestExerciseReservedWordRejectsVariableUse) {
    Parser_ parser;
    AssertScriptError([&] { parser.Parse("x = exercise + 1"); }, {"ReservedIdentifier", "EXERCISE", "rename the variable", "line="});
    AssertScriptError([&] { parser.Parse("exercise = 2"); }, {"ReservedIdentifier", "EXERCISE", "rename the variable", "line="});
    AssertScriptError([&] { parser.Parse("exercise PAYS 2"); }, {"ReservedIdentifier", "EXERCISE", "rename the variable", "line="});
}

TEST(ScriptExerciseParseTest, TestExerciseReservedWordRejectsDefinition) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    AssertScriptError([] { static_cast<void>(ScriptProduct_({Cell_("exercise"), Cell_(Date_(2026, 9, 22))}, {"2", "pay PAYS 1"})); },
                      {"ReservedIdentifier", "EXERCISE", "rename the definition", "row=1"});
}

TEST(ScriptExerciseParseTest, TestParseExerciseNestedInIfRejected) {
    Parser_ parser;
    AssertScriptError([&] { parser.Parse("IF x > 1 THEN EXERCISE 1 END"); }, {"UnsupportedExerciseNesting", "line="});
    AssertScriptError([&] { parser.Parse("IF x > 1 THEN y = 2 ELSE EXERCISE 1 END"); }, {"UnsupportedExerciseNesting", "line="});
}

TEST(ScriptExerciseParseTest, TestParseDuplicateExerciseRejected) {
    Parser_ parser;
    AssertScriptError([&] { parser.Parse("EXERCISE 1\nEXERCISE 2"); }, {"DuplicateExercise", "line=", "column=", "offset="});
}

TEST(ScriptExerciseParseTest, TestDuplicateExerciseAcrossSameDateRowsCarriesRow) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    AssertScriptError([] { static_cast<void>(ScriptProduct_({Cell_(Date_(2026, 9, 22)), Cell_(Date_(2026, 9, 22))}, {"EXERCISE 1", "EXERCISE 2"})); },
                      {"DuplicateExercise", "row=2", "event=2026-09-22"});
}

TEST(ScriptExerciseParseTest, TestParseDanglingExerciseConditionRejected) {
    Parser_ parser;
    AssertScriptError([&] { parser.Parse("EXERCISE 1 IF x > 1 THEN y = 2 END"); }, {"InvalidExerciseCondition", "THEN", "line="});
    AssertScriptError([&] { parser.Parse("EXERCISE 1 IF x > 1 END"); }, {"InvalidExerciseCondition", "END", "line="});
    AssertScriptError([&] { parser.Parse("EXERCISE 1 IF x > 1\nIF y > 2 THEN z = 3 END"); }, {"InvalidExerciseCondition", "IF", "line="});
}

TEST(ScriptExerciseParseTest, TestParseExerciseFollowedByOrdinaryStatement) {
    // a following statement that starts with a variable is not a condition boundary
    Parser_ parser;
    const auto event = parser.Parse("EXERCISE 1 IF x > 1\ny = 2");
    ASSERT_EQ(event.size(), 2u);
    ASSERT_NE(ExerciseOf(event), nullptr);
    ASSERT_NE(dynamic_cast<const NodeAssign_*>(event[1].get()), nullptr);
}

namespace {
    Debugger_ Debugged(const String_& text) {
        Parser_ parser;
        auto event = parser.Parse(text);
        Debugger_ debugger;
        event[0]->Accept(debugger);
        return debugger;
    }
} // namespace

TEST(ScriptExerciseParseTest, TestDebuggerRendersExerciseText) {
    ASSERT_EQ(Debugged("EXERCISE 1.5").String(), String_("EXERCISE[CONT,EPS=-1.000000](\n"
                                                         "\tCONST[1.500000]\n"
                                                         ")\n"));
    ASSERT_EQ(Debugged("EXERCISE 1.5 IF spot() > 100; 0.02").String(), String_("EXERCISE[CONT,EPS=0.020000](\n"
                                                                               "\tCONST[1.500000]\n"
                                                                               ",\n"
                                                                               "\tGTZERO[CONT,EPS=0.020000](\n"
                                                                               "\t\tSUBTRACT(\n"
                                                                               "\t\t\tSPOT\n"
                                                                               "\t\t,\n"
                                                                               "\t\t\tCONST[100.000000]\n"
                                                                               "\t\t)\n"
                                                                               "\t)\n"
                                                                               ")\n"));
}

TEST(ScriptExerciseParseTest, TestDebuggerJsonExerciseNode) {
    const auto debugger = Debugged("EXERCISE 1.5 IF spot() > 100; 0.02");
    std::ostringstream out;
    size_t id = 0;
    DebugNodeJson(debugger.Top(), id, out);
    ASSERT_EQ(out.str(), "{\"id\":\"n0\",\"kind\":\"exercise\",\"mode\":\"continuous\",\"eps\":0.02,\"children\":["
                         "{\"id\":\"n1\",\"kind\":\"const\",\"value\":1.5},"
                         "{\"id\":\"n2\",\"kind\":\"gt0\",\"mode\":\"continuous\",\"eps\":0.02,\"children\":["
                         "{\"id\":\"n3\",\"kind\":\"sub\",\"children\":["
                         "{\"id\":\"n4\",\"kind\":\"spot\"},"
                         "{\"id\":\"n5\",\"kind\":\"const\",\"value\":100}"
                         "]}"
                         "]}]}");

    { // no condition: metadata stays, single child
        std::ostringstream plain;
        size_t fresh = 0;
        DebugNodeJson(Debugged("EXERCISE 1.5").Top(), fresh, plain);
        ASSERT_EQ(plain.str(), "{\"id\":\"n0\",\"kind\":\"exercise\",\"mode\":\"continuous\",\"eps\":-1,\"children\":["
                               "{\"id\":\"n1\",\"kind\":\"const\",\"value\":1.5}]}");
    }
}

TEST(ScriptExerciseParseTest, TestDebuggerTreeExerciseStatement) {
    {
        Vector_<String_> lines;
        DebugNodeTree(Debugged("EXERCISE 1.5").Top(), "(1) ", "    ", TreeStyle(false), 125, lines);
        ASSERT_EQ(lines.size(), 1u);
        ASSERT_EQ(lines[0], String_("(1) exercise 1.5"));
    }
    {
        Vector_<String_> lines;
        DebugNodeTree(Debugged("EXERCISE 1.5 IF spot() > 100; 0.02").Top(), "(1) ", "    ", TreeStyle(false), 125, lines);
        ASSERT_EQ(lines.size(), 1u);
        // the ;eps hint renders once, on the condition's comparison
        ASSERT_EQ(lines[0], String_("(1) exercise 1.5 if spot() > 100 ⟨ε=0.02⟩"));
    }
    { // too wide: keyword head, value branch, unconnected condition branch
        Vector_<String_> lines;
        DebugNodeTree(Debugged("EXERCISE MAX(spot() - 100, 0) IF spot() > 100").Top(), "(1) ", "    ", TreeStyle(false), 20, lines);
        ASSERT_GT(lines.size(), 1u);
        ASSERT_EQ(lines[0], String_("(1) exercise"));
        ASSERT_NE(String_(lines[1].c_str()).find("max"), String_::npos);
        bool hasConditionBranch = false;
        for (const auto& line : lines)
            if (String_(line.c_str()).find("if spot() > 100") != String_::npos)
                hasConditionBranch = true;
        ASSERT_TRUE(hasConditionBranch);
    }
}

TEST(ScriptExerciseParseTest, TestDescribeExerciseProductJsonGolden) {
    const ScriptProductData_ data("ex", {Cell_(Date_(2026, 9, 22))}, {"EXERCISE 1.5"});
    ASSERT_EQ(DescribeScriptProductData(data),
              String_("{\"schema\":\"dal.script-product/2\",\"name\":\"ex\",\"default_index\":{\"original\":\"\",\"canonical\":null},"
                      "\"input_rows\":[{\"row\":1,\"date_or_definition\":\"2026-09-22\",\"text\":\"EXERCISE 1.5\"}],"
                      "\"variables\":[],\"constants\":[],\"payoff_index\":null,"
                      "\"events\":[{\"event_id\":0,\"date\":\"2026-09-22\",\"origins\":[{\"row\":1,\"offset\":0,\"event_date\":\"2026-09-22\"}],"
                      "\"statements\":[{\"id\":\"n0\",\"kind\":\"exercise\",\"mode\":\"continuous\",\"eps\":-1,\"children\":["
                      "{\"id\":\"n1\",\"kind\":\"const\",\"value\":1.5}]}]}]}"));

    { // a variable without PAYS keeps the sentinel: payoff_index stays null
        const ScriptProductData_ withVariable("ex", {Cell_(Date_(2026, 9, 22))}, {"x = 1\nEXERCISE x"});
        const auto described = DescribeScriptProductData(withVariable);
        ASSERT_NE(described.find(String_("\"variables\":[{\"index\":0,\"name\":\"x\"}]")), String_::npos);
        ASSERT_NE(described.find(String_("\"payoff_index\":null")), String_::npos);
    }
}

TEST(ScriptExerciseParseTest, TestDebuggerTreeExerciseProductGolden) {
    const ScriptProduct_ product({Cell_(Date_(2026, 9, 22))}, {"EXERCISE 1.5"});
    std::ostringstream out;
    product.DebugTree(out);
    ASSERT_EQ(out.str(), "📅 1 · 2026-09-22 · future\n"
                         "└── (1) exercise 1.5\n"
                         "\n");
}

TEST(ScriptExerciseParseTest, TestLegacySchemaOneGateRejectsExercise) {
    // EXERCISE-only products must not leak into the legacy /1 dump
    const ScriptProduct_ product({Cell_(Date_(2026, 9, 22))}, {"EXERCISE 1.5"});
    std::ostringstream json;
    AssertScriptError([&] { product.DebugJson(json); }, {"DebugSchemaUnsupported", "dal.script-product/1", "EXERCISE", "DescribeScriptProduct"});
    ASSERT_TRUE(json.str().empty());
}

namespace {
    Handle_<ModelData_> TestModel() { return Handle_<ModelData_>(new BSModelData_("", 100.0, 0.2, 0.05)); }
} // namespace

TEST(ScriptExerciseParseTest, TestExerciseOnlyPassesPayoffGate) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    const auto model = TestModel();
    const auto prepared = PrepareScript(ScriptTestProduct("EXERCISE 1.0"), CreateModel<double>(model).get(), {}, {});
    ASSERT_FALSE(prepared.AllExpired());
    // T2 unlocked tree-walk and T3 the compiled evaluator: both engines value the single path
    const auto valued = MCSimulation<double>(ScriptTestProduct("EXERCISE 1.0"), model, 1, {}, {});
    ASSERT_NEAR(valued.aggregated_, std::exp(-0.05 * 10.0 / 365.0), 1e-12); //  single path exercises for 1.0 on 2026-09-22
    const auto compiledValued =
        MCSimulation<double>(ScriptTestProduct("EXERCISE 1.0"), model, 1, {}, MonteCarloSettings_{"sobol", false, false, 0.01, true});
    ASSERT_NEAR(compiledValued.aggregated_, valued.aggregated_, 1e-12);
}

TEST(ScriptExerciseParseTest, TestExerciseDateMustBeStrictlyAfterEvaluation) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    const auto model = TestModel();
    { // past exercise events are not replayable
        AssertScriptError(
            [&] {
                static_cast<void>(PrepareScript(ScriptTestProduct("EXERCISE 1.0", Date_(2026, 9, 11)), CreateModel<double>(model).get(), {}, {}));
            },
            {"UnsupportedExerciseDate", "2026-09-11", "2026-09-12", "row=1", "strictly after"});
    }
    { // same-day exercise is excluded by S1
        AssertScriptError(
            [&] {
                static_cast<void>(PrepareScript(ScriptTestProduct("EXERCISE 1.0", Date_(2026, 9, 12)), CreateModel<double>(model).get(), {}, {}));
            },
            {"UnsupportedExerciseDate", "2026-09-12", "row=1"});
    }
}

TEST(ScriptExerciseParseTest, TestExerciseRequiresSobolRsg) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    const auto model = TestModel();
    for (const String_& rsg : {String_("mrg32"), String_("irn")}) {
        AssertScriptError(
            [&] {
                static_cast<void>(PrepareScript(ScriptTestProduct("EXERCISE 1.0"), CreateModel<double>(model).get(), {}, MonteCarloSettings_{rsg}));
            },
            {"UnsupportedRsgForExercise", "rsg=" + rsg, "use method='sobol'", "row=1"});
    }
    { // sobol preparation succeeds
        ASSERT_NO_THROW(
            static_cast<void>(PrepareScript(ScriptTestProduct("EXERCISE 1.0"), CreateModel<double>(model).get(), {}, MonteCarloSettings_{"sobol"})));
    }
}

TEST(ScriptExerciseParseTest, TestHistoryOnlyPreparationRejectsExercise) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    AssertScriptError([&] { static_cast<void>(PrepareScript(ScriptTestProduct("EXERCISE 1.0"))); }, {"UnsupportedExecutionMode"});
}

TEST(ScriptExerciseParseTest, TestLegacyPreProcessRejectsExercise) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    ScriptProduct_ product({Cell_(Date_(2026, 9, 22))}, {"x = 1\nEXERCISE x"});
    AssertScriptError([&] { static_cast<void>(product.PreProcess(false, true)); }, {"UnsupportedExecutionMode"});
}

TEST(ScriptExerciseParseTest, TestPayoffReceiverRoutingFollowsPays) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    { // PAYS present: the payoff variable keeps its holder role
        ScriptProduct_ product({Cell_(Date_(2026, 9, 22)), Cell_(Date_(2026, 9, 22)), Cell_(Date_(2026, 9, 22))},
                               {"x = 1", "y PAYS x", "EXERCISE y"});
        product.IndexVariables();
        ASSERT_EQ(product.PayOffIdx(), 1u);
    }
    { // no PAYS with EXERCISE: no receiver, the sentinel stays
        ScriptProduct_ product({Cell_(Date_(2026, 9, 22))}, {"x = 1\nEXERCISE x"});
        product.IndexVariables();
        ASSERT_EQ(product.PayOffIdx(), NO_PAYOFF_SLOT);
        ASSERT_TRUE(product.HasPayoff());
        ASSERT_FALSE(product.HasPays());
    }
    { // without EXERCISE the legacy default is untouched
        ScriptProduct_ product({Cell_(Date_(2026, 9, 22)), Cell_(Date_(2026, 9, 22))}, {"x = 1", "y = x + 1"});
        product.IndexVariables();
        ASSERT_EQ(product.PayOffIdx(), 1u);
    }
}

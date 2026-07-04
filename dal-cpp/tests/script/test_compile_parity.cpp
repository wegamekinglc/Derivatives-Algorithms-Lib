//
// Created by wegame on 2026/07/04.
//
// Parity guards for tree-walk and compiled script evaluators.
//

#include <gtest/gtest.h>

#include <set>
#include <string>

#include <dal/platform/platform.hpp>
#include <dal/math/aad/aad.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/storage/globals.hpp>
#include <dal/script/event.hpp>
#include <dal/script/simulation.hpp>
#include <dal/script/visitor/compiler.hpp>

using namespace Dal;
using namespace Dal::AAD;
using namespace Dal::Script;

namespace {
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

    void AssertPerPathParity(const ScriptProduct_& product, double spot, double tol = 1e-8) {
        Scenario_<double> scenario(product.EventDates().size());
        for (auto& s : scenario) {
            s.spot_ = spot;
            s.numeraire_ = 1.0;
        }

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

TEST(ScriptCompiledParityTest, TestParity_VanillaCall) {
    Global::Dates_::SetEvaluationDate(Date_(2022, 6, 22));
    Date_ exerciseDate(2024, 6, 21);
    VanillaProduct_ vanilla(exerciseDate, 11.0, "call pays MAX(spot() - STRIKE, 0.0)");
    ScriptProduct_ product = vanilla.Build();
    product.PreProcess(false, false);

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

    auto model = StandardBSModel(10.0, 0.20, 0.034, 0.021);
    AssertAggregatedParityDouble(product, model, 4096);
}

TEST(ScriptCompiledParityTest, TestParity_TreeWalkUnchangedAfterCompile) {
    Global::Dates_::SetEvaluationDate(Date_(2022, 6, 22));
    Date_ exerciseDate(2024, 6, 21);
    VanillaProduct_ vanilla(exerciseDate, 11.0, "call pays MAX(spot() - STRIKE, 0.0)");
    ScriptProduct_ product = vanilla.Build();
    product.PreProcess(false, false);

    auto model = StandardBSModel(10.0, 0.20, 0.034, 0.021);
    const SimResults_ before = MCSimulation<double>(product, model, 2048, "sobol", false, false);

    const ScriptCompiled_ compiled = product.Compile();

    const SimResults_ after = MCSimulation<double>(product, model, 2048, "sobol", false, false);

    ASSERT_NEAR(after.aggregated_, before.aggregated_, 1e-8)
        << "tree-walk PV changed after Compile(); Compile() must not mutate the AST in a tree-walk-visible way";
}

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
        << "<double> default diverges from explicit tree-walk";
}

TEST(ScriptCompiledParityTest, TestParity_PastEvents_InitSeeding) {
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

    AssertPerPathParity(product, 1.0);

    auto model = StandardBSModel(10.0, 0.20, 0.034, 0.021);
    AssertAggregatedParityDouble(product, model, 2048);
}

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
    for (const double spot : {0.5, 5.0, 10.0}) {
        SCOPED_TRACE("spot=" + std::to_string(spot));
        AssertPerPathParity(product, spot);
    }
}

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

TEST(ScriptCompiledParityTest, TestParity_CompileNotCalled_GuardsThrow) {
    Global::Dates_::SetEvaluationDate(Date_(2022, 6, 22));
    Date_ exerciseDate(2024, 6, 21);
    VanillaProduct_ vanilla(exerciseDate, 11.0, "call pays MAX(spot() - STRIKE, 0.0)");
    ScriptProduct_ product = vanilla.Build();
    ASSERT_THROW(static_cast<void>(product.Compile()), Dal::Exception_);

    auto model = StandardBSModel(10.0, 0.20, 0.034, 0.021);
    ASSERT_THROW(MCSimulation<double>(product, model, 64, "sobol", false, true), Dal::Exception_);
}

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

    const size_t nParams = 4;
    ASSERT_NEAR(compiled.risks_[nParams], treeWalk.risks_[nParams], 1e-8)
        << "const-var STRIKE greek: compiled (" << compiled.risks_[nParams]
        << ") vs tree-walk (" << treeWalk.risks_[nParams] << ")";
}

TEST(ScriptCompiledParityTest, TestParity_ConstVarVals_MutationSeam) {
    Global::Dates_::SetEvaluationDate(Date_(2022, 6, 22));
    Date_ exerciseDate(2024, 6, 21);
    Vector_<Cell_> eventDates{Cell_(String_("STRIKE")), Cell_(exerciseDate)};
    Vector_<String_> events{"11.0", "call pays MAX(spot() - STRIKE, 0.0)"};
    ScriptProduct_ product(eventDates, events);
    product.PreProcess(false, false);
    const ScriptCompiled_ compiled = product.Compile();

    Scenario_<double> scenario(1);
    scenario[0].spot_ = 13.0;
    scenario[0].numeraire_ = 1.0;

    Evaluator_<double> treeEval = product.BuildEvaluator<double>();
    treeEval.ConstVarVals()[0] = 5.0;
    product.Evaluate(scenario, treeEval);
    const double treePayoff = treeEval.VarVals()[product.PayOffIdx()];

    EvalState_<double> compiledState = product.BuildEvalState<double>();
    compiledState.ConstVarVals()[0] = 5.0;
    compiled.Evaluate(scenario, compiledState);
    const double compiledPayoff = compiledState.VarVals()[product.PayOffIdx()];

    ASSERT_NEAR(compiledPayoff, treePayoff, 1e-8);
}

TEST(ScriptCompiledParityTest, TestParity_ConstCondProcessed_Collect) {
    Global::Dates_::SetEvaluationDate(Date_(2023, 1, 1));
    Vector_<Cell_> eventDates{Cell_(Date_(2023, 1, 28))};
    Vector_<String_> events{R"(
        IF 2 >= 1 THEN
            x = spot() + 3
        END
    )"};
    ScriptProduct_ product(eventDates, events, "x");
    product.PreProcess(false, false);
    AssertPerPathParity(product, 5.0);
}

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

TEST(ScriptCompiledParityTest, TestParity_Barrier_Sup) {
    Global::Dates_::SetEvaluationDate(Date_(2022, 6, 22));
    ScriptProduct_ product = FixedBarrierProduct();
    product.PreProcess(false, false);

    for (const double spot : {10.0, 13.0, 16.0}) {
        SCOPED_TRACE("spot=" + std::to_string(spot));
        AssertPerPathParity(product, spot);
    }

    auto model = StandardBSModel(10.0, 0.20, 0.034, 0.021);
    AssertAggregatedParityDouble(product, model, 4096);
}

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

TEST(ScriptCompiledParityTest, TestGolden_FixedBarrier_PV_Risks) {
    Global::Dates_::SetEvaluationDate(Date_(2022, 6, 22));
    ScriptProduct_ product = FixedBarrierProduct();
    const int maxNested = static_cast<int>(product.PreProcess(true, false));

    auto model = StandardBSModel(10.0, 0.20, 0.034, 0.021);
    const SimResults_ results = MCSimulation<Number_>(product, model, 4096, "sobol", false, false, maxNested);

    ASSERT_NEAR(results.aggregated_, 2616.68830169999, 1e-6);

    ASSERT_EQ(results.risks_.size(), 6u);
    ASSERT_NEAR(results.risks_[0], 0.392061230359593, 1e-6);
    ASSERT_NEAR(results.risks_[1], 4.25364478488146, 1e-6);
    ASSERT_NEAR(results.risks_[2], 6.56354477237741, 1e-6);
    ASSERT_NEAR(results.risks_[3], -7.84122460719187, 1e-6);
    ASSERT_NEAR(results.risks_[4], 0.0, 1e-6);
    ASSERT_NEAR(results.risks_[5], -0.298342944198973, 1e-6);
}

namespace {
    bool IsOneOperandOpcode(int op) {
        static const std::set<int> ops = {
            AddConst, SubConst, ConstSub, MultiConst, DivConst, ConstDiv,
            PowConst, ConstPow, Max2Const, Min2Const, Var, Const, ConstVar,
            Assign, Pays, If, FuzzyEqual, FuzzyComp
        };
        return ops.count(op) != 0;
    }

    bool IsTwoOperandOpcode(int op) {
        static const std::set<int> ops = {
            AssignConst, PaysConst, IfElse, FuzzyEqualDiscrete, FuzzyCompDiscrete
        };
        return ops.count(op) != 0;
    }

    size_t OpcodeWidth(const Vector_<int>& stream, size_t idx) {
        const int op = stream[idx];
        if (IsOneOperandOpcode(op))
            return 2;
        if (IsTwoOperandOpcode(op))
            return 3;
        if (op == FuzzyIf)
            return 4 + stream[idx + 3];
        return 1;
    }

    void CollectOpcodes(const Vector_<int>& stream, std::set<int>* out) {
        size_t i = 0;
        while (i < stream.size()) {
            out->insert(stream[i]);
            i += OpcodeWidth(stream, i);
        }
    }

    void MergeProductOpcodes(const String_& body, std::set<int>* out) {
        Vector_<Cell_> eventDates{Cell_(Date_(2023, 1, 28))};
        Vector_<String_> events{body};
        ScriptProduct_ product(eventDates, events);
        product.PreProcess(false, true);
        const ScriptCompiled_ compiled = product.Compile();
        for (const auto& stream : compiled.NodeStreams())
            CollectOpcodes(stream, out);
    }

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

    MergeConstVarProductOpcodes(&seen);

    MergeFuzzyProductOpcodes(&seen);

    const std::set<int> unreachable = {Const, 31};
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

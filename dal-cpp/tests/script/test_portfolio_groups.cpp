//
// Created by Codex on 2026/10/6.
//

#include <gtest/gtest.h>

#include <cmath>
#include <functional>
#include <limits>

#include <dal/model/blackscholes.hpp>
#include <dal/script/portfoliogroups.hpp>

using namespace Dal;
using namespace Dal::Script;
using namespace Dal::Script::Detail;

namespace {
    PreparedPortfolioTradeView_ View(const PreparedScript_& script, size_t owner, const AAD::Model_<double>& model) {
        return {&script, owner, model.SimDim(), model.NumFactors(), model.NumeraireIsDeterministic(), true};
    }

    void CheckPrivateHistory(const PreparedScript_& a, const PreparedScript_& b, bool compiled, double divisor) {
        AAD::Scenario_<double> path;
        AAD::AllocatePath(a.DefLine(), path);
        AAD::InitializePath(path);
        const auto runPaths = [&](auto& stateA, auto& stateB, const auto& evaluate) {
            for (const double spot : {100.0, 120.0, 100.0}) {
                path[0].spot_ = spot;
                path[0].observations_[0] = spot;
                evaluate(a, stateA);
                evaluate(b, stateB);
                ASSERT_DOUBLE_EQ(stateA.VarVals()[a.PayOffIdx()], (400.0 + spot) / divisor);
                ASSERT_DOUBLE_EQ(stateB.VarVals()[b.PayOffIdx()], (560.0 + spot) / divisor);
                ASSERT_NE(stateA.VarVals().data(), stateB.VarVals().data());
            }
        };
        if (compiled) {
            auto stateA = a.BuildEvalState<double>();
            auto stateB = b.BuildEvalState<double>();
            runPaths(stateA, stateB, [&](const auto& prepared, auto& state) { prepared.CompiledProgram().Evaluate(path, state); });
        } else {
            auto stateA = a.BuildEvaluator<double>();
            auto stateB = b.BuildEvaluator<double>();
            runPaths(stateA, stateB, [&](const auto& prepared, auto& state) { prepared.Evaluate(path, state); });
        }
    }
} // namespace

TEST(PortfolioGroupingTest, TestStableGroupsPreserveOwnerAndOriginalTimeline) {
    ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = Date_(2026, 1, 1);
    const ScriptProductData_ a("A", {Cell_(Date_(2027, 1, 1))}, {"pay PAYS 2 * SPOT()"});
    const ScriptProductData_ b("B", {Cell_(Date_(2027, 1, 1))}, {"pay PAYS 3 * SPOT()"});
    const ScriptProductData_ later("C", {Cell_(Date_(2027, 1, 1)), Cell_(Date_(2028, 1, 1))}, {"x = SPOT()", "pay PAYS x + SPOT()"});
    AAD::BlackScholes_<double> firstModel(100.0, 0.2);
    AAD::BlackScholes_<double> equalModel(100.0, 0.2);
    const auto preparedA = PrepareScript(a, &firstModel, valuation, {});
    const auto viewA = View(preparedA, 0, firstModel);
    const auto preparedB = PrepareScript(b, &equalModel, valuation, {});
    const auto preparedLater = PrepareScript(later, &firstModel, valuation, {});
    const auto groups =
        GroupPreparedPortfolio({viewA, View(preparedB, 1, equalModel), View(preparedB, 0, equalModel), View(preparedLater, 0, firstModel)});
    ASSERT_EQ(groups.size(), 3);
    ASSERT_EQ(groups[0].modelOwner_, 0);
    ASSERT_EQ(groups[0].tradePositions_, Vector_<size_t>({0, 2}));
    ASSERT_EQ(groups[1].modelOwner_, 1);
    ASSERT_EQ(groups[1].tradePositions_, Vector_<size_t>({1}));
    ASSERT_EQ(groups[2].modelOwner_, 0);
    ASSERT_EQ(groups[2].tradePositions_, Vector_<size_t>({3}));
    ASSERT_EQ(preparedA.TimeLine().size(), 1);
    ASSERT_EQ(preparedLater.TimeLine().size(), 2);
    ASSERT_EQ(viewA.randomDimension_, 1);
    ASSERT_EQ(groups[0].tradePositions_.size() + groups[1].tradePositions_.size() + groups[2].tradePositions_.size(), 4);
}

TEST(PortfolioGroupingTest, TestCompleteSampleDefinitionsCompareValuesAndOrder) {
    AAD::SampleDef_ sample;
    sample.indexNames_ = {"EQ[A]", "EQ[B]"};
    sample.discountMats_ = {0.5, 1.0};
    sample.liborDefs_ = {{0.25, 0.5, "curve"}};
    sample.forwardMats_ = {{1.0, 2.0}, {3.0}};
    ASSERT_TRUE(SamePortfolioSampleDefinition(sample, sample));
    for (const auto& change : Vector_<std::function<void(AAD::SampleDef_*)>>{
             [](auto* x) { x->numeraire_ = false; }, [](auto* x) { std::swap(x->indexNames_[0], x->indexNames_[1]); },
             [](auto* x) { x->indexNames_[1] = "EQ[C]"; }, [](auto* x) { x->discountMats_[1] = std::nextafter(1.0, 2.0); },
             [](auto* x) { x->liborDefs_[0].start_ = 0.3; }, [](auto* x) { x->liborDefs_[0].end_ = 0.7; },
             [](auto* x) { x->liborDefs_[0].curve_ = "other"; }, [](auto* x) { x->liborDefs_.emplace_back(0.25, 0.5, "curve"); },
             [](auto* x) { std::swap(x->forwardMats_[0][0], x->forwardMats_[0][1]); }, [](auto* x) { x->forwardMats_[1].push_back(4.0); }}) {
        auto other = sample;
        change(&other);
        ASSERT_FALSE(SamePortfolioSampleDefinition(sample, other));
        ASSERT_FALSE(SamePortfolioSampleDefinition(other, sample));
    }
    sample.discountMats_[0] = std::numeric_limits<double>::quiet_NaN();
    ASSERT_FALSE(SamePortfolioSampleDefinition(sample, sample));
}

TEST(PortfolioGroupingTest, TestUnprovedSharingAndModelDimensionsKeepSeparateGroups) {
    ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = Date_(2026, 1, 1);
    const ScriptProductData_ product("", {Cell_(Date_(2027, 1, 1))}, {"pay PAYS SPOT()"});
    AAD::BlackScholes_<double> model(100.0, 0.2);
    const auto prepared = PrepareScript(product, &model, valuation, {});
    const auto first = View(prepared, 0, model);
    for (const auto& change : Vector_<std::function<void(PreparedPortfolioTradeView_*)>>{
             [](auto* x) { ++x->randomDimension_; }, [](auto* x) { ++x->factors_; }, [](auto* x) { x->deterministicNumeraire_ = false; },
             [](auto* x) { x->canShareScenario_ = false; }}) {
        auto second = first;
        change(&second);
        const auto groups = GroupPreparedPortfolio({first, second});
        ASSERT_EQ(groups.size(), 2);
        ASSERT_EQ(groups[0].tradePositions_, Vector_<size_t>({0}));
        ASSERT_EQ(groups[1].tradePositions_, Vector_<size_t>({1}));
    }
    auto unproved = first;
    unproved.canShareScenario_ = false;
    ASSERT_EQ(GroupPreparedPortfolio({unproved, unproved, first, first}).size(), 3);
}

TEST(PortfolioGroupingTest, TestSimulationContractsCannotBeMergedByGeometryAlone) {
    ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = Date_(2026, 1, 1);
    const ScriptProductData_ product("", {Cell_(Date_(2027, 1, 1))}, {"pay PAYS SPOT()"});
    AAD::BlackScholes_<double> model(100.0, 0.2);
    MonteCarloSettings_ base;
    base.enableAad_ = true;
    base.compiled_ = false;
    const auto prepared = PrepareScript(product, &model, valuation, base);
    const auto first = View(prepared, 0, model);
    for (const auto& change : Vector_<std::function<void(MonteCarloSettings_*)>>{
             [](auto* x) { x->rsg_ = "mrg32"; }, [](auto* x) { x->useBb_ = true; }, [](auto* x) { x->enableAad_ = false; },
             [](auto* x) { x->smooth_ = 0.02; }, [](auto* x) { x->compiled_ = true; }, [](auto* x) { x->lsmcBasisDegree_ = 2; },
             [](auto* x) { x->lsmcTrainingPaths_ = 10; }, [](auto* x) { x->lsmcValidationPaths_ = 10; }, [](auto* x) { x->lsmcRqmcReplicates_ = 2; },
             [](auto* x) { x->lsmcPolicyRiskMode_ = "RetrainedBump"; }, [](auto* x) { x->lsmcPolicyBumpRelative_ = 0.002; }}) {
        auto simulation = base;
        change(&simulation);
        const auto other = PrepareScript(product, &model, valuation, simulation);
        ASSERT_EQ(GroupPreparedPortfolio({first, View(other, 0, model)}).size(), 2);
    }
    base.lsmcRqmcReplicates_ = 2;
    base.lsmcTrainingSeed_ = 0;
    base.lsmcPricingSeed_ = 0;
    const auto replicated = PrepareScript(product, &model, valuation, base);
    for (const bool training : {false, true}) {
        auto simulation = base;
        if (training)
            simulation.lsmcTrainingSeed_ = 1;
        else
            simulation.lsmcPricingSeed_ = 1;
        const auto other = PrepareScript(product, &model, valuation, simulation);
        ASSERT_EQ(GroupPreparedPortfolio({View(replicated, 0, model), View(other, 0, model)}).size(), 2);
    }
}

TEST(PortfolioGroupingTest, TestFixingMeaningAndDelayedPaymentsKeepSeparateGroups) {
    ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = Date_(2026, 1, 1);
    const Date_ event(2027, 1, 1);
    const ScriptProductData_ a("", {Cell_(event)}, {"pay PAYS FIX(EQ[A])"});
    const ScriptProductData_ b("", {Cell_(event)}, {"pay PAYS FIX(EQ[B])"});
    const ScriptProductData_ delayed("", {Cell_(event)}, {"pay PAYS FIX(EQ[A]) ON 2028-01-01"});
    AAD::BlackScholes_<double> model(100.0, 0.2, 0.03);
    const auto preparedA = PrepareScript(a, &model, valuation, {});
    const auto preparedB = PrepareScript(b, &model, valuation, {});
    const auto preparedDelayed = PrepareScript(delayed, &model, valuation, {});
    ASSERT_EQ(preparedA.Plan().Requests()[0].modelSlot_->outputId_, preparedB.Plan().Requests()[0].modelSlot_->outputId_);
    ASSERT_EQ(GroupPreparedPortfolio({View(preparedA, 0, model), View(preparedB, 0, model), View(preparedDelayed, 0, model)}).size(), 3);
    ASSERT_EQ(preparedA.TimeLine(), preparedDelayed.TimeLine());
    ASSERT_TRUE(preparedA.DefLine()[0].discountMats_.empty());
    ASSERT_EQ(preparedDelayed.DefLine()[0].discountMats_.size(), 1);
}

TEST(PortfolioGroupingTest, TestHistoricalKeysAndValuesUseSemanticEquality) {
    ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = Date_(2026, 1, 2);
    valuation.fixings_ = Handle_<MarketFixingSnapshot_>(
        new MarketFixingSnapshot_({{"EQ[A]", {{DateTime_(Date_(2026, 1, 1), 0.0), 80.0}}}, {"EQ[B]", {{DateTime_(Date_(2026, 1, 1), 0.0), 20.0}}}}));
    const Vector_<Cell_> dates{Cell_(Date_(2026, 1, 1)), Cell_(Date_(2027, 1, 1))};
    const ScriptProductData_ a("", dates, {"x = FIX(EQ[A]) + FIX(EQ[B])", "pay PAYS x + FIX(EQ[MODEL])"});
    const ScriptProductData_ reversed("", dates, {"x = FIX(EQ[B]) + FIX(EQ[A])", "pay PAYS x + FIX(EQ[MODEL])"});
    AAD::BlackScholes_<double> model(100.0, 0.2);
    const auto preparedA = PrepareScript(a, &model, valuation, {});
    const auto preparedB = PrepareScript(reversed, &model, valuation, {});
    ASSERT_NE(preparedA.Plan().Requests()[0].key_.canonicalIndex_, preparedB.Plan().Requests()[0].key_.canonicalIndex_);
    ASSERT_EQ(GroupPreparedPortfolio({View(preparedA, 0, model), View(preparedB, 0, model)}).size(), 1);
    valuation.fixings_ = Handle_<MarketFixingSnapshot_>(
        new MarketFixingSnapshot_({{"EQ[A]", {{DateTime_(Date_(2026, 1, 1), 0.0), 81.0}}}, {"EQ[B]", {{DateTime_(Date_(2026, 1, 1), 0.0), 20.0}}}}));
    const auto changed = PrepareScript(a, &model, valuation, {});
    ASSERT_EQ(GroupPreparedPortfolio({View(preparedA, 0, model), View(changed, 0, model)}).size(), 2);
}

TEST(PortfolioGroupingTest, TestSharedScenarioKeepsPrivateHistoricalScalarState) {
    ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = Date_(2026, 1, 2);
    valuation.fixings_ = Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_({{"EQ[PAST]", {{DateTime_(Date_(2026, 1, 1), 0.0), 80.0}}}}));
    const Vector_<Cell_> dates{Cell_("X"), Cell_(Date_(2026, 1, 1)), Cell_(Date_(2027, 1, 1))};
    const ScriptProductData_ a("", dates, {"5", "state = X * FIX(EQ[PAST])", "pay PAYS state + FIX(EQ[MODEL])"});
    const ScriptProductData_ b("", dates, {"7", "state = X * FIX(EQ[PAST])", "pay PAYS state + FIX(EQ[MODEL])"});
    for (const bool compiled : {false, true}) {
        MonteCarloSettings_ simulation;
        simulation.compiled_ = compiled;
        AAD::BlackScholes_<double> model(100.0, 0.2);
        const auto preparedA = PrepareScript(a, &model, valuation, simulation);
        const auto preparedB = PrepareScript(b, &model, valuation, simulation);
        ASSERT_EQ(GroupPreparedPortfolio({View(preparedA, 0, model), View(preparedB, 0, model)}).size(), 1);
        CheckPrivateHistory(preparedA, preparedB, compiled, 1.0);
    }
}

TEST(PortfolioGroupingTest, TestSharedScenarioKeepsPrivateHistoricalVectorsAcrossPaths) {
    ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = Date_(2026, 1, 2);
    valuation.fixings_ = Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_({{"EQ[PAST]", {{DateTime_(Date_(2026, 1, 1), 0.0), 80.0}}}}));
    const Vector_<Cell_> dates{Cell_("X"), Cell_(Date_(2026, 1, 1)), Cell_(Date_(2027, 1, 1))};
    const ScriptProductData_ a("", dates, {"5", "APPEND(v, X * FIX(EQ[PAST]))", "APPEND(v, FIX(EQ[MODEL])) pay PAYS AVERAGE(v)"});
    const ScriptProductData_ b("", dates, {"7", "APPEND(v, X * FIX(EQ[PAST]))", "APPEND(v, FIX(EQ[MODEL])) pay PAYS AVERAGE(v)"});
    for (const bool compiled : {false, true}) {
        MonteCarloSettings_ simulation;
        simulation.compiled_ = compiled;
        AAD::BlackScholes_<double> model(100.0, 0.2);
        const auto preparedA = PrepareScript(a, &model, valuation, simulation);
        const auto preparedB = PrepareScript(b, &model, valuation, simulation);
        ASSERT_EQ(GroupPreparedPortfolio({View(preparedA, 0, model), View(preparedB, 0, model)}).size(), 1);
        CheckPrivateHistory(preparedA, preparedB, compiled, 2.0);
    }
}

TEST(PortfolioGroupingTest, TestValuationDatePolicyAndFixingSourceCannotBeIgnored) {
    ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = Date_(2026, 1, 1);
    const ScriptProductData_ product("", {Cell_(Date_(2027, 1, 1))}, {"pay PAYS 1"});
    AAD::BlackScholes_<double> model(100.0, 0.2);
    const auto prepared = PrepareScript(product, &model, valuation, {});
    const auto first = View(prepared, 0, model);
    for (const auto& change : Vector_<std::function<void(ScriptValuationSettings_*)>>{
             [](auto* x) { x->evaluationDate_ = Date_(2026, 1, 2); },
             [](auto* x) { x->todayFixingPolicy_ = TodayFixingPolicy_("RequireHistorical"); },
             [](auto* x) { x->fixings_ = Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_({})); }}) {
        auto otherSettings = valuation;
        change(&otherSettings);
        const auto other = PrepareScript(product, &model, otherSettings, {});
        ASSERT_EQ(GroupPreparedPortfolio({first, View(other, 0, model)}).size(), 2);
    }
}

TEST(PortfolioGroupingTest, TestInvalidAndUnsupportedViewsFailBeforeGrouping) {
    ASSERT_THROW(static_cast<void>(GroupPreparedPortfolio({})), ScriptError_);
    ASSERT_THROW(static_cast<void>(GroupPreparedPortfolio({PreparedPortfolioTradeView_{}})), ScriptError_);
    ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = Date_(2026, 1, 1);
    const ScriptProductData_ product("", {Cell_(Date_(2027, 1, 1))}, {"pay PAYS 1"});
    const auto historyOnly = PrepareScript(product, valuation);
    ASSERT_THROW(static_cast<void>(GroupPreparedPortfolio({{&historyOnly}})), ScriptError_);
    AAD::BlackScholes_<double> model(100.0, 0.2);
    const ScriptProductData_ exercise("", {Cell_(Date_(2027, 1, 1))}, {"EXERCISE 1"});
    const auto exercising = PrepareScript(exercise, &model, valuation, {});
    ASSERT_THROW(static_cast<void>(GroupPreparedPortfolio({View(exercising, 0, model)})), ScriptError_);
    valuation.evaluationDate_ = Date_(2028, 1, 1);
    const auto expired = PrepareScript(product, valuation);
    ASSERT_THROW(static_cast<void>(GroupPreparedPortfolio({{&expired}})), ScriptError_);
}

//
// Created by Codex on 2026/10/6.
//

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

#include <dal/model/blackscholes.hpp>
#include <dal/script/portfoliobatch.hpp>
#include <dal/script/simulation.hpp>

using namespace Dal;
using namespace Dal::Script;

namespace {
    ScriptValuationSettings_ Valuation() {
        ScriptValuationSettings_ result;
        result.evaluationDate_ = Date_(2026, 1, 1);
        return result;
    }

    Handle_<ScriptProductData_> Trade(double constant, const String_& event) {
        return Handle_<ScriptProductData_>(
            new ScriptProductData_("", {Cell_("X"), Cell_(Date_(2027, 1, 1))}, {String_(std::to_string(constant)), event}));
    }

    Dal::Script::Detail::PortfolioBatchOutput_
    Output(const Dal::Script::Detail::PreparedPortfolio_& portfolio, size_t trade, size_t slot, double weight) {
        const auto& prepared = portfolio.Trades()[trade];
        auto coordinate = ScriptRiskOutputAxis(prepared.Product())[slot];
        coordinate.id_ = "trade:" + String_(std::to_string(trade)) + ":" + coordinate.id_;
        return {trade, std::move(coordinate), weight};
    }

    Dal::Script::Detail::PortfolioBatchOutput_ Payoff(const Dal::Script::Detail::PreparedPortfolio_& portfolio, size_t trade, double weight) {
        return Output(portfolio, trade, portfolio.Trades()[trade].PayOffIdx(), weight);
    }

    Dal::Script::Detail::PortfolioBatchOutput_
    NamedOutput(const Dal::Script::Detail::PreparedPortfolio_& portfolio, size_t trade, const String_& name, double weight) {
        const auto& names = portfolio.Trades()[trade].Product().VarNames();
        const auto found = std::find(names.begin(), names.end(), name);
        REQUIRE(found != names.end(), "test requires an indexed scalar output");
        return Output(portfolio, trade, static_cast<size_t>(found - names.begin()), weight);
    }

    SimResults_ IndependentBatch(const Dal::Script::Detail::PreparedPortfolio_& portfolio, size_t trade, const PathBatch_& batch) {
        const auto& product = portfolio.Trades()[trade];
        const auto& data = portfolio.Portfolio()->Models()[portfolio.Portfolio()->ModelOwners()[trade]];
        const auto model = CreateModel<double>(data);
        SimResults_ result(Vector::Join(model->ParameterLabels(), product.ConstVarNames()));
        const auto& simulation = product.Simulation();
        const Dal::Script::Detail::AADBatchSettings_ settings{simulation.rsg_,
                                                              simulation.useBb_,
                                                              static_cast<int>(product.MaxNestedIfs()),
                                                              simulation.smooth_,
                                                              static_cast<size_t>(portfolio.PathCount()),
                                                              model->NumParams(),
                                                              product.ConstVarNames().size(),
                                                              product.PayOffIdx()};
        const auto program = simulation.compiled_.value_or(false) ? std::optional<ScriptCompiled_>(product.Compile(true)) : std::nullopt;
        Dal::Script::Detail::EvaluateAADBatch(product, data, settings, program, batch, &result);
        return result;
    }

    template <class F_> void AssertFailure(const F_& action, const String_& context) {
        try {
            action();
            FAIL() << "expected " << context;
        } catch (const ScriptError_& error) {
            ASSERT_NE(String_(error.what()).find(context), String_::npos) << error.what();
        }
    }
} // namespace

TEST(PortfolioBatchTest, TestSharedModelAndPrivateConstantsMatchOwnershipOracle) {
    const Handle_<ModelData_> model(new BSModelData_("", 100.0, 0.0));
    const Handle_<ScriptPortfolioData_> data(
        new ScriptPortfolioData_("", {{"A", Trade(5.0, "pay PAYS 2 * SPOT() + X"), model}, {"B", Trade(7.0, "pay PAYS 3 * SPOT() + X"), model}}));
    for (const bool compiled : {false, true}) {
        MonteCarloSettings_ simulation;
        simulation.enableAad_ = true;
        simulation.compiled_ = compiled;
        const auto portfolio = Dal::Script::Detail::PrepareScriptPortfolio(data, 17, Valuation(), simulation);
        ASSERT_EQ(portfolio.Groups().size(), 1);
        const auto result =
            Dal::Script::Detail::EvaluatePortfolioWeightedBatch(portfolio, 0, {0, 17}, {Payoff(portfolio, 0, 2.0), Payoff(portfolio, 1, -1.0)});
        const auto a = IndependentBatch(portfolio, 0, {0, 17});
        const auto b = IndependentBatch(portfolio, 1, {0, 17});
        ASSERT_DOUBLE_EQ(result.weightedSum_ / 17, 103.0);
        ASSERT_EQ(result.componentSums_, Vector_<>({a.aggregated_, b.aggregated_}));
        ASSERT_DOUBLE_EQ(result.componentSums_[0] / 17, 205.0);
        ASSERT_DOUBLE_EQ(result.componentSums_[1] / 17, 307.0);
        ASSERT_EQ(result.modelGradientSums_.size(), 4);
        ASSERT_DOUBLE_EQ(result.modelGradientSums_[0] / 17, 1.0);
        ASSERT_EQ(result.constantGradientSums_.size(), 2);
        ASSERT_EQ(result.constantGradientSums_[0], Vector_<>({34.0}));
        ASSERT_EQ(result.constantGradientSums_[1], Vector_<>({-17.0}));
        ASSERT_EQ(result.generatedScenarios_, 17);
        ASSERT_EQ(result.evaluatorCalls_, 34);
        ASSERT_EQ(result.suffixReversals_, 17);
        ASSERT_EQ(result.prefixReversals_, 1);
    }
}

TEST(PortfolioBatchTest, TestBlockedRootsPreservePrivateAliasesSharedInputsAndZeroTailLanes) {
    const Handle_<ModelData_> model(new BSModelData_("", 100.0, 0.0));
    const Handle_<ScriptPortfolioData_> data(new ScriptPortfolioData_(
        "", {{"A", Trade(5.0, "a = X pay PAYS 2 * SPOT() + X"), model}, {"B", Trade(7.0, "a = X pay PAYS 3 * SPOT() + X"), model}}));
    for (const bool compiled : {false, true}) {
        auto simulation = MonteCarloSettings_();
        simulation.enableAad_ = true;
        simulation.compiled_ = compiled;
        const auto portfolio = Dal::Script::Detail::PrepareScriptPortfolio(data, 17, Valuation(), simulation);
        const Vector_<Dal::Script::Detail::PortfolioBatchOutput_> outputs{Payoff(portfolio, 1, 1.0), NamedOutput(portfolio, 0, "a", 1.0),
                                                                          Payoff(portfolio, 0, 1.0)};
        for (const size_t width : {3, 4}) {
            const auto result = Dal::Script::Detail::EvaluatePortfolioJacobianBatch(portfolio, 0, {0, 17}, outputs, width);
            const auto first = IndependentBatch(portfolio, 0, {0, 17});
            const auto second = IndependentBatch(portfolio, 1, {0, 17});
            ASSERT_EQ(result.componentSums_, Vector_<double>({second.aggregated_, 5.0 * 17, first.aggregated_}));
            ASSERT_DOUBLE_EQ(result.componentSums_[0] / 17, 307.0);
            ASSERT_DOUBLE_EQ(result.componentSums_[2] / 17, 205.0);
            ASSERT_EQ(result.tradePositions_, Vector_<size_t>({0, 1}));
            ASSERT_EQ(result.modelGradientSums_.Rows(), static_cast<int>(width));
            ASSERT_EQ(result.modelGradientSums_.Cols(), 4);
            ASSERT_EQ(result.constantGradientSums_.size(), 2);
            ASSERT_DOUBLE_EQ(result.modelGradientSums_(0, 0), 3.0 * 17);
            ASSERT_DOUBLE_EQ(result.modelGradientSums_(1, 0), 0.0);
            ASSERT_DOUBLE_EQ(result.modelGradientSums_(2, 0), 2.0 * 17);
            ASSERT_DOUBLE_EQ(result.constantGradientSums_[0](0, 0), 0.0);
            ASSERT_DOUBLE_EQ(result.constantGradientSums_[0](1, 0), 17.0);
            ASSERT_DOUBLE_EQ(result.constantGradientSums_[0](2, 0), 17.0);
            ASSERT_DOUBLE_EQ(result.constantGradientSums_[1](0, 0), 17.0);
            ASSERT_DOUBLE_EQ(result.constantGradientSums_[1](1, 0), 0.0);
            ASSERT_DOUBLE_EQ(result.constantGradientSums_[1](2, 0), 0.0);
            if (width == 4) {
                for (int column = 0; column < 4; ++column)
                    ASSERT_DOUBLE_EQ(result.modelGradientSums_(3, column), 0.0);
                ASSERT_DOUBLE_EQ(result.constantGradientSums_[0](3, 0), 0.0);
                ASSERT_DOUBLE_EQ(result.constantGradientSums_[1](3, 0), 0.0);
            }
            ASSERT_EQ(result.generatedScenarios_, 17);
            ASSERT_EQ(result.evaluatorCalls_, 34);
            ASSERT_EQ(result.suffixReversals_, 17);
            ASSERT_EQ(result.prefixReversals_, 1);
        }
    }
}

TEST(PortfolioBatchTest, TestBlockedAbsolutePathsMatchEveryIndependentModelAndPrivateRisk) {
    const Handle_<ModelData_> model(new BSModelData_("", 100.0, 0.23, 0.02, 0.01));
    const Handle_<ScriptPortfolioData_> data(new ScriptPortfolioData_(
        "", {{"A", Trade(5.0, "pay PAYS MAX(SPOT() - 20 * X, 0)"), model}, {"B", Trade(7.0, "pay PAYS SPOT() * X"), model}}));
    for (const bool compiled : {false, true})
        for (const String_& rsg : Vector_<String_>{"sobol", "mrg32"})
            for (const bool bridge : {false, true}) {
                auto simulation = MonteCarloSettings_();
                simulation.enableAad_ = true;
                simulation.compiled_ = compiled;
                simulation.rsg_ = rsg;
                simulation.useBb_ = bridge;
                const auto portfolio = Dal::Script::Detail::PrepareScriptPortfolio(data, 257, Valuation(), simulation);
                const auto first = IndependentBatch(portfolio, 0, {13, 37});
                const auto second = IndependentBatch(portfolio, 1, {13, 37});
                const auto result = Dal::Script::Detail::EvaluatePortfolioJacobianBatch(portfolio, 0, {13, 37},
                                                                                        {Payoff(portfolio, 0, 1.0), Payoff(portfolio, 1, 1.0)}, 3);
                ASSERT_EQ(result.componentSums_, Vector_<double>({first.aggregated_, second.aggregated_}));
                for (int column = 0; column < 4; ++column) {
                    ASSERT_NEAR(result.modelGradientSums_(0, column) / 257, first.risks_[column], 1e-10);
                    ASSERT_NEAR(result.modelGradientSums_(1, column) / 257, second.risks_[column], 1e-10);
                    ASSERT_DOUBLE_EQ(result.modelGradientSums_(2, column), 0.0);
                }
                ASSERT_NEAR(result.constantGradientSums_[0](0, 0) / 257, first.risks_[4], 1e-10);
                ASSERT_DOUBLE_EQ(result.constantGradientSums_[0](1, 0), 0.0);
                ASSERT_NEAR(result.constantGradientSums_[1](1, 0) / 257, second.risks_[4], 1e-10);
                ASSERT_DOUBLE_EQ(result.constantGradientSums_[1](0, 0), 0.0);
                ASSERT_EQ(result.generatedScenarios_, 37);
                ASSERT_EQ(result.evaluatorCalls_, 74);
            }
}

TEST(PortfolioBatchTest, TestBlockedFailureAndCapacitiesRestoreModesAndPriorResults) {
    const Handle_<ModelData_> model(new BSModelData_("", 1000.0, 0.0));
    const Handle_<ScriptPortfolioData_> data(
        new ScriptPortfolioData_("", {{"Good", Trade(5.0, "pay PAYS SPOT() + X"), model}, {"Bad", Trade(7.0, "pay PAYS EXP(SPOT())"), model}}));
    for (const bool compiled : {false, true}) {
        auto simulation = MonteCarloSettings_();
        simulation.enableAad_ = true;
        simulation.compiled_ = compiled;
        const auto portfolio = Dal::Script::Detail::PrepareScriptPortfolio(data, 17, Valuation(), simulation);
        auto mode = AAD::SetNumResultsForAAD(true, 3);
        const auto good = Payoff(portfolio, 0, 1.0);
        const auto bad = Payoff(portfolio, 1, 0.0);
        const auto prior = Dal::Script::Detail::EvaluatePortfolioJacobianBatch(portfolio, 0, {0, 17}, {good}, 1);
        for (const size_t width : {size_t{0}, AAD::ADJ_SIZE + 1})
            AssertFailure([&] { static_cast<void>(Dal::Script::Detail::EvaluatePortfolioJacobianBatch(portfolio, 0, {0, 17}, {good}, width)); },
                          "field=width");
        AssertFailure([&] { static_cast<void>(Dal::Script::Detail::EvaluatePortfolioJacobianBatch(portfolio, 0, {0, 17}, {good, bad}, 1)); },
                      "field=width");
        AssertFailure([&] { static_cast<void>(Dal::Script::Detail::EvaluatePortfolioJacobianBatch(portfolio, 0, {0, 17}, {good, bad}, 2)); },
                      "trade=Bad");
        AAD::TapeCapacityBudget_ tapeZero(0);
        AssertFailure(
            [&] { static_cast<void>(Dal::Script::Detail::EvaluatePortfolioJacobianBatch(portfolio, 0, {0, 17}, {good}, 2, nullptr, &tapeZero)); },
            "Tape capacity budget exceeded");
        BufferCapacityBudget_ scratchZero(0);
        AssertFailure([&] { static_cast<void>(Dal::Script::Detail::EvaluatePortfolioJacobianBatch(portfolio, 0, {0, 17}, {good}, 2, &scratchZero)); },
                      "Scratch buffer capacity budget exceeded");
        ASSERT_TRUE(AAD::Tape()->multi_);
        ASSERT_EQ(AAD::Tape()->numAdj_, 3);
        BufferCapacityBudget_ scratch(64 * 1024 * 1024);
        AAD::TapeCapacityBudget_ tape(256 * 1024 * 1024);
        const auto recovered = Dal::Script::Detail::EvaluatePortfolioJacobianBatch(portfolio, 0, {0, 17}, {good}, 2, &scratch, &tape);
        ASSERT_EQ(recovered.componentSums_, prior.componentSums_);
        ASSERT_DOUBLE_EQ(recovered.modelGradientSums_(0, 0), prior.modelGradientSums_(0, 0));
        ASSERT_DOUBLE_EQ(recovered.constantGradientSums_[0](0, 0), 17.0);
        ASSERT_DOUBLE_EQ(recovered.modelGradientSums_(1, 0), 0.0);
        ASSERT_LE(scratch.PeakCapacityBytes(), scratch.LimitBytes());
        ASSERT_LE(tape.PeakCapacityBytes(), tape.LimitBytes());
        ASSERT_TRUE(AAD::Tape()->multi_);
        ASSERT_EQ(AAD::Tape()->numAdj_, 3);
    }
}

TEST(PortfolioBatchTest, TestZeroWeightNonfiniteOutputNamesTradeAndRecordingRecovers) {
    const Handle_<ModelData_> model(new BSModelData_("", 1000.0, 0.0));
    const Handle_<ScriptPortfolioData_> data(new ScriptPortfolioData_(
        "", {{"valid", Trade(5.0, "pay PAYS SPOT() + X"), model}, {"exploding", Trade(7.0, "pay PAYS EXP(SPOT()) + X"), model}}));
    for (const bool compiled : {false, true}) {
        MonteCarloSettings_ simulation;
        simulation.enableAad_ = true;
        simulation.compiled_ = compiled;
        const auto portfolio = Dal::Script::Detail::PrepareScriptPortfolio(data, 17, Valuation(), simulation);
        ASSERT_EQ(portfolio.Groups().size(), 1);
        const auto prior = Dal::Script::Detail::EvaluatePortfolioWeightedBatch(portfolio, 0, {0, 17}, {Payoff(portfolio, 0, 1.0)});
        AssertFailure(
            [&] {
                static_cast<void>(Dal::Script::Detail::EvaluatePortfolioWeightedBatch(portfolio, 0, {0, 17},
                                                                                      {Payoff(portfolio, 0, 1.0), Payoff(portfolio, 1, 0.0)}));
            },
            "trade=exploding");
        const auto recovered = Dal::Script::Detail::EvaluatePortfolioWeightedBatch(portfolio, 0, {0, 17}, {Payoff(portfolio, 0, 1.0)});
        ASSERT_EQ(prior.componentSums_, recovered.componentSums_);
        ASSERT_EQ(prior.modelGradientSums_, recovered.modelGradientSums_);
        ASSERT_EQ(prior.constantGradientSums_, recovered.constantGradientSums_);
        ASSERT_EQ(recovered.tradePositions_, Vector_<size_t>({0}));
        ASSERT_EQ(recovered.generatedScenarios_, 17);
        ASSERT_EQ(recovered.evaluatorCalls_, 17);
    }
}

TEST(PortfolioBatchTest, TestAbsolutePathsAndPrivateVectorHistoryMatchIndependentBatches) {
    const Handle_<ModelData_> model(new BSModelData_("", 100.0, 0.23, 0.02, 0.01));
    const Vector_<Cell_> dates{Cell_("X"), Cell_(Date_(2025, 12, 30)), Cell_(Date_(2026, 6, 7)), Cell_(Date_(2027, 1, 1))};
    const Vector_<String_> events{"5", "APPEND(v, X * FIX(EQ[PORTFOLIO_BATCH_PAST]))", "APPEND(v, SPOT())",
                                  "APPEND(v, SPOT()) pay PAYS SUM(v) + MAX(SPOT() - 20 * X, 0)"};
    const Handle_<ScriptProductData_> a(new ScriptProductData_("", dates, events, ScriptProductSettings_{"EQ[MODEL]", {}}));
    auto other = events;
    other[0] = "7";
    const Handle_<ScriptProductData_> b(new ScriptProductData_("", dates, other, ScriptProductSettings_{"EQ[MODEL]", {}}));
    const Handle_<ScriptPortfolioData_> data(new ScriptPortfolioData_("", {{"A", a, model}, {"B", b, model}}));
    auto valuation = Valuation();
    valuation.fixings_ =
        Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_({{"EQ[PORTFOLIO_BATCH_PAST]", {{DateTime_(Date_(2025, 12, 30), 0.0), 80.0}}}}));
    const PathBatch_ batch{7, 257};
    for (const bool compiled : {false, true})
        for (const String_& rsg : Vector_<String_>{"sobol", "mrg32"})
            for (const bool bridge : {false, true}) {
                SCOPED_TRACE(rsg + (compiled ? "/compiled" : "/tree") + (bridge ? "/bridge" : "/ordinary"));
                MonteCarloSettings_ simulation;
                simulation.enableAad_ = true;
                simulation.compiled_ = compiled;
                simulation.rsg_ = rsg;
                simulation.useBb_ = bridge;
                const auto portfolio = Dal::Script::Detail::PrepareScriptPortfolio(data, 311, valuation, simulation);
                ASSERT_EQ(portfolio.Groups().size(), 1);
                ASSERT_EQ(portfolio.Trades()[0].TimeLine().size(), 2);
                const auto result =
                    Dal::Script::Detail::EvaluatePortfolioWeightedBatch(portfolio, 0, batch, {Payoff(portfolio, 0, 3.0), Payoff(portfolio, 1, -2.0)});
                const auto first = IndependentBatch(portfolio, 0, batch);
                const auto second = IndependentBatch(portfolio, 1, batch);
                ASSERT_EQ(result.componentSums_, Vector_<>({first.aggregated_, second.aggregated_}));
                ASSERT_NEAR(result.weightedSum_ / 257, (3 * first.aggregated_ - 2 * second.aggregated_) / 257, 1e-10);
                ASSERT_EQ(result.modelGradientSums_.size(), 4);
                for (size_t input = 0; input < 4; ++input)
                    ASSERT_NEAR(result.modelGradientSums_[input] / 257, (3 * first.risks_[input] - 2 * second.risks_[input]) * 311 / 257, 1e-10);
                ASSERT_NEAR(result.constantGradientSums_[0][0] / 257, 3 * first.risks_[4] * 311 / 257, 1e-10);
                ASSERT_NEAR(result.constantGradientSums_[1][0] / 257, -2 * second.risks_[4] * 311 / 257, 1e-10);
                ASSERT_EQ(result.generatedScenarios_, 257);
                ASSERT_EQ(result.evaluatorCalls_, 514);
                const BatchPlan_ batches(32785, 4);
                const auto reusable = Dal::Script::Detail::PrepareScriptPortfolio(data, 32785, valuation, simulation);
                const Vector_<Dal::Script::Detail::PortfolioBatchOutput_> outputs{Payoff(reusable, 1, -2.0), Payoff(reusable, 0, 3.0)};
                Vector_<Dal::Script::Detail::PortfolioWeightedBatchResult_> slots;
                for (size_t index = 0; index < batches.BatchCount(); ++index)
                    slots.emplace_back(outputs.size());
                Dal::Script::Detail::EvaluatePortfolioWeightedWorker(reusable, 0, batches, 0, 4, outputs, &slots);
                for (const size_t index : {0, 4}) {
                    const auto fresh = Dal::Script::Detail::EvaluatePortfolioWeightedBatch(reusable, 0, batches.BatchAt(index), outputs);
                    ASSERT_EQ(slots[index].componentSums_, fresh.componentSums_);
                    ASSERT_DOUBLE_EQ(slots[index].weightedSum_, fresh.weightedSum_);
                    ASSERT_EQ(slots[index].modelGradientSums_, fresh.modelGradientSums_);
                    ASSERT_EQ(slots[index].constantGradientSums_, fresh.constantGradientSums_);
                    ASSERT_EQ(slots[index].generatedScenarios_, batches.BatchAt(index).pathCount_);
                    ASSERT_EQ(slots[index].prefixReversals_, 1);
                }
                ASSERT_EQ(slots[1].generatedScenarios_, 0);
            }
}

TEST(PortfolioBatchTest, TestWorkerRangeAndSlotValidationPrecedesRecording) {
    const Handle_<ModelData_> model(new BSModelData_("", 100.0, 0.2));
    const Handle_<ScriptPortfolioData_> data(new ScriptPortfolioData_("", {{"A", Trade(5.0, "pay PAYS SPOT() + X"), model}}));
    MonteCarloSettings_ simulation;
    simulation.enableAad_ = true;
    const auto portfolio = Dal::Script::Detail::PrepareScriptPortfolio(data, 17, Valuation(), simulation);
    const BatchPlan_ batches(17, 1);
    const Vector_<Dal::Script::Detail::PortfolioBatchOutput_> outputs{Payoff(portfolio, 0, 1.0)};
    Vector_<Dal::Script::Detail::PortfolioWeightedBatchResult_> slots;
    slots.emplace_back(1);
    AAD::RecordingScope_ recording;
    for (const size_t workers : {0, 2})
        AssertFailure([&] { Dal::Script::Detail::EvaluatePortfolioWeightedWorker(portfolio, 0, batches, 0, workers, outputs, &slots); },
                      "field=workers");
    AssertFailure([&] { Dal::Script::Detail::EvaluatePortfolioWeightedWorker(portfolio, 0, batches, 1, 1, outputs, &slots); }, "field=workers");
    AssertFailure([&] { Dal::Script::Detail::EvaluatePortfolioWeightedWorker(portfolio, 0, batches, 0, 1, outputs, nullptr); }, "field=slots");
    const BatchPlan_ outside(8193, 1);
    slots.emplace_back(1);
    AssertFailure([&] { Dal::Script::Detail::EvaluatePortfolioWeightedWorker(portfolio, 0, outside, 0, 1, outputs, &slots); }, "field=batch");
    ASSERT_EQ(slots[0].generatedScenarios_, 0);
}

TEST(PortfolioBatchTest, TestHistoricalPrefixAndDirectConstantAliasesReverseOnce) {
    const Handle_<ModelData_> model(new BSModelData_("", 100.0, 0.0));
    const Vector_<Cell_> dates{Cell_("X"), Cell_(Date_(2025, 12, 30)), Cell_(Date_(2027, 1, 1))};
    const Vector_<String_> events{"5", "state = X * FIX(EQ[PORTFOLIO_BATCH_PAST])",
                                  "alias = X alias2 = alias prefix = state pay PAYS state + SPOT()"};
    const Handle_<ScriptProductData_> a(new ScriptProductData_("", dates, events, ScriptProductSettings_{"EQ[MODEL]", {}}));
    auto other = events;
    other[0] = "7";
    const Handle_<ScriptProductData_> b(new ScriptProductData_("", dates, other, ScriptProductSettings_{"EQ[MODEL]", {}}));
    const Handle_<ScriptPortfolioData_> data(new ScriptPortfolioData_("", {{"A", a, model}, {"B", b, model}}));
    auto valuation = Valuation();
    valuation.fixings_ =
        Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_({{"EQ[PORTFOLIO_BATCH_PAST]", {{DateTime_(Date_(2025, 12, 30), 0.0), 80.0}}}}));
    for (const bool compiled : {false, true}) {
        MonteCarloSettings_ simulation;
        simulation.enableAad_ = true;
        simulation.compiled_ = compiled;
        const auto portfolio = Dal::Script::Detail::PrepareScriptPortfolio(data, 19, valuation, simulation);
        ASSERT_EQ(portfolio.Groups().size(), 1);
        const auto result =
            Dal::Script::Detail::EvaluatePortfolioWeightedBatch(portfolio, 0, {0, 19},
                                                                {Payoff(portfolio, 1, -1.0), NamedOutput(portfolio, 0, "alias2", 1.0),
                                                                 NamedOutput(portfolio, 0, "prefix", 1.0), NamedOutput(portfolio, 0, "alias", 1.0)});
        ASSERT_DOUBLE_EQ(result.weightedSum_ / 19, -250.0);
        ASSERT_DOUBLE_EQ(result.componentSums_[0] / 19, 660.0);
        ASSERT_DOUBLE_EQ(result.componentSums_[1] / 19, 5.0);
        ASSERT_DOUBLE_EQ(result.componentSums_[2] / 19, 400.0);
        ASSERT_DOUBLE_EQ(result.componentSums_[3] / 19, 5.0);
        ASSERT_DOUBLE_EQ(result.modelGradientSums_[0] / 19, -1.0);
        ASSERT_EQ(result.constantGradientSums_[0], Vector_<>({19 * 82.0}));
        ASSERT_EQ(result.constantGradientSums_[1], Vector_<>({19 * -80.0}));
        ASSERT_EQ(result.suffixReversals_, 19);
        ASSERT_EQ(result.prefixReversals_, 1);
        const auto block =
            Dal::Script::Detail::EvaluatePortfolioJacobianBatch(portfolio, 0, {0, 19},
                                                                {Payoff(portfolio, 1, -1.0), NamedOutput(portfolio, 0, "alias2", 1.0),
                                                                 NamedOutput(portfolio, 0, "prefix", 1.0), NamedOutput(portfolio, 0, "alias", 1.0)},
                                                                4);
        ASSERT_EQ(block.componentSums_, result.componentSums_);
        ASSERT_DOUBLE_EQ(block.modelGradientSums_(0, 0) / 19, 1.0);
        ASSERT_DOUBLE_EQ(block.modelGradientSums_(1, 0), 0.0);
        ASSERT_DOUBLE_EQ(block.modelGradientSums_(2, 0), 0.0);
        ASSERT_DOUBLE_EQ(block.modelGradientSums_(3, 0), 0.0);
        ASSERT_DOUBLE_EQ(block.constantGradientSums_[0](0, 0), 0.0);
        ASSERT_DOUBLE_EQ(block.constantGradientSums_[0](1, 0), 19.0);
        ASSERT_DOUBLE_EQ(block.constantGradientSums_[0](2, 0), 19 * 80.0);
        ASSERT_DOUBLE_EQ(block.constantGradientSums_[0](3, 0), 19.0);
        ASSERT_DOUBLE_EQ(block.constantGradientSums_[1](0, 0), 19 * 80.0);
        ASSERT_EQ(block.suffixReversals_, 19);
        ASSERT_EQ(block.prefixReversals_, 1);
    }
}

TEST(PortfolioBatchTest, TestOutputOrderAndUnselectedTradeKeepIndependentState) {
    const Handle_<ModelData_> model(new BSModelData_("", 1000.0, 0.0));
    const Handle_<ScriptPortfolioData_> data(new ScriptPortfolioData_("", {{"A", Trade(5.0, "pay PAYS SPOT() + X"), model},
                                                                           {"unselected", Trade(7.0, "pay PAYS EXP(SPOT()) + X"), model},
                                                                           {"C", Trade(11.0, "pay PAYS 3 * SPOT() + X"), model}}));
    for (const bool compiled : {false, true}) {
        MonteCarloSettings_ simulation;
        simulation.enableAad_ = true;
        simulation.compiled_ = compiled;
        const auto portfolio = Dal::Script::Detail::PrepareScriptPortfolio(data, 31, Valuation(), simulation);
        ASSERT_EQ(portfolio.Groups().size(), 1);
        const auto result =
            Dal::Script::Detail::EvaluatePortfolioWeightedBatch(portfolio, 0, {0, 31}, {Payoff(portfolio, 2, -1.0), Payoff(portfolio, 0, 2.0)});
        ASSERT_DOUBLE_EQ(result.weightedSum_ / 31, -1001.0);
        ASSERT_DOUBLE_EQ(result.componentSums_[0] / 31, 3011.0);
        ASSERT_DOUBLE_EQ(result.componentSums_[1] / 31, 1005.0);
        ASSERT_EQ(result.tradePositions_, Vector_<size_t>({0, 2}));
        ASSERT_EQ(result.constantGradientSums_[0], Vector_<>({62.0}));
        ASSERT_EQ(result.constantGradientSums_[1], Vector_<>({-31.0}));
        ASSERT_DOUBLE_EQ(result.modelGradientSums_[0] / 31, -1.0);
        ASSERT_EQ(result.generatedScenarios_, 31);
        ASSERT_EQ(result.evaluatorCalls_, 62);
    }
}

TEST(PortfolioBatchTest, TestMalformedOutputsAndRangesRejectBeforeRecording) {
    const Handle_<ModelData_> model(new BSModelData_("", 100.0, 0.2));
    const Handle_<ModelData_> other(new BSModelData_("", 100.0, 0.2));
    const auto trade = Trade(5.0, "pay PAYS SPOT() + X");
    const Handle_<ScriptPortfolioData_> data(new ScriptPortfolioData_("", {{"A", trade, model}, {"B", trade, other}}));
    MonteCarloSettings_ simulation;
    simulation.enableAad_ = true;
    const auto portfolio = Dal::Script::Detail::PrepareScriptPortfolio(data, 13, Valuation(), simulation);
    const auto output = Payoff(portfolio, 0, 1.0);
    auto badSlot = output;
    badSlot.coordinate_.slot_ = std::numeric_limits<size_t>::max();
    auto badLabel = output;
    badLabel.coordinate_.label_ = "changed";
    auto badWeight = output;
    badWeight.weight_ = std::numeric_limits<double>::infinity();
    {
        AAD::RecordingScope_ recording;
        for (const auto& outputs : Vector_<Vector_<Dal::Script::Detail::PortfolioBatchOutput_>>{
                 {}, {output, output}, {badSlot}, {badLabel}, {badWeight}, {Payoff(portfolio, 1, 1.0)}})
            AssertFailure([&] { static_cast<void>(Dal::Script::Detail::EvaluatePortfolioWeightedBatch(portfolio, 0, {0, 13}, outputs)); },
                          "InvalidPortfolioBatch:");
        for (const PathBatch_ batch : {PathBatch_{0, 0}, PathBatch_{13, 1}, PathBatch_{1, 13}, PathBatch_{std::numeric_limits<size_t>::max(), 1}})
            AssertFailure([&] { static_cast<void>(Dal::Script::Detail::EvaluatePortfolioWeightedBatch(portfolio, 0, batch, {output})); },
                          "field=batch");
        AssertFailure(
            [&] {
                static_cast<void>(
                    Dal::Script::Detail::EvaluatePortfolioWeightedBatch(portfolio, std::numeric_limits<size_t>::max(), {0, 13}, {output}));
            },
            "field=group");
    }
    ASSERT_NO_THROW(static_cast<void>(Dal::Script::Detail::EvaluatePortfolioWeightedBatch(portfolio, 0, {0, 13}, {output})));
}

TEST(PortfolioBatchTest, TestWeightedOverflowRetainsGroupContextAndRecovers) {
    const Handle_<ModelData_> model(new BSModelData_("", 709.0, 0.0));
    const Handle_<ScriptPortfolioData_> data(new ScriptPortfolioData_("", {{"A", Trade(5.0, "pay PAYS EXP(SPOT())"), model}}));
    MonteCarloSettings_ simulation;
    simulation.enableAad_ = true;
    const auto portfolio = Dal::Script::Detail::PrepareScriptPortfolio(data, 1, Valuation(), simulation);
    AssertFailure([&] { static_cast<void>(Dal::Script::Detail::EvaluatePortfolioWeightedBatch(portfolio, 0, {0, 1}, {Payoff(portfolio, 0, 3.0)})); },
                  "PortfolioWeightedBatchFailed: group=0");
    const Handle_<ModelData_> validModel(new BSModelData_("", 100.0, 0.2));
    const Handle_<ScriptPortfolioData_> validData(new ScriptPortfolioData_("", {{"valid", Trade(5.0, "pay PAYS SPOT() + X"), validModel}}));
    const auto fresh = Dal::Script::Detail::PrepareScriptPortfolio(validData, 1, Valuation(), simulation);
    const auto valid = Dal::Script::Detail::EvaluatePortfolioWeightedBatch(fresh, 0, {0, 1}, {Payoff(fresh, 0, 1.0)});
    ASSERT_TRUE(std::isfinite(valid.weightedSum_));
    for (const auto gradient : valid.modelGradientSums_)
        ASSERT_TRUE(std::isfinite(gradient));
    ASSERT_EQ(valid.prefixReversals_, 1);
}

TEST(PortfolioBatchTest, TestZeroWeightsAndNativeModeRestore) {
    const Handle_<ModelData_> model(new BSModelData_("", 100.0, 0.2));
    const Handle_<ScriptPortfolioData_> data(
        new ScriptPortfolioData_("", {{"A", Trade(5.0, "pay PAYS X"), model}, {"B", Trade(7.0, "pay PAYS 2 * X"), model}}));
    for (const bool compiled : {false, true}) {
        MonteCarloSettings_ simulation;
        simulation.enableAad_ = true;
        simulation.compiled_ = compiled;
        const auto portfolio = Dal::Script::Detail::PrepareScriptPortfolio(data, 23, Valuation(), simulation);
        auto mode = AAD::SetNumResultsForAAD(true, 3);
        const auto result =
            Dal::Script::Detail::EvaluatePortfolioWeightedBatch(portfolio, 0, {0, 23}, {Payoff(portfolio, 0, 0.0), Payoff(portfolio, 1, 0.0)});
        ASSERT_TRUE(AAD::Tape()->multi_);
        ASSERT_EQ(AAD::Tape()->numAdj_, 3);
        ASSERT_DOUBLE_EQ(result.weightedSum_, 0.0);
        ASSERT_EQ(result.componentSums_, Vector_<>({23 * 5.0, 23 * 14.0}));
        ASSERT_EQ(result.modelGradientSums_, Vector_<>(4, 0.0));
        ASSERT_EQ(result.constantGradientSums_, Vector_<Vector_<double>>({{0.0}, {0.0}}));
        ASSERT_EQ(result.suffixReversals_, 23);
        ASSERT_EQ(result.prefixReversals_, 1);
    }
}

TEST(PortfolioBatchTest, TestPrivateEvaluationFailureNamesTradeAndLegacyRecovers) {
    const Handle_<ModelData_> model(new BSModelData_("", 100.0, 0.2));
    const Handle_<ScriptPortfolioData_> data(new ScriptPortfolioData_(
        "", {{"valid", Trade(5.0, "pay PAYS SPOT() + X"), model}, {"bad-vector", Trade(7.0, "APPEND(v, SPOT()) pay PAYS v[3] + X"), model}}));
    for (const bool compiled : {false, true}) {
        MonteCarloSettings_ simulation;
        simulation.enableAad_ = true;
        simulation.compiled_ = compiled;
        const auto portfolio = Dal::Script::Detail::PrepareScriptPortfolio(data, 13, Valuation(), simulation);
        ASSERT_EQ(portfolio.Groups().size(), 1);
        const auto prior = Dal::Script::Detail::EvaluatePortfolioWeightedBatch(portfolio, 0, {0, 13}, {Payoff(portfolio, 0, 1.0)});
        AssertFailure(
            [&] {
                static_cast<void>(Dal::Script::Detail::EvaluatePortfolioWeightedBatch(portfolio, 0, {0, 13},
                                                                                      {Payoff(portfolio, 0, 1.0), Payoff(portfolio, 1, 1.0)}));
            },
            "PortfolioTradeEvaluationFailed: trade=bad-vector");
        const auto legacy = IndependentBatch(portfolio, 0, {0, 13});
        ASSERT_EQ(prior.componentSums_[0], legacy.aggregated_);
        ASSERT_NEAR(prior.modelGradientSums_[0] / 13, legacy.risks_[0], 1e-10);
    }
}

//
// Created by Codex on 2026/10/6.
//

#include <gtest/gtest.h>

#include <dal/model/blackscholes.hpp>
#include <dal/script/portfoliopreparation.hpp>

#include "script_test_observers.hpp"

using namespace Dal;
using namespace Dal::Script;
using namespace Dal::Script::TestSupport;

namespace {
    Handle_<ScriptProductData_> HistoricalTrade(const String_& index) {
        return Handle_<ScriptProductData_>(new ScriptProductData_("", {Cell_(Date_(2026, 1, 1)), Cell_(Date_(2027, 1, 1))},
                                                                  {"state = FIX(" + index + ")", "pay PAYS state + FIX(EQ[MODEL])"}));
    }

    ScriptValuationSettings_ Valuation() {
        ScriptValuationSettings_ result;
        result.evaluationDate_ = Date_(2026, 1, 2);
        return result;
    }

    template <class F_> void AssertPortfolioFailure(const F_& action, const String_& expected, const String_& source = {}) {
        try {
            action();
            FAIL() << "expected " << expected;
        } catch (const ScriptError_& error) {
            ASSERT_NE(String_(error.what()).find(expected), String_::npos) << error.what();
            if (!source.empty())
                ASSERT_NE(String_(error.what()).find(source), String_::npos) << error.what();
        }
    }

    struct ChangeGlobalsAfterFreeze_ : NamedFixingReadCounter_ {
        void BeforeFixing(const Index_&, const Environment_*, const DateTime_&) override {
            if (fixings_++ == 0) {
                StoreScriptTestFixing("EQ[PORTFOLIO_A]", 90.0, DateTime_(Date_(2026, 1, 1), 0.0));
                StoreScriptTestFixing("EQ[PORTFOLIO_B]", 30.0, DateTime_(Date_(2026, 1, 1), 0.0));
                XGLOBAL::SetEvaluationDate(Date_(2030, 1, 1));
            }
        }
    };

    struct RejectCompilation_ : SubmissionCounter_ {
        void BeforeCompilation() override { THROW2("injected compilation failure", ScriptError_); }
    };
} // namespace

TEST(PortfolioPreparationTest, TestAllPlansAdmitBeforeOneSharedHistorySnapshot) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 1, 2));
    StoreScriptTestFixing("EQ[PORTFOLIO_PREPARED]", 80.0, DateTime_(Date_(2026, 1, 1), 0.0));
    const Handle_<ModelData_> model(new BSModelData_("", 100.0, 0.2));
    const Vector_<Cell_> dates{Cell_("X"), Cell_(Date_(2026, 1, 1)), Cell_(Date_(2027, 1, 1))};
    const Handle_<ScriptProductData_> a(
        new ScriptProductData_("", dates, {"5", "state = X * FIX(EQ[PORTFOLIO_PREPARED])", "pay PAYS state + FIX(EQ[MODEL])"}));
    const Handle_<ScriptProductData_> b(
        new ScriptProductData_("", dates, {"7", "state = X * FIX(EQ[PORTFOLIO_PREPARED])", "pay PAYS state + FIX(EQ[MODEL])"}));
    const Handle_<ScriptPortfolioData_> portfolio(new ScriptPortfolioData_("", {{"A", a, model}, {"B", b, model}}));
    NamedFixingReadCounter_ reads;
    RejectSubmissions_ tasks;
    const Dal::Detail::ScopedFixingReadObserver_ observeReads(&reads);
    const Dal::Script::Detail::ScopedSimulationObserver_ observeTasks(&tasks);
    bool admitted = false;
    const auto prepared = Dal::Script::Detail::PrepareScriptPortfolio(portfolio, 257, {}, {}, [&](const auto& plans) {
        ASSERT_EQ(plans.size(), 2);
        ASSERT_TRUE(reads.histories_.empty());
        ASSERT_EQ(reads.fixings_, 0);
        for (const auto& plan : plans) {
            ASSERT_TRUE(plan.View().Plan().KnownValues().empty());
            ASSERT_EQ(plan.Model().SimDim(), 1);
        }
        admitted = true;
    });
    ASSERT_TRUE(admitted);
    ASSERT_EQ(prepared.PathCount(), 257);
    ASSERT_EQ(reads.histories_.at("EQ[PORTFOLIO_PREPARED]"), 1);
    ASSERT_EQ(prepared.Groups().size(), 1);
    ASSERT_EQ(prepared.Groups()[0].tradePositions_, Vector_<size_t>({0, 1}));
    ASSERT_EQ(prepared.Trades().size(), 2);
    ASSERT_STREQ(FixingSourceKind(prepared.Valuation()), "GlobalSnapshot");
    ASSERT_EQ(prepared.Valuation().evaluationDate_, Date_(2026, 1, 2));
    ASSERT_DOUBLE_EQ(*prepared.Fixings()->Find("EQ[PORTFOLIO_PREPARED]", DateTime_(Date_(2026, 1, 1), 0.0)), 80.0);
    AAD::Scenario_<double> path;
    AAD::AllocatePath(prepared.Trades()[0].DefLine(), path);
    AAD::InitializePath(path);
    path[0].observations_[0] = 100.0;
    auto stateA = prepared.Trades()[0].BuildEvaluator<double>();
    auto stateB = prepared.Trades()[1].BuildEvaluator<double>();
    prepared.Trades()[0].Evaluate(path, stateA);
    prepared.Trades()[1].Evaluate(path, stateB);
    ASSERT_DOUBLE_EQ(stateA.VarVals()[prepared.Trades()[0].PayOffIdx()], 500.0);
    ASSERT_DOUBLE_EQ(stateB.VarVals()[prepared.Trades()[1].PayOffIdx()], 660.0);
    ASSERT_EQ(tasks.calls_, 0);
}

TEST(PortfolioPreparationTest, TestLateInvalidTradeFailsBeforeHistoryAndAdmission) {
    const Handle_<ModelData_> model(new BSModelData_("", 100.0, 0.2));
    const Handle_<ScriptProductData_> invalid(new ScriptProductData_("", {Cell_(Date_(2027, 1, 1))}, {"x = 1"}));
    const Handle_<ScriptPortfolioData_> portfolio(
        new ScriptPortfolioData_("", {{"A", HistoricalTrade("EQ[PORTFOLIO_UNREAD]"), model}, {"B", invalid, model}}));
    RejectFixingReads_ reads;
    RejectSubmissions_ tasks;
    const Dal::Detail::ScopedFixingReadObserver_ observeReads(&reads);
    const Dal::Script::Detail::ScopedSimulationObserver_ observeTasks(&tasks);
    bool admitted = false;
    AssertPortfolioFailure(
        [&] {
            static_cast<void>(Dal::Script::Detail::PrepareScriptPortfolio(portfolio, 257, Valuation(), {}, [&](const auto&) { admitted = true; }));
        },
        "trade=B; cause=");
    ASSERT_FALSE(admitted);
    ASSERT_EQ(reads.historyCalls_, 0);
    ASSERT_EQ(reads.fixingCalls_, 0);
    ASSERT_EQ(tasks.calls_, 0);
}

TEST(PortfolioPreparationTest, TestImpossibleStartupAdmissionReadsNoHistory) {
    const Handle_<ModelData_> model(new BSModelData_("", 100.0, 0.2));
    const auto product = HistoricalTrade("EQ[PORTFOLIO_UNREAD]");
    const Handle_<ScriptPortfolioData_> portfolio(new ScriptPortfolioData_("", {{"A", product, model}, {"B", product, model}}));
    RejectFixingReads_ reads;
    RejectSubmissions_ tasks;
    const Dal::Detail::ScopedFixingReadObserver_ observeReads(&reads);
    const Dal::Script::Detail::ScopedSimulationObserver_ observeTasks(&tasks);
    AssertPortfolioFailure(
        [&] {
            static_cast<void>(Dal::Script::Detail::PrepareScriptPortfolio(
                portfolio, 257, Valuation(), {}, [](const auto&) { THROW2("ScratchCapacityExceeded: group=0", ScriptError_); }));
        },
        "ScratchCapacityExceeded: group=0");
    ASSERT_EQ(reads.historyCalls_, 0);
    ASSERT_EQ(reads.fixingCalls_, 0);
    ASSERT_EQ(tasks.calls_, 0);
}

TEST(PortfolioPreparationTest, TestProspectiveGroupsMatchCompletedGroupsBeforeAnyHistory) {
    const DateTime_ fixing(Date_(2026, 1, 1), 0.0);
    StoreScriptTestFixing("EQ[PORTFOLIO_PLANNED_GROUP]", 80.0, fixing);
    const Handle_<ModelData_> shared(new BSModelData_("", 100.0, 0.2));
    const Handle_<ModelData_> distinct(new BSModelData_("", 100.0, 0.2));
    const auto trade = HistoricalTrade("EQ[PORTFOLIO_PLANNED_GROUP]");
    const Handle_<ScriptProductData_> later(new ScriptProductData_("", {Cell_(Date_(2026, 1, 1)), Cell_(Date_(2028, 1, 1))},
                                                                   {"h = FIX(EQ[PORTFOLIO_PLANNED_GROUP])", "pay PAYS h + FIX(EQ[MODEL])"}));
    const Handle_<ScriptPortfolioData_> portfolio(
        new ScriptPortfolioData_("", {{"A", trade, shared}, {"B", later, shared}, {"C", trade, shared}, {"D", trade, distinct}}));
    Vector_<Dal::Script::Detail::PortfolioScenarioGroup_> prospective;
    const auto prepared = Dal::Script::Detail::PrepareScriptPortfolio(portfolio, 257, Valuation(), {}, [&](const auto& plans) {
        RejectFixingReads_ reads;
        const Dal::Detail::ScopedFixingReadObserver_ observer(&reads);
        prospective = Dal::Script::Detail::PlanPortfolioScenarioGroups(*portfolio, plans);
        ASSERT_EQ(reads.historyCalls_, 0);
        ASSERT_EQ(reads.fixingCalls_, 0);
        ASSERT_EQ(prospective.size(), 3);
        ASSERT_EQ(prospective[0].tradePositions_, Vector_<size_t>({0, 2}));
        ASSERT_THROW(static_cast<void>(Dal::Script::Detail::GroupPreparedPortfolio({{&plans[0].View(), 0, 1, 1, true, true}})), ScriptError_);
    });
    ASSERT_EQ(prospective.size(), prepared.Groups().size());
    for (size_t group = 0; group < prospective.size(); ++group) {
        ASSERT_EQ(prospective[group].modelOwner_, prepared.Groups()[group].modelOwner_);
        ASSERT_EQ(prospective[group].tradePositions_, prepared.Groups()[group].tradePositions_);
    }
}

TEST(PortfolioPreparationTest, TestUnionSnapshotAndDateSurviveMutationDuringResolution) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 1, 2));
    StoreScriptTestFixing("EQ[PORTFOLIO_A]", 80.0, DateTime_(Date_(2026, 1, 1), 0.0));
    StoreScriptTestFixing("EQ[PORTFOLIO_B]", 20.0, DateTime_(Date_(2026, 1, 1), 0.0));
    const Handle_<ModelData_> model(new BSModelData_("", 100.0, 0.2));
    auto portfolio = Handle_<ScriptPortfolioData_>(
        new ScriptPortfolioData_("", {{"A", HistoricalTrade("EQ[PORTFOLIO_A]"), model}, {"B", HistoricalTrade("EQ[PORTFOLIO_B]"), model}}));
    auto valuation = Valuation();
    MonteCarloSettings_ simulation;
    simulation.compiled_ = false;
    ChangeGlobalsAfterFreeze_ reads;
    const Dal::Detail::ScopedFixingReadObserver_ observeReads(&reads);
    const auto prepared = Dal::Script::Detail::PrepareScriptPortfolio(portfolio, 257, valuation, simulation, [&](const auto&) {
        valuation.evaluationDate_ = Date_(2030, 1, 1);
        simulation.compiled_ = true;
        portfolio.reset();
    });
    ASSERT_EQ(prepared.Valuation().evaluationDate_, Date_(2026, 1, 2));
    ASSERT_TRUE(prepared.Portfolio());
    ASSERT_DOUBLE_EQ(prepared.Trades()[0].Plan().KnownValue(0), 80.0);
    ASSERT_DOUBLE_EQ(prepared.Trades()[1].Plan().KnownValue(0), 20.0);
    ASSERT_EQ(reads.histories_.at("EQ[PORTFOLIO_A]"), 1);
    ASSERT_EQ(reads.histories_.at("EQ[PORTFOLIO_B]"), 1);
    ASSERT_EQ(prepared.Groups().size(), 2);
    for (const auto& trade : prepared.Trades()) {
        ASSERT_EQ(trade.EvaluationDate(), Date_(2026, 1, 2));
        ASSERT_EQ(trade.Simulation().compiled_, false);
        ASSERT_STREQ(FixingSourceKind(trade.Settings()), "GlobalSnapshot");
    }
}

TEST(PortfolioPreparationTest, TestExplicitSnapshotMissingTradePreservesSourceAndRecovers) {
    const Handle_<ModelData_> model(new BSModelData_("", 100.0, 0.2));
    const Handle_<ScriptPortfolioData_> portfolio(
        new ScriptPortfolioData_("", {{"A", HistoricalTrade("EQ[PORTFOLIO_A]"), model}, {"B", HistoricalTrade("EQ[PORTFOLIO_B]"), model}}));
    auto valuation = Valuation();
    valuation.fixings_ =
        Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_({{"EQ[PORTFOLIO_A]", {{DateTime_(Date_(2026, 1, 1), 0.0), 80.0}}}}));
    NamedFixingReadCounter_ reads;
    const Dal::Detail::ScopedFixingReadObserver_ observeReads(&reads);
    AssertPortfolioFailure([&] { static_cast<void>(Dal::Script::Detail::PrepareScriptPortfolio(portfolio, 257, valuation)); },
                           "trade=B; cause=", "source=ExplicitSnapshot");
    valuation.fixings_ = Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_(
        {{"EQ[PORTFOLIO_A]", {{DateTime_(Date_(2026, 1, 1), 0.0), 80.0}}}, {"EQ[PORTFOLIO_B]", {{DateTime_(Date_(2026, 1, 1), 0.0), 20.0}}}}));
    const auto prepared = Dal::Script::Detail::PrepareScriptPortfolio(portfolio, 257, valuation);
    ASSERT_TRUE(reads.histories_.empty());
    ASSERT_EQ(prepared.Fixings(), valuation.fixings_);
    ASSERT_STREQ(FixingSourceKind(prepared.Valuation()), "ExplicitSnapshot");
    ASSERT_DOUBLE_EQ(prepared.Trades()[1].Plan().KnownValue(0), 20.0);
}

TEST(PortfolioPreparationTest, TestInvalidPathsAndUnsupportedTradesFailBeforeHistory) {
    const Handle_<ModelData_> model(new BSModelData_("", 100.0, 0.2));
    const auto product = HistoricalTrade("EQ[PORTFOLIO_UNREAD]");
    const Handle_<ScriptPortfolioData_> portfolio(new ScriptPortfolioData_("", {{"A", product, model}}));
    RejectFixingReads_ reads;
    const Dal::Detail::ScopedFixingReadObserver_ observeReads(&reads);
    for (const int paths : {0, -1})
        AssertPortfolioFailure([&] { static_cast<void>(Dal::Script::Detail::PrepareScriptPortfolio(portfolio, paths, Valuation())); },
                               "field=numPaths");
    ASSERT_THROW(static_cast<void>(Dal::Script::Detail::PrepareScriptPortfolio({}, 1, Valuation())), ScriptError_);
    for (const auto& [date, text] : Vector_<std::pair<Date_, String_>>{{Date_(2025, 1, 1), "pay PAYS 1"}, {Date_(2027, 1, 1), "EXERCISE 1"}}) {
        const Handle_<ScriptProductData_> unsupported(new ScriptProductData_("", {Cell_(date)}, {text}));
        const Handle_<ScriptPortfolioData_> bad(new ScriptPortfolioData_("", {{"offending", unsupported, model}}));
        AssertPortfolioFailure([&] { static_cast<void>(Dal::Script::Detail::PrepareScriptPortfolio(bad, 1, Valuation())); },
                               "trade=offending; cause=");
    }
    ASSERT_EQ(reads.historyCalls_, 0);
    ASSERT_EQ(reads.fixingCalls_, 0);
}

TEST(PortfolioPreparationTest, TestOriginalOwnersAndMeshesSurviveProducerGrouping) {
    const Handle_<ModelData_> shared(new BSModelData_("same", 100.0, 0.2));
    const Handle_<ModelData_> equal(new BSModelData_("same", 100.0, 0.2));
    const Handle_<ScriptProductData_> shortTrade(new ScriptProductData_("", {Cell_(Date_(2027, 1, 1))}, {"pay PAYS SPOT()"}));
    const Handle_<ScriptProductData_> longTrade(
        new ScriptProductData_("", {Cell_(Date_(2027, 1, 1)), Cell_(Date_(2028, 1, 1))}, {"x = SPOT()", "pay PAYS x + SPOT()"}));
    const Handle_<ScriptPortfolioData_> portfolio(
        new ScriptPortfolioData_("", {{"A", shortTrade, shared}, {"B", longTrade, shared}, {"C", shortTrade, shared}, {"D", shortTrade, equal}}));
    RejectFixingReads_ reads;
    const Dal::Detail::ScopedFixingReadObserver_ observeReads(&reads);
    const auto prepared = Dal::Script::Detail::PrepareScriptPortfolio(portfolio, 257, Valuation());
    ASSERT_EQ(prepared.Groups().size(), 3);
    ASSERT_EQ(prepared.Groups()[0].modelOwner_, 0);
    ASSERT_EQ(prepared.Groups()[0].tradePositions_, Vector_<size_t>({0, 2}));
    ASSERT_EQ(prepared.Groups()[1].modelOwner_, 0);
    ASSERT_EQ(prepared.Groups()[1].tradePositions_, Vector_<size_t>({1}));
    ASSERT_EQ(prepared.Groups()[2].modelOwner_, 1);
    ASSERT_EQ(prepared.Groups()[2].tradePositions_, Vector_<size_t>({3}));
    ASSERT_EQ(prepared.Trades()[0].TimeLine().size(), 1);
    ASSERT_EQ(prepared.Trades()[1].TimeLine().size(), 2);
    ASSERT_EQ(prepared.Trades()[0].Plan().SampleDates(), Vector_<Date_>({Date_(2027, 1, 1)}));
    ASSERT_EQ(prepared.Trades()[1].Plan().SampleDates(), Vector_<Date_>({Date_(2027, 1, 1), Date_(2028, 1, 1)}));
    ASSERT_EQ(reads.historyCalls_, 0);
    ASSERT_EQ(reads.fixingCalls_, 0);
}

TEST(PortfolioPreparationTest, TestInvalidUnionQuoteNamesOffendingTradeBeforeResolution) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 1, 2));
    StoreScriptTestFixing("EQ[PORTFOLIO_A]", 80.0, DateTime_(Date_(2026, 1, 1), 0.0));
    StoreScriptTestFixing("FX[AUD/CAD]", 1.25, DateTime_(Date_(2026, 1, 1), 0.0));
    StoreScriptTestFixing("FX[CAD/AUD]", 0.9, DateTime_(Date_(2026, 1, 1), 0.0));
    const Handle_<ModelData_> model(new BSModelData_("", 100.0, 0.2));
    const Handle_<ScriptPortfolioData_> portfolio(
        new ScriptPortfolioData_("", {{"A", HistoricalTrade("EQ[PORTFOLIO_A]"), model}, {"B", HistoricalTrade("FX[AUD/CAD]"), model}}));
    NamedFixingReadCounter_ reads;
    const Dal::Detail::ScopedFixingReadObserver_ observeReads(&reads);
    AssertPortfolioFailure([&] { static_cast<void>(Dal::Script::Detail::PrepareScriptPortfolio(portfolio, 257)); }, "trade=B",
                           "source=GlobalSnapshot");
    ASSERT_EQ(reads.histories_.at("FX[AUD/CAD]"), 1);
    ASSERT_EQ(reads.histories_.at("FX[CAD/AUD]"), 1);
    ASSERT_EQ(reads.fixings_, 0);
    StoreScriptTestFixing("FX[CAD/AUD]", 0.8, DateTime_(Date_(2026, 1, 1), 0.0));
}

TEST(PortfolioPreparationTest, TestCompilationFailurePublishesNoPortfolioAndRecovers) {
    const Handle_<ModelData_> model(new BSModelData_("", 100.0, 0.2));
    const Handle_<ScriptProductData_> product(new ScriptProductData_("", {Cell_(Date_(2027, 1, 1))}, {"pay PAYS SPOT()"}));
    const Handle_<ScriptPortfolioData_> portfolio(new ScriptPortfolioData_("", {{"A", product, model}, {"B", product, model}}));
    MonteCarloSettings_ simulation;
    simulation.compiled_ = true;
    const auto prior = Dal::Script::Detail::PrepareScriptPortfolio(portfolio, 257, Valuation(), simulation);
    {
        RejectCompilation_ reject;
        const Dal::Script::Detail::ScopedSimulationObserver_ observe(&reject);
        AssertPortfolioFailure([&] { static_cast<void>(Dal::Script::Detail::PrepareScriptPortfolio(portfolio, 257, Valuation(), simulation)); },
                               "trade=A; cause=", "injected compilation failure");
        ASSERT_EQ(reject.submissions_, 0);
    }
    const auto recovered = Dal::Script::Detail::PrepareScriptPortfolio(portfolio, 257, Valuation(), simulation);
    ASSERT_EQ(recovered.Groups()[0].tradePositions_, prior.Groups()[0].tradePositions_);
    prior.Trades()[0].RequireExecutable();
    recovered.Trades()[1].RequireExecutable();
}

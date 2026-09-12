//
// Created by Codex on 2026/9/13.
//

#include <gtest/gtest.h>

#include <dal/indice/detail/fixingobserver.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/event.hpp>
#include <dal/script/preparation.hpp>
#include <dal/script/simulation.hpp>
#include <dal/storage/globals.hpp>
#include <limits>

using namespace Dal;
using namespace Dal::Script;

namespace {
    struct ReadCounter_ : Dal::Detail::FixingReadObserver_ {
        std::map<String_, size_t> histories_;
        size_t finalFixings_ = 0;
        void BeforeHistory(const String_& name) override { ++histories_[name]; }
        void BeforeFixing(const Index_&, const Environment_*, const DateTime_&) override { ++finalFixings_; }
    };

    struct SubmissionCounter_ : Dal::Script::Detail::SimulationObserver_ {
        size_t submissions_ = 0;
        void AfterSubmission() override { ++submissions_; }
    };

    ScriptProductData_ Product(const String_& text, const Date_& date = Date_(2026, 9, 22)) { return {"", {Cell_(date)}, {text}}; }

    void Store(const String_& name, double value, const DateTime_& time = DateTime_(Date_(2026, 9, 11), 0.0)) {
        FixHistory_ history;
        history.vals_ = {{time, value}};
        XGLOBAL::StoreFixings(name, history, false);
    }

    void ClearHistory(const String_& name) { Store(name, 1.0, DateTime_(Date_(2000, 1, 1), 0.0)); }

    template <class F_> void AssertError(F_ action, const std::string& expected) {
        try {
            action();
            FAIL() << "expected " << expected;
        } catch (const ScriptError_& error) {
            ASSERT_NE(std::string(error.what()).find(expected), std::string::npos) << error.what();
        }
    }
} // namespace

TEST(ScriptFixingPreparationTest, TestUnpreparedFixingRejectedBeforeModelAccess) {
    SubmissionCounter_ counter;
    const Dal::Script::Detail::ScopedSimulationObserver_ observe(&counter);
    ScriptProduct_ product({Cell_(Date_(2026, 9, 22))}, {"payoff PAYS FIX(EQ[DAL199_BARRIER], 2026-09-11)"});
    for (const bool compiled : {false, true}) {
        for (const bool aad : {false, true}) {
            try {
                if (aad)
                    static_cast<void>(MCSimulation<AAD::Number_>(product, {}, 1, "sobol", false, compiled));
                else
                    static_cast<void>(MCSimulation<double>(product, {}, 1, "sobol", false, compiled));
                FAIL() << "unprepared FIX must fail before any model access";
            } catch (const ScriptError_& error) {
                ASSERT_NE(std::string(error.what()).find("PreparationRequired"), std::string::npos);
            }
        }
    }
    ASSERT_EQ(counter.submissions_, 0);
}

TEST(ScriptFixingPreparationTest, TestEvaluationDateCapturedAtPreparation) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    ScriptProduct_ product({Cell_(Date_(2026, 9, 11))}, {"payoff PAYS 80"});
    XGLOBAL::SetEvaluationDate(Date_(2026, 9, 10));
    product.PreProcess(false, true);
    ASSERT_EQ(product.EventDates().size(), 1);
    ASSERT_NEAR(product.TimeLine()[0], 1.0 / DAYS_PER_YEAR, 1.0e-12);
}

TEST(ScriptFixingPreparationTest, TestUniqueHistoryCount) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    const DateTime_ historyTime(Date_(2026, 9, 11), 0.0);
    FixHistory_ history;
    history.vals_ = {{historyTime, 80.0}};
    XGLOBAL::StoreFixings("EQ[DAL199_COUNT]", history, false);
    String_ expression = "payoff PAYS ";
    for (size_t i = 0; i < 100; ++i)
        expression += (i ? "+" : "") + String_("FIX(EQ[DAL199_COUNT], 2026-09-11)");
    const ScriptProductData_ product("", {Cell_(Date_(2026, 9, 11)), Cell_(Date_(2026, 9, 15)), Cell_(Date_(2026, 9, 22))},
                                     {expression, expression, expression});
    ReadCounter_ counter;
    const Dal::Detail::ScopedFixingReadObserver_ observe(&counter);
    const auto prepared = PrepareScript(product);
    ASSERT_EQ(prepared.Plan().Requests().size(), 1);
    ASSERT_EQ(prepared.Plan().Requests()[0].uses_.size(), 300);
    ASSERT_EQ(prepared.Plan().KnownValues().size(), 1);
    ASSERT_DOUBLE_EQ(prepared.Plan().KnownValue(0), 80.0);
    ASSERT_EQ(counter.finalFixings_, 1);
    ASSERT_EQ(counter.histories_.at("EQ[DAL199_COUNT]"), 1);
    ASSERT_EQ(prepared.Product().ParsedEventDates().size(), 3);
    ASSERT_EQ(prepared.Product().PastEventDates().size(), 1);
    ASSERT_EQ(prepared.Product().EventDates().size(), 2);
}

TEST(ScriptFixingPreparationTest, TestCanonicalKey) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    const DateTime_ h(Date_(2026, 9, 11), 0.0);
    FixHistory_ history;
    history.vals_ = {{DateTime_(Date_(2026, 9, 10), 0.0), 70.0}, {h, 80.0}};
    XGLOBAL::StoreFixings("EQ[DAL199_KEYS]", history, false);
    Store("EQ[DAL199_KEYS]>3M", 81.0);
    Store("EQ[DAL199_KEYS]@2026-12-31", 82.0);
    Store("FX[AUD/CAD]", 1.25);
    ClearHistory("FX[CAD/AUD]");
    ReadCounter_ counter;
    const Dal::Detail::ScopedFixingReadObserver_ observe(&counter);
    const auto prepared = PrepareScript(Product("payoff PAYS FIX(EQ[DAL199_KEYS], 2026-09-11) + FIX(eq[dal199_keys], 2026-09-11)"
                                                " + FIX(EQ[DAL199_KEYS], 2026-09-10) + FIX(EQ[DAL199_KEYS]>3M, 2026-09-11)"
                                                " + FIX(EQ[DAL199_KEYS]@2026-12-31, 2026-09-11)"
                                                " + FIX(FX[AUD/CAD], 2026-09-11) + FIX(FX[CAD/AUD], 2026-09-11)"));
    ASSERT_EQ(prepared.Plan().Requests().size(), 6);
    ASSERT_EQ(prepared.Plan().Request(0).uses_.size(), 2);
    ASSERT_EQ(counter.finalFixings_, 6);
    ASSERT_EQ(counter.histories_.size(), 5);
    for (const auto& historyCount : counter.histories_)
        ASSERT_EQ(historyCount.second, 1);
    ASSERT_NEAR(prepared.Plan().KnownValue(4), 1.25, 1.0e-12);
    ASSERT_NEAR(prepared.Plan().KnownValue(5), 0.8, 1.0e-12);
    ASSERT_THROW(static_cast<void>(prepared.Plan().Request(6)), ScriptError_);
    ASSERT_THROW(static_cast<void>(prepared.Plan().KnownValue(6)), ScriptError_);
}

TEST(ScriptFixingPreparationTest, TestTodayPolicyAndFutureOnly) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    Store("EQ[DAL199_TODAY]", 80.0, DateTime_(Date_(2026, 9, 12), 0.0));
    const auto product = Product("payoff PAYS FIX(EQ[DAL199_TODAY])", Date_(2026, 9, 12));
    ReadCounter_ counter;
    const Dal::Detail::ScopedFixingReadObserver_ observe(&counter);
    const auto modelToday = PrepareScript(product);
    ASSERT_EQ(modelToday.Plan().Requests().size(), 1);
    ASSERT_TRUE(modelToday.Plan().KnownValues().empty());
    ASSERT_FALSE(modelToday.Plan().Request(0).historyValueId_);
    const auto future = PrepareScript(Product("payoff PAYS FIX(EQ[DAL199_TODAY], 2026-09-15)"));
    ASSERT_TRUE(future.Plan().KnownValues().empty());
    ASSERT_TRUE(counter.histories_.empty());
    ASSERT_EQ(counter.finalFixings_, 0);
    ScriptValuationSettings_ settings;
    settings.todayFixingPolicy_ = TodayFixingPolicy_::Value_::REQUIREHISTORICAL;
    const auto historicalToday = PrepareScript(product, settings);
    ASSERT_DOUBLE_EQ(historicalToday.Plan().KnownValue(0), 80.0);
    ASSERT_EQ(counter.finalFixings_, 1);
    ASSERT_EQ(counter.histories_.size(), 1);
    const Handle_<MarketFixingSnapshot_> empty(new MarketFixingSnapshot_());
    AssertError([&] { PrepareScript(product, settings, empty); }, "MissingFixing");
    ASSERT_EQ(counter.histories_.size(), 1);
    AssertError([&] { MCSimulation<double>(future, {}, 1); }, "UnsupportedExecutionMode");
}

TEST(ScriptFixingPreparationTest, TestExactTimestamp) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    for (const auto time : {DateTime_(Date_(2026, 9, 11), 11, 0), DateTime_(Date_(2026, 9, 10), 0.0)}) {
        Store("EQ[DAL199_TIME]", 80.0, time);
        AssertError([&] { PrepareScript(Product("payoff PAYS FIX(EQ[DAL199_TIME], 2026-09-11)")); }, "MissingFixing");
    }
    for (const auto* time : {"2026-09-11+1", "2026-09-11T00:00:00", "H"})
        ASSERT_THROW(PrepareScript(Product("payoff PAYS FIX(EQ[DAL199_TIME], " + String_(time) + ")")), ScriptError_);
}

TEST(ScriptFixingPreparationTest, TestIndexVirtualFx) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    const auto product = Product("payoff PAYS FIX(FX[CHF/JPY], 2026-09-11)");
    ClearHistory("FX[CHF/JPY]");
    Store("FX[JPY/CHF]", 0.8);
    ASSERT_NEAR(PrepareScript(product).Plan().KnownValue(0), 1.25, 1.0e-12);
    Store("FX[CHF/JPY]", 1.25);
    ASSERT_NEAR(PrepareScript(product).Plan().KnownValue(0), 1.25, 1.0e-12);
    Store("FX[CHF/JPY]", 1.3);
    AssertError([&] { PrepareScript(product); }, "Inconsistent direct/reverse");
    ClearHistory("FX[JPY/CHF]");
    for (const double value : {0.0, -1.0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()}) {
        Store("FX[CHF/JPY]", value);
        ASSERT_THROW(PrepareScript(product), ScriptError_);
    }
    ClearHistory("FX[CHF/JPY]");
    Store("FX[JPY/CHF]", std::numeric_limits<double>::denorm_min());
    AssertError([&] { PrepareScript(product); }, "InvalidFixing");
}

TEST(ScriptFixingPreparationTest, TestMissingAndFinite) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    const auto product = Product("payoff PAYS FIX(EQ[DAL199_FINITE], 2026-09-11)");
    ClearHistory("EQ[DAL199_FINITE]");
    AssertError([&] { PrepareScript(product); }, "MissingFixing");
    for (const double value : {0.0, -80.0}) {
        Store("EQ[DAL199_FINITE]", value);
        ASSERT_DOUBLE_EQ(PrepareScript(product).Plan().KnownValue(0), value);
    }
    for (const double value :
         {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()}) {
        Store("EQ[DAL199_FINITE]", value);
        AssertError([&] { PrepareScript(product); }, "finite");
    }
    ASSERT_THROW(PrepareScript(Product("payoff PAYS FIX(UNREGISTERED_DAL199[X], 2026-09-11)")), ScriptError_);
}

TEST(ScriptFixingPreparationTest, TestRepricing) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    const DateTime_ h(Date_(2026, 9, 11), 0.0);
    const auto product = Product("payoff PAYS FIX(EQ[DAL199_REPRICE], 2026-09-11)");
    Store("EQ[DAL199_REPRICE]", 80.0);
    const auto snapshot = SnapshotGlobalFixings({{"EQ[DAL199_REPRICE]", h}});
    const auto first = PrepareScript(product);
    Store("EQ[DAL199_REPRICE]", 90.0);
    const auto second = PrepareScript(product);
    ReadCounter_ counter;
    const Dal::Detail::ScopedFixingReadObserver_ observe(&counter);
    const auto third = PrepareScript(product, {}, snapshot);
    ASSERT_DOUBLE_EQ(first.Plan().KnownValue(0), 80.0);
    ASSERT_DOUBLE_EQ(second.Plan().KnownValue(0), 90.0);
    ASSERT_DOUBLE_EQ(third.Plan().KnownValue(0), 80.0);
    ASSERT_EQ(counter.finalFixings_, 1);
    ASSERT_TRUE(counter.histories_.empty());
}

TEST(ScriptFixingPreparationTest, TestBranchPrefetchAndSourceUses) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    const String_ dead = "IF 1 = 0 THEN payoff PAYS FIX(EQ[DAL199_DEAD], 2026-09-11) ELSE payoff PAYS 0 END";
    ClearHistory("EQ[DAL199_DEAD]");
    const auto product = Product(dead);
    AssertError([&] { PrepareScript(product); }, "row=1");
    const ScriptProductData_ pastPayoff("", {Cell_(Date_(2026, 9, 11)), Cell_(Date_(2026, 9, 22))},
                                        {"payoff PAYS FIX(EQ[DAL199_DEAD])", "payoff PAYS 0"});
    ASSERT_THROW(PrepareScript(pastPayoff), ScriptError_);
    Store("EQ[DAL199_DEAD]", 80.0);
    const auto prepared = PrepareScript(pastPayoff);
    const auto& use = prepared.Plan().Request(0).uses_[0];
    ASSERT_EQ(use.source_.row_, 1);
    ASSERT_EQ(use.source_.eventDate_, Date_(2026, 9, 11));
    ASSERT_EQ(use.eventId_, 0);
    ASSERT_EQ(use.statementId_, 0);
    ASSERT_GT(use.nodeId_, 0);
    ClearHistory("EQ[DAL199_DEAD]");
    ASSERT_THROW(PrepareScript(pastPayoff), ScriptError_);
    ASSERT_DOUBLE_EQ(prepared.Plan().KnownValue(0), 80.0);
}

TEST(ScriptFixingPreparationTest, TestNoLookAheadAndExpiredStructure) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    ReadCounter_ counter;
    const Dal::Detail::ScopedFixingReadObserver_ observe(&counter);
    for (const auto event : {Date_(2026, 9, 10), Date_(2026, 9, 14)}) {
        const auto product = Product("IF 1 = 0 THEN payoff PAYS FIX(EQ[DAL199_LOOK], 2026-09-15) ELSE payoff PAYS 0 END", event);
        AssertError([&] { PrepareScript(product); }, "LookAheadObservation");
    }
    const auto equal = PrepareScript(Product("payoff PAYS FIX(EQ[DAL199_LOOK], 2026-09-15)", Date_(2026, 9, 15)));
    ASSERT_TRUE(equal.Plan().KnownValues().empty());
    const auto expired = PrepareScript(Product("payoff PAYS FIX(EQ[DAL199_LOOK])", Date_(2026, 9, 11)));
    ASSERT_TRUE(expired.AllExpired());
    ASSERT_TRUE(expired.Plan().KnownValues().empty());
    ASSERT_EQ(expired.Plan().Requests().size(), 1);
    const ScriptProductData_ empty("", {}, {});
    const ScriptProductData_ definitions("", {Cell_("SCALE")}, {"2"});
    for (const auto* invalid : {&empty, &definitions})
        AssertError([&] { PrepareScript(*invalid); }, "InvalidScriptStructure");
    for (const auto event : {Date_(2026, 9, 11), Date_(2026, 9, 22)})
        AssertError([&] { PrepareScript(Product("x = 1", event)); }, "no PAYS payoff");
    ASSERT_TRUE(counter.histories_.empty());
    ASSERT_EQ(counter.finalFixings_, 0);
}

TEST(ScriptFixingPreparationTest, TestPreparationFailurePublishesNoPlanOrWorkers) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    Store("EQ[DAL199_FIRST]", 80.0);
    ClearHistory("EQ[DAL199_LAST]");
    ReadCounter_ reads;
    SubmissionCounter_ workers;
    const Dal::Detail::ScopedFixingReadObserver_ readObserve(&reads);
    const Dal::Script::Detail::ScopedSimulationObserver_ workerObserve(&workers);
    std::optional<PreparedScript_> result;
    AssertError([&] { result.emplace(PrepareScript(Product("payoff PAYS FIX(EQ[DAL199_FIRST], 2026-09-11) + FIX(EQ[DAL199_LAST], 2026-09-11)"))); },
                "EQ[DAL199_LAST]");
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(reads.finalFixings_, 2);
    ASSERT_EQ(workers.submissions_, 0);
}

TEST(ScriptFixingPreparationTest, TestFrozenReadsNeedNoHistoryAccess) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    Store("EQ[DAL199_FROZEN]", 80.0);
    const auto prepared = PrepareScript(Product("payoff PAYS FIX(EQ[DAL199_FROZEN], 2026-09-11)"));
    struct RejectReads_ : Dal::Detail::FixingReadObserver_ {
        void BeforeHistory(const String_&) override { THROW("unexpected history read"); }
        void BeforeFixing(const Index_&, const Environment_*, const DateTime_&) override { THROW("unexpected fixing call"); }
    } reject;
    const Dal::Detail::ScopedFixingReadObserver_ observe(&reject);
    for (size_t path = 0; path < 8193; ++path)
        ASSERT_DOUBLE_EQ(prepared.Plan().KnownValue(*prepared.Plan().Request(0).historyValueId_), 80.0);
}

TEST(ScriptFixingPreparationTest, TestExpiredSimulationAndSubmissionSeam) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    const auto expired = PrepareScript(Product("payoff PAYS FIX(EQ[DAL199_EXPIRED])", Date_(2026, 9, 11)));
    const Handle_<ModelData_> model(new BSModelData_("", 100.0, 0.2));
    SubmissionCounter_ workers;
    const Dal::Script::Detail::ScopedSimulationObserver_ observe(&workers);
    for (const bool compiled : {false, true}) {
        const auto plain = MCSimulation<double>(expired, model, 8193, "sobol", false, compiled);
        const auto aad = MCSimulation<AAD::Number_>(expired, model, 8193, "sobol", false, compiled);
        ASSERT_DOUBLE_EQ(plain.aggregated_, 0.0);
        ASSERT_DOUBLE_EQ(aad.aggregated_, 0.0);
        ASSERT_EQ(aad.risks_.size(), 4);
        for (const double risk : aad.risks_)
            ASSERT_DOUBLE_EQ(risk, 0.0);
    }
    ASSERT_EQ(workers.submissions_, 0);
    ScriptProduct_ legacy({Cell_(Date_(2026, 9, 22))}, {"payoff PAYS SPOT()"});
    legacy.PreProcess(false, true);
    ASSERT_THROW(MCSimulation<double>(legacy, {}, 1), Exception_);
    ASSERT_THROW(MCSimulation<AAD::Number_>(legacy, {}, 1), Exception_);
    ASSERT_EQ(workers.submissions_, 0);
    ASSERT_GT(MCSimulation<double>(legacy, model, 1).aggregated_, 0.0);
    ASSERT_EQ(workers.submissions_, 1);
}

TEST(ScriptFixingPreparationTest, TestSnapshotFailureNamesFailingUse) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    Store("EQ[DAL199_SOURCE_A]", 80.0);
    Store("EQ[DAL199_SOURCE_B]", std::numeric_limits<double>::quiet_NaN());
    const ScriptProductData_ product("", {Cell_(Date_(2026, 9, 22)), Cell_(Date_(2026, 9, 22))},
                                     {"payoff PAYS FIX(EQ[DAL199_SOURCE_A], 2026-09-11)", "payoff PAYS FIX(EQ[DAL199_SOURCE_B], 2026-09-11)"});
    AssertError([&] { PrepareScript(product); }, "row=2");
}

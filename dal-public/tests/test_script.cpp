//
// Created by dal-tester on 2026/8/15.
//

#include <gtest/gtest.h>

#include <dal/indice/detail/fixingobserver.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/detail/simulationobserver.hpp>

#include <dal-public/src/global.hpp>
#include <dal-public/src/script.hpp>

using Dal::Cell_;
using Dal::Date_;
using Dal::String_;
using Dal::Vector_;

namespace {
    // Legacy text dumps only live events at the current evaluation date.
    class ScopedEvaluationDate_ {
        Date_ previous_;

    public:
        explicit ScopedEvaluationDate_(const Date_& d) : previous_(Dal::GetEvaluationDate()) { Dal::SetEvaluationDate(d); }
        ~ScopedEvaluationDate_() { Dal::SetEvaluationDate(previous_); }
    };
} // namespace

TEST(ScriptTest, TestDebugReturnsNonEmptyDescription) {
    const ScopedEvaluationDate_ evalDate(Date_(2022, 9, 25));
    const Vector_<Cell_> dates = {Cell_("STRIKE"), Cell_(Date_(2023, 9, 25))};
    const Vector_<String_> events = {String_("100.0"), String_("call pays MAX(spot() - STRIKE, 0.0)")};
    const auto product = Dal::NewScriptProduct(String_("dal_public_script_debug"), dates, events);

    const String_ description = Dal::DebugScriptProduct(product);

    ASSERT_FALSE(description.empty());
}

TEST(ScriptTest, TestDebugIsRepeatable) {
    const ScopedEvaluationDate_ evalDate(Date_(2022, 9, 25));
    const Vector_<Cell_> dates = {Cell_("STRIKE"), Cell_(Date_(2023, 9, 25))};
    const Vector_<String_> events = {String_("100.0"), String_("call pays MAX(spot() - STRIKE, 0.0)")};
    const auto product = Dal::NewScriptProduct(String_("dal_public_script_repeat"), dates, events);

    const String_ first = Dal::DebugScriptProduct(product);
    const String_ second = Dal::DebugScriptProduct(product);

    ASSERT_EQ(first, second);
}

TEST(ScriptTest, TestDebugRejectsMalformedScript) {
    const ScopedEvaluationDate_ evalDate(Date_(2022, 9, 25));
    const Vector_<Cell_> dates = {Cell_(Date_(2023, 9, 25))};
    const Vector_<String_> events = {String_("call pays THIS IS NOT A SCRIPT")};
    const auto product = Dal::NewScriptProduct(String_("dal_public_script_bad"), dates, events);

    ASSERT_THROW(Dal::DebugScriptProduct(product), Dal::Exception_);
}

namespace {
    Dal::Handle_<Dal::ScriptProductData_> MakeCallProduct(const char* name) {
        const Vector_<Cell_> dates = {Cell_("STRIKE"), Cell_(Date_(2023, 9, 25))};
        const Vector_<String_> events = {String_("100.0"), String_("call pays MAX(spot() - STRIKE, 0.0)")};
        return Dal::NewScriptProduct(String_(name), dates, events);
    }
} // namespace

TEST(ScriptTest, TestDebugJsonSchemaVariablesAndConstants) {
    const ScopedEvaluationDate_ evalDate(Date_(2022, 9, 25));
    const auto product = MakeCallProduct("dal_public_script_json");

    const String_ json = Dal::DebugScriptProductJson(product);

    ASSERT_NE(json.find(String_("\"schema\":\"dal.script-product/1\"")), String_::npos);
    //  IndexVariables enrichment: resolved indices, variable table, constant table, payoff slot
    ASSERT_NE(json.find(String_("\"variables\":[{\"index\":0,\"name\":\"call\"}],\"payoff_index\":0")), String_::npos);
    ASSERT_NE(json.find(String_("\"constants\":[{\"index\":0,\"name\":\"STRIKE\",\"value\":100}]")), String_::npos);
    ASSERT_NE(json.find(String_("{\"id\":\"n0\",\"kind\":\"pays\",\"target\":{\"id\":\"n1\",\"kind\":\"var\",\"name\":\"call\",\"index\":0")), String_::npos);
    ASSERT_NE(json.find(String_("\"date\":\"2023-09-25\",\"phase\":\"future\"")), String_::npos);
}

TEST(ScriptTest, TestDebugJsonIsRepeatable) {
    const ScopedEvaluationDate_ evalDate(Date_(2022, 9, 25));
    const auto product = MakeCallProduct("dal_public_script_json_repeat");

    const String_ first = Dal::DebugScriptProductJson(product);
    const String_ second = Dal::DebugScriptProductJson(product);

    ASSERT_EQ(first, second);
}

TEST(ScriptTest, TestDebugJsonEmptyProduct) {
    const ScopedEvaluationDate_ evalDate(Date_(2022, 9, 25));
    const auto product = Dal::NewScriptProduct(String_("dal_public_script_empty"), {}, {});

    const String_ json = Dal::DebugScriptProductJson(product);

    ASSERT_EQ(json, String_("{\"schema\":\"dal.script-product/1\",\"events\":[]}"));
}

TEST(ScriptTest, TestDebugTreeShowsVariablesAndEvents) {
    const ScopedEvaluationDate_ evalDate(Date_(2022, 9, 25));
    const auto product = MakeCallProduct("dal_public_script_tree");

    const String_ tree = Dal::DebugScriptProductTree(product);

    ASSERT_NE(tree.find(String_("Variables: call*")), String_::npos);
    ASSERT_NE(tree.find(String_("Constants: STRIKE=100")), String_::npos);
    ASSERT_NE(tree.find(String_("📅 1 · 2023-09-25 · future")), String_::npos);
    ASSERT_NE(tree.find(String_("└── (1) call ⇐ max(spot() − STRIKE, 0)")), String_::npos);
}

TEST(ScriptTest, TestDebugTreeAsciiStyleAndWidth) {
    const ScopedEvaluationDate_ evalDate(Date_(2022, 9, 25));
    const auto product = MakeCallProduct("dal_public_script_tree_ascii");

    const String_ tree = Dal::DebugScriptProductTree(product, true, 40);

    ASSERT_NE(tree.find(String_("`-- (1) call <= max(spot() - STRIKE, 0)")), String_::npos);
    ASSERT_NE(tree.find(String_("# 1 @ 2023-09-25 @ future")), String_::npos);
}

TEST(ScriptTest, TestPublicDumpPastTodayAndFuturePhases) {
    const ScopedEvaluationDate_ evalDate(Date_(2026, 9, 12));
    const auto product = Dal::NewScriptProduct("dump_phases", {Cell_(Date_(2026, 9, 11)), Cell_(Date_(2026, 9, 12)), Cell_(Date_(2026, 9, 22))},
                                               {"x = 80", "y = 1", "payoff PAYS x"});
    const String_ json = Dal::DebugScriptProductJson(product);
    ASSERT_NE(json.find("\"schema\":\"dal.script-product/1\""), String_::npos);
    ASSERT_NE(json.find("\"date\":\"2026-09-11\",\"phase\":\"past\""), String_::npos);
    ASSERT_NE(json.find("\"date\":\"2026-09-12\",\"phase\":\"future\""), String_::npos);
    ASSERT_NE(json.find("\"date\":\"2026-09-22\",\"phase\":\"future\""), String_::npos);
    const String_ tree = Dal::DebugScriptProductTree(product, true);
    ASSERT_NE(tree.find("# 1 @ 2026-09-11 @ past"), String_::npos);
    ASSERT_NE(tree.find("# 2 @ 2026-09-12 @ future"), String_::npos);
    ASSERT_NE(tree.find("# 3 @ 2026-09-22 @ future"), String_::npos);
}

TEST(ScriptTest, TestLegacyDumpOmitsHistoricalEventsWithoutIndexing) {
    const ScopedEvaluationDate_ evalDate(Date_(2026, 9, 12));
    const auto product = Dal::NewScriptProduct("dump_live_only", {Cell_(Date_(2026, 9, 11)), Cell_(Date_(2026, 9, 22))}, {"x = 80", "payoff PAYS x"});
    const String_ legacy = Dal::DebugScriptProduct(product);
    ASSERT_EQ(legacy.find("2026-09-11"), String_::npos);
    ASSERT_NE(legacy.find("EventTime_: 2026-09-22\tEvent_: 1"), String_::npos);
    ASSERT_EQ(legacy.find("Var[0] ="), String_::npos);
    ASSERT_NE(legacy.find("VAR[payoff,-1,"), String_::npos);
    const auto liveOnly = Dal::NewScriptProduct("dump_live_only_expected", {Cell_(Date_(2026, 9, 22))}, {"payoff PAYS x"});
    ASSERT_EQ(legacy, Dal::DebugScriptProduct(liveOnly));
}

TEST(ScriptTest, TestPublicDumpsRefreshEvaluationDateOnSameProduct) {
    const ScopedEvaluationDate_ evalDate(Date_(2026, 9, 12));
    const auto product =
        Dal::NewScriptProduct("dump_repartition", {Cell_(Date_(2026, 9, 11)), Cell_(Date_(2026, 9, 22))}, {"x = 80", "payoff PAYS x"});
    const String_ originalJson = Dal::DebugScriptProductJson(product);
    const String_ originalTree = Dal::DebugScriptProductTree(product, true);
    const String_ originalLegacy = Dal::DebugScriptProduct(product);

    Dal::SetEvaluationDate(Date_(2026, 9, 10));
    ASSERT_EQ(Dal::DebugScriptProductJson(product).find("\"phase\":\"past\""), String_::npos);
    ASSERT_NE(Dal::DebugScriptProductTree(product, true).find("# 1 @ 2026-09-11 @ future"), String_::npos);
    ASSERT_NE(Dal::DebugScriptProduct(product).find("EventTime_: 2026-09-11"), String_::npos);

    Dal::SetEvaluationDate(Date_(2026, 9, 23));
    ASSERT_EQ(Dal::DebugScriptProductJson(product).find("\"phase\":\"future\""), String_::npos);
    ASSERT_NE(Dal::DebugScriptProductTree(product, true).find("# 2 @ 2026-09-22 @ past"), String_::npos);
    ASSERT_THROW(Dal::DebugScriptProduct(product), Dal::ScriptError_);

    Dal::SetEvaluationDate(Date_(2026, 9, 12));
    ASSERT_EQ(Dal::DebugScriptProductJson(product), originalJson);
    ASSERT_EQ(Dal::DebugScriptProductTree(product, true), originalTree);
    ASSERT_EQ(Dal::DebugScriptProduct(product), originalLegacy);
}

TEST(ScriptTest, TestPublicDumpsPreserveRawBranchesAndFixRestrictionsWithoutHistory) {
    Dal::InitGlobalData(1);
    const ScopedEvaluationDate_ evalDate(Date_(2026, 9, 12));
    struct RejectReads_ : Dal::Detail::FixingReadObserver_ {
        size_t historyCalls_ = 0;
        size_t fixingCalls_ = 0;
        void BeforeHistory(const String_&) override {
            ++historyCalls_;
            THROW("debug must not read global history");
        }
        void BeforeFixing(const Dal::Index_&, const Dal::Environment_*, const Dal::DateTime_&) override {
            ++fixingCalls_;
            THROW("debug must not resolve a fixing");
        }
    } reads;
    struct RejectWorkers_ : Dal::Script::Detail::SimulationObserver_ {
        size_t calls_ = 0;
        void AfterSubmission() override {
            ++calls_;
            THROW("debug must not submit workers");
        }
    } workers;
    const Dal::Detail::ScopedFixingReadObserver_ observeReads(&reads);
    const Dal::Script::Detail::ScopedSimulationObserver_ observeWorkers(&workers);
    const auto fixing = Dal::NewScriptProduct(
        "dump_fix", {Cell_(Date_(2026, 9, 11)), Cell_(Date_(2026, 9, 22))},
        {"x = FIX(EQ[DAL199_DUMP_PAST])", "IF 1 = 0 THEN payoff PAYS FIX(EQ[DAL199_DUMP_DEAD], 2026-09-11) ELSE payoff PAYS 0 END"});
    const String_ tree = Dal::DebugScriptProductTree(fixing, true);
    ASSERT_NE(tree.find("# 1 @ 2026-09-11 @ past"), String_::npos);
    ASSERT_NE(tree.find("FIX(EQ[DAL199_DUMP_PAST])"), String_::npos);
    ASSERT_NE(tree.find("FIX(EQ[DAL199_DUMP_DEAD], 2026-09-11)"), String_::npos);
    const String_ legacy = Dal::DebugScriptProduct(fixing);
    ASSERT_EQ(legacy.find("DAL199_DUMP_PAST"), String_::npos);
    ASSERT_NE(legacy.find("DAL199_DUMP_DEAD"), String_::npos);
    try {
        static_cast<void>(Dal::DebugScriptProductJson(fixing));
        FAIL() << "schema /1 must keep rejecting FIX";
    } catch (const Dal::ScriptError_& error) {
        ASSERT_NE(std::string(error.what()).find("DebugSchemaUnsupported"), std::string::npos);
    }

    const auto ordinary = Dal::NewScriptProduct("dump_branches", {Cell_(Date_(2026, 9, 11)), Cell_(Date_(2026, 9, 22))},
                                                {"x = 80", "IF 1 = 0 THEN payoff PAYS 111 ELSE payoff PAYS 222 END"});
    const String_ json = Dal::DebugScriptProductJson(ordinary);
    ASSERT_NE(json.find("\"kind\":\"if\""), String_::npos);
    ASSERT_NE(json.find("\"value\":111"), String_::npos);
    ASSERT_NE(json.find("\"value\":222"), String_::npos);
    ASSERT_EQ(reads.historyCalls_, 0);
    ASSERT_EQ(reads.fixingCalls_, 0);
    ASSERT_EQ(workers.calls_, 0);
}

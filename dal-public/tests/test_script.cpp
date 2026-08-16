//
// Created by dal-tester on 2026/8/15.
//

#include <gtest/gtest.h>

#include <dal/platform/platform.hpp>

#include <dal-public/src/global.hpp>
#include <dal-public/src/script.hpp>

using Dal::Cell_;
using Dal::Date_;
using Dal::String_;
using Dal::Vector_;

namespace {
    // events are parsed into the live (non-past) event list only when the
    // evaluation date precedes them; DebugScriptProduct only dumps live events
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

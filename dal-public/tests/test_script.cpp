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

//
// Created by wegam on 2026/5/30.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/script/preprocessor.hpp>
#include <dal/utilities/algorithms.hpp>

using namespace Dal;
using namespace Dal::Script;

namespace {
    Vector_<std::pair<Cell_, String_>> MakeTable(const Vector_<Cell_>& dates, const Vector_<String_>& events) {
        return Zip(dates, events);
    }
} // namespace

TEST(ScriptPreprocessorTest, TestConstVariableDefinition) {
    Vector_<Cell_> dates = {Cell_("STRIKE")};
    Vector_<String_> events = {"110.0"};

    Preprocessor_ preprocessor;
    auto result = preprocessor.Process(MakeTable(dates, events));

    ASSERT_EQ(result.constVariables_.size(), 1);
    ASSERT_NEAR(result.constVariables_["STRIKE"], 110.0, 1e-10);
    ASSERT_TRUE(result.events_.empty());
}

TEST(ScriptPreprocessorTest, TestMacroExpansionIntoDatedEvent) {
    Vector_<Cell_> dates = {Cell_("PAYOFF"), Cell_(Date_(2023, 12, 1))};
    Vector_<String_> events = {"MAX(spot() - 100.0, 0.0)", "call PAYS PAYOFF"};

    Preprocessor_ preprocessor;
    auto result = preprocessor.Process(MakeTable(dates, events));

    // The macro itself is not emitted as a constant variable.
    ASSERT_TRUE(result.constVariables_.empty());
    ASSERT_EQ(result.events_.size(), 1);

    const auto& statement = result.events_.at(Date_(2023, 12, 1));
    ASSERT_NE(statement.find("MAX(spot() - 100.0, 0.0)"), String_::npos);
    ASSERT_EQ(statement.find("PAYOFF"), String_::npos);
}

TEST(ScriptPreprocessorTest, TestConstVariableNotExpandedAsMacro) {
    // A const variable must stay symbolic in the event text; only its value is
    // recorded so the parser can fold it later.
    Vector_<Cell_> dates = {Cell_("STRIKE"), Cell_(Date_(2023, 12, 1))};
    Vector_<String_> events = {"110.0", "x = STRIKE"};

    Preprocessor_ preprocessor;
    auto result = preprocessor.Process(MakeTable(dates, events));

    ASSERT_NEAR(result.constVariables_["STRIKE"], 110.0, 1e-10);
    const auto& statement = result.events_.at(Date_(2023, 12, 1));
    ASSERT_NE(statement.find("STRIKE"), String_::npos);
}

TEST(ScriptPreprocessorTest, TestMultipleEventsSameDateConcatenated) {
    Vector_<Cell_> dates = {Cell_(Date_(2023, 12, 1)), Cell_(Date_(2023, 12, 1))};
    Vector_<String_> events = {"x = 1", "y = 2"};

    Preprocessor_ preprocessor;
    auto result = preprocessor.Process(MakeTable(dates, events));

    ASSERT_EQ(result.events_.size(), 1);
    const auto& statement = result.events_.at(Date_(2023, 12, 1));
    ASSERT_NE(statement.find("x = 1"), String_::npos);
    ASSERT_NE(statement.find("y = 2"), String_::npos);
    ASSERT_NE(statement.find("\n"), String_::npos);
}

TEST(ScriptPreprocessorTest, TestMacroNotAtFrontThrows) {
    Vector_<Cell_> dates = {Cell_(Date_(2023, 12, 1)), Cell_("STRIKE")};
    Vector_<String_> events = {"call PAYS MAX(spot() - STRIKE, 0.0)", "110.0"};

    Preprocessor_ preprocessor;
    ASSERT_THROW(static_cast<void>(preprocessor.Process(MakeTable(dates, events))), ScriptError_);
}

TEST(ScriptPreprocessorTest, TestDuplicateDefinitionThrows) {
    Vector_<Cell_> dates = {Cell_("STRIKE"), Cell_("STRIKE")};
    Vector_<String_> events = {"110.0", "120.0"};

    Preprocessor_ preprocessor;
    ASSERT_THROW(static_cast<void>(preprocessor.Process(MakeTable(dates, events))), ScriptError_);
}

TEST(ScriptPreprocessorTest, TestScheduleExpansion) {
    Vector_<Cell_> dates = {Cell_("START: 2022-05-07 END: 2023-05-07 FREQ: 1m CALENDAR: CN.SSE")};
    Vector_<String_> events = {"STRIKE = 110.0"};

    Preprocessor_ preprocessor;
    auto result = preprocessor.Process(MakeTable(dates, events));

    ASSERT_EQ(result.events_.size(), 12);
}

TEST(ScriptPreprocessorTest, TestSchedulePlaceholderReplacement) {
    Vector_<Cell_> dates = {Cell_("START: 2022-05-07 END: 2023-05-07 FREQ: 1m CALENDAR: CN.SSE")};
    Vector_<String_> events = {"acc = DCF(ACT365F, PeriodBegin, PeriodEnd)"};

    Preprocessor_ preprocessor;
    auto result = preprocessor.Process(MakeTable(dates, events));

    ASSERT_EQ(result.events_.size(), 12);
    for (const auto& dateEvent : result.events_) {
        ASSERT_EQ(dateEvent.second.find("PeriodBegin"), String_::npos);
        ASSERT_EQ(dateEvent.second.find("PeriodEnd"), String_::npos);
    }
}

namespace {
    // A preprocessor that refuses to classify any definition value as a const
    // variable, so even a numeric value is registered as a textual macro. The
    // base class records "0.05" as a const variable, so this override
    // demonstrably changes the classification Process() performs.
    class NoConstPreprocessor_ : public Preprocessor_ {
    protected:
        bool IsConstVariable(const String_&) const override { return false; }
    };
} // namespace

TEST(ScriptPreprocessorTest, TestExtensibilityViaOverride) {
    // "0.05" is numeric, so the base class would record RATE as a const variable
    // and leave the later event symbolic. The override forces RATE to be treated
    // as a macro instead: constVariables_ stays empty and RATE expands into the
    // dated event. Both assertions fail if the override is removed, so the test
    // genuinely verifies the extension point.
    Vector_<Cell_> dates = {Cell_("RATE"), Cell_(Date_(2023, 12, 1))};
    Vector_<String_> events = {"0.05", "x = RATE"};

    NoConstPreprocessor_ preprocessor;
    auto result = preprocessor.Process(MakeTable(dates, events));

    ASSERT_TRUE(result.constVariables_.empty());
    const auto& statement = result.events_.at(Date_(2023, 12, 1));
    ASSERT_NE(statement.find("0.05"), String_::npos);
    ASSERT_EQ(statement.find("RATE"), String_::npos);
}

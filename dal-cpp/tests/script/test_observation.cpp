//
// Created by Codex on 2026/9/12.
//

#include <gtest/gtest.h>

#include <dal/platform/platform.hpp>
#include <dal/indice/index.hpp>
#include <dal/script/event.hpp>
#include <dal/script/parser.hpp>
#include <dal/script/preprocessor.hpp>
#include <dal/script/visitor/all.hpp>

using namespace Dal;
using namespace Dal::Script;

namespace {
    class ObservationPreprocessor_ : public Preprocessor_ {
    public:
        using Preprocessor_::ExpandMacros;
        using Preprocessor_::ExpandSchedulePlaceholders;
    };
} // namespace

TEST(ScriptObservationTest, TestIndexLiteral) {
    Parser_ parser;
    for (const auto* text : {"x = FIX(EQ[AAPL])", "x = FIX(FX[EUR/USD])", "x = FIX(EQ[AAPL]>3M)", "x = FIX(EQ[AAPL]@2026-12-31, 2026-09-11)"}) {
        SCOPED_TRACE(text);
        ASSERT_NO_THROW(parser.Parse(text));
    }
}

TEST(ScriptObservationTest, TestProtectedExpansion) {
    ObservationPreprocessor_ preprocessor;
    const auto expanded = preprocessor.ExpandMacros("x = FIX(ASSET, PeriodBegin) + SCALE",
                                                    {{"ASSET", "EQ[PeriodBegin]>PeriodEnd"}, {"PeriodBegin", "CHANGED"}, {"SCALE", "2"}});
    ASSERT_EQ(expanded, "x = FIX(EQ[PeriodBegin]>PeriodEnd, CHANGED) + 2");
    const auto scheduled = preprocessor.ExpandSchedulePlaceholders("x = FIX(EQ[PeriodBegin], PeriodBegin) + FIX(EQ[AAPL]>PeriodEnd)",
                                                                   Date_(2026, 9, 11), Date_(2026, 9, 12));
    ASSERT_EQ(scheduled, "x = FIX(EQ[PeriodBegin], 2026-09-11) + FIX(EQ[AAPL]>PeriodEnd)");
    ASSERT_EQ(preprocessor.ExpandMacros("x = FIX(FX[EUR/USD]) + FIX(EQ[SCALE]@2026-12-31)", {{"USD", "JPY"}, {"SCALE", "BAD"}}),
              "x = FIX(FX[EUR/USD]) + FIX(EQ[SCALE]@2026-12-31)");
}

TEST(ScriptObservationTest, TestInvalidSyntax) {
    Parser_ parser;
    for (const auto* text : {"FIX()",
                             "FIX(EQ[AAPL],)",
                             "FIX(EQ[AAPL],2026-09-11,2026-09-12)",
                             "FIX(\"EQ[AAPL]\")",
                             "FIX('EQ[AAPL]')",
                             "FIX(EQ[AAPL)",
                             "FIX(EQ[AAPL]])",
                             "FIX(EQ[[AAPL]])",
                             "FIX(EQ[AAPL]junk)",
                             "FIX(FX[EUR/USD]junk)",
                             "FIX(EQ[])",
                             "FIX(XX[AAPL])",
                             "FIX(asset)",
                             "FIX(EQ[AAPL],date)",
                             "FIX(EQ[AAPL],2026-9-11)",
                             "FIX(EQ[AAPL],2026-02-30)",
                             "FIX(EQ[AAPL],2026-09-11+1)",
                             "FIX(EQ[AAPL],2026-09-11T00:00:00)",
                             "FIX(EQ[AAPL],2026 - 09 - 11)",
                             "SPOT(EQ[AAPL])",
                             "FIX(EQ[AAPL])$",
                             "(FIX(EQ[AAPL]) 123)",
                             "FIX(EQ[AAPL])[]"}) {
        SCOPED_TRACE(text);
        ASSERT_THROW(parser.Parse("x = " + String_(text)), ScriptError_);
    }
    ASSERT_NO_THROW(parser.Parse("x = SPOT()"));
}

TEST(ScriptObservationTest, TestReservedIdentifier) {
    for (const auto* statement : {"FIX = 1", "fix PAYS 2", "x = fix"}) {
        SCOPED_TRACE(statement);
        try {
            Parser_().Parse(statement);
            FAIL() << "FIX must be reserved";
        } catch (const ScriptError_& error) {
            ASSERT_NE(std::string(error.what()).find("ReservedIdentifier"), std::string::npos);
        }
    }
    for (const auto* value : {"2.0", "SPOT()"}) {
        try {
            static_cast<void>(Preprocessor_().Process({{Cell_("fIx"), String_(value)}}));
            FAIL() << "FIX macro must be reserved";
        } catch (const ScriptError_& error) {
            ASSERT_NE(std::string(error.what()).find("ReservedIdentifier"), std::string::npos);
            ASSERT_NE(std::string(error.what()).find("row=1"), std::string::npos);
        }
    }
}

TEST(ScriptObservationTest, TestLiteralIdentityDateAndSource) {
    auto event = Parser_().Parse("\n  x = FIX(eQ[Aapl]@2026-12-31, 2026-09-11)");
    const auto* node = dynamic_cast<const NodeFix_*>(event[0]->arguments_[1].get());
    ASSERT_NE(node, nullptr);
    ASSERT_EQ(std::string(node->literal_.raw_.begin(), node->literal_.raw_.end()), "eQ[Aapl]@2026-12-31");
    ASSERT_EQ(node->index_->Name(), "EQ[Aapl]@2026-12-31");
    ASSERT_TRUE(node->fixingDate_.has_value());
    ASSERT_EQ(*node->fixingDate_, Date_(2026, 9, 11));
    ASSERT_EQ(node->source_.offset_, 11);
    ASSERT_EQ(node->source_.line_, 2);
    ASSERT_EQ(node->source_.column_, 11);
    ASSERT_FALSE(node->isConst_);

    auto defaultDate = Parser_().Parse("x = FIX(FX[USD/EUR])");
    const auto* fx = dynamic_cast<const NodeFix_*>(defaultDate[0]->arguments_[1].get());
    ASSERT_NE(fx, nullptr);
    ASSERT_FALSE(fx->fixingDate_.has_value());
    ASSERT_EQ(fx->index_->Name(), "FX[USD/EUR]");
}

TEST(ScriptObservationTest, TestScheduleSourceContext) {
    for (const auto* index : {"UNKNOWN[AAPL]", "EQ[[AAPL]]"}) {
        try {
            const ScriptProduct_ product({Cell_("SCALE"), Cell_("START: 2026-09-11 END: 2026-09-12 FREQ: 1m CALENDAR: CN.SSE")},
                                         {"2.0", "x = FIX(" + String_(index) + ", PeriodBegin)"});
            FAIL() << "invalid index must fail with originating row and expanded date";
        } catch (const ScriptError_& error) {
            const std::string message(error.what());
            const char* expected = String_(index).substr(0, 2) == "EQ" ? "InvalidIndex" : "UnknownIndex";
            ASSERT_NE(message.find(expected), std::string::npos);
            ASSERT_NE(message.find("row=2"), std::string::npos);
            ASSERT_NE(message.find("event=2026-09-"), std::string::npos);
        }
    }
}

TEST(ScriptObservationTest, TestPreparationRequiredBeforeLegacyProcessing) {
    for (const auto date : {Date_(2020, 9, 11), Date_(2030, 9, 11)}) {
        for (const bool skipDomain : {false, true}) {
            ScriptProduct_ product({Cell_(date)}, {"IF 1 = 0 THEN x = FIX(EQ[AAPL]) ELSE x = 1 END"});
            try {
                product.PreProcess(false, skipDomain);
                FAIL() << "unprepared FIX must fail even in an unreachable branch";
            } catch (const ScriptError_& error) {
                ASSERT_NE(std::string(error.what()).find("PreparationRequired"), std::string::npos);
            }
        }
    }
}

namespace {
    template <class F_> void AssertPreparationRequired(F_ action) {
        try {
            action();
            FAIL() << "unprepared FIX must require preparation";
        } catch (const ScriptError_& error) {
            ASSERT_NE(std::string(error.what()).find("PreparationRequired"), std::string::npos);
            ASSERT_NE(std::string(error.what()).find("EQ[AAPL]"), std::string::npos);
        }
    }
} // namespace

TEST(ScriptObservationTest, TestPreparationRequiredVisitors) {
    auto event = Parser_().Parse("x = FIX(EQ[AAPL])");
    const auto& node = *event[0]->arguments_[1];
    Evaluator_<double> tree({});
    Evaluator_<AAD::Number_> aad({});
    PastEvaluator_<double> past({});
    FuzzyEvaluator_<double> fuzzy({}, {}, 0, 0.1);
    FuzzyEvaluator_<AAD::Number_> fuzzyAad({}, {}, 0, 0.1);
    Compiler_ compiler;
    Compiler_ fuzzyCompiler(true);
    AssertPreparationRequired([&] { node.Accept(tree); });
    AssertPreparationRequired([&] { node.Accept(aad); });
    AssertPreparationRequired([&] { node.Accept(past); });
    AssertPreparationRequired([&] { node.Accept(fuzzy); });
    AssertPreparationRequired([&] { node.Accept(fuzzyAad); });
    AssertPreparationRequired([&] { node.Accept(compiler); });
    AssertPreparationRequired([&] { node.Accept(fuzzyCompiler); });
}

TEST(ScriptObservationTest, TestPreparationRequiredProductEntryPoints) {
    const ScriptProduct_ product({Cell_(Date_(2030, 9, 11))}, {"IF 1 = 0 THEN x = FIX(EQ[AAPL]) ELSE x = 1 END"});
    AssertPreparationRequired([&] { static_cast<void>(product.Compile()); });
    AssertPreparationRequired([&] { static_cast<void>(product.PastEvaluate()); });
    auto evaluator = product.BuildEvaluator<double>();
    AssertPreparationRequired([&] { product.Evaluate(Scenario_<double>{}, evaluator); });
}

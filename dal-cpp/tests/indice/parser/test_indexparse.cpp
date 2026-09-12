//
// Created by wegam on 2026/7/19.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/indice/index.hpp>
#include <dal/indice/index/equity.hpp>
#include <dal/indice/index/fx.hpp>
#include <dal/indice/indexparse.hpp>
#include <dal/string/strings.hpp>

using namespace Dal;

TEST(IndexParseTest, TestParseDispatchesEquityForms) {
    {
        std::unique_ptr<Index_> index(Index::Parse(String_("EQ[IBM]")));
        ASSERT_EQ(index->Name(), String_("EQ[IBM]"));
    }
    {
        std::unique_ptr<Index_> index(Index::Parse(String_("EQ[IBM]@2022-01-24")));
        ASSERT_EQ(index->Name(), String_("EQ[IBM]@2022-01-24"));
    }
    {
        std::unique_ptr<Index_> index(Index::Parse(String_("EQ[IBM]>3M")));
        ASSERT_EQ(index->Name(), String_("EQ[IBM]>3M"));
    }
}

TEST(IndexParseTest, TestParseDispatchesFxForm) {
    std::unique_ptr<Index_> index(Index::Parse(String_("FX[EUR/GBP]")));
    ASSERT_EQ(index->Name(), String_("FX[EUR/GBP]"));
}

TEST(IndexParseTest, TestParseRejectsUnknownPrefix) {
    ASSERT_THROW((void)Index::Parse(String_("XX[ABC]")), Dal::Exception_);
    ASSERT_THROW((void)Index::Parse(String_("IR:USD,3M")), Dal::Exception_);
}

TEST(IndexParseTest, TestParseRejectsBareNameAndNullParser) {
    ASSERT_THROW(Index::Parse("IBM"), Dal::Exception_);
    ASSERT_THROW(Index::Parse(""), Dal::Exception_);
    Index::RegisterParser("NULL_TEST", [](const String_&) -> std::unique_ptr<Index_> { return nullptr; });
    ASSERT_THROW(Index::Parse("NULL_TEST[ABC]"), Dal::Exception_);
}

TEST(IndexParseTest, TestParseRejectsMalformedAndTrailingInput) {
    for (const auto* text : {"EQ[]", "EQ[AAPL", "EQ[[AAPL]]", "EQ[AAPL]]", "EQ[AAPL]junk", "EQ[AAPL]junk>3M", "EQ[AAPL]@2026-12-31junk", "EQ[AAPL]>",
                             "EQ[AAPL]>3Mjunk", "EQ[AAPL]@", "EQ[\"AAPL\"]", "FX[EUR/USD]junk", "FX[[EUR/USD]]", "FX[EUR/USD]]", "FX[/USD]",
                             "FX[EUR/]", "FX[EUR/USD/JPY]", "FX[EUR/USD]>3M"}) {
        SCOPED_TRACE(text);
        ASSERT_THROW(Index::Parse(text), Dal::Exception_);
    }
}

TEST(IndexParseTest, TestCloneRoundTripsThroughName) {
    std::unique_ptr<Index_> index(Index::Parse(String_("FX[USD/JPY]")));
    const Handle_<Index_> cloned = Index::Clone(*index);
    ASSERT_FALSE(cloned.IsEmpty());
    ASSERT_EQ(cloned->Name(), String_("FX[USD/JPY]"));
}

TEST(IndexParseTest, TestRegisterParserAddsNewPrefix) {
    Index::RegisterParser(String_("TST"), [](const String_&) -> std::unique_ptr<Index_> {
        return std::make_unique<Index::Fx_>(Ccy_("USD"), Ccy_("EUR"));
    });
    std::unique_ptr<Index_> index(Index::Parse(String_("TST[ANYTHING]")));
    ASSERT_EQ(index->Name(), String_("FX[EUR/USD]"));
}

TEST(IndexParseTest, TestRegisterParserKeepsFirstRegistration) {
    // std::map::insert does not overwrite, so re-registering an existing prefix is a no-op
    Index::RegisterParser(String_("FX"), [](const String_&) -> std::unique_ptr<Index_> {
        return std::make_unique<Index::Fx_>(Ccy_("USD"), Ccy_("EUR"));
    });
    std::unique_ptr<Index_> index(Index::Parse(String_("FX[USD/JPY]")));
    ASSERT_EQ(index->Name(), String_("FX[USD/JPY]"));
}

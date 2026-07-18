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

TEST(IndexParseTest, TestParseBareNameReturnsNull) {
    // bare names route to the (not yet supported) supershot parser, which yields null
    std::unique_ptr<Index_> index(Index::Parse(String_("IBM")));
    ASSERT_TRUE(index == nullptr);
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

//
// Created by wegam on 2023/1/24.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/indice/index.hpp>
#include <dal/indice/indexparse.hpp>
#include <dal/string/strings.hpp>
#include <dal/indice/parser/fx.hpp>

using namespace Dal;

TEST(IndexTest, TestFxParser) {
    String_ name = "FX[USD/JPY]";
    std::unique_ptr<Index_> index(Index::FxParser(name));
    ASSERT_EQ(index->Name(), name);
}

TEST(IndexTest, TestFxParserRegisteredWithIndexParse) {
    // The parser must be reachable via Index::Parse, not only by calling
    // Index::FxParser directly. Without registration Index::Parse falls through
    // to a REQUIRE failure ("no parser for 'FX[USD/JPY]'").
    String_ name = "FX[USD/JPY]";
    std::unique_ptr<Index_> index(Index::Parse(name));
    ASSERT_EQ(index->Name(), name);
}

TEST(IndexTest, TestFxParserRejectsMissingOpenBracket) {
    ASSERT_THROW((void)Index::FxParser(String_("FXUSD/JPY]")), Dal::Exception_);
}

TEST(IndexTest, TestFxParserRejectsMissingCloseBracket) {
    ASSERT_THROW((void)Index::FxParser(String_("FX[USD/JPY")), Dal::Exception_);
}

TEST(IndexTest, TestFxParserRejectsMissingSeparator) {
    ASSERT_THROW((void)Index::FxParser(String_("FX[USDJPY]")), Dal::Exception_);
}

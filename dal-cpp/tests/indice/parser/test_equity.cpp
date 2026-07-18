//
// Created by wegam on 2022/1/24.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/string/strings.hpp>
#include <dal/indice/index.hpp>
#include <dal/indice/index/equity.hpp>
#include <dal/indice/parser/equity.hpp>
#include <dal/time/datetime.hpp>

using namespace Dal;

TEST(IndexTest, TestParserWithEmptyDelivery) {
    String_ name = "EQ[IBM]";
    std::unique_ptr<Index_> index(Index::EquityParser(name));
    ASSERT_EQ(index->Name(), name);
}

TEST(IndexTest, TestParserWithDeliveryDate) {
    String_ name = "EQ[IBM]@2022-01-24";
    std::unique_ptr<Index_> index(Index::EquityParser(name));
    ASSERT_EQ(index->Name(), name);
}

TEST(IndexTest, TestParserWithTenor) {
    String_ name = "EQ[IBM]>3M";
    std::unique_ptr<Index_> index(Index::EquityParser(name));
    ASSERT_EQ(index->Name(), name);
}

TEST(IndexTest, TestParserWithTenorVariants) {
    for (const char* name : {"EQ[IBM]>1W", "EQ[IBM]>6M", "EQ[IBM]>2Y"}) {
        std::unique_ptr<Index_> index(Index::EquityParser(String_(name)));
        ASSERT_EQ(index->Name(), String_(name));
    }
}

TEST(IndexTest, TestParserRejectsMalformedDeliveryDate) {
    ASSERT_THROW((void)Index::EquityParser(String_("EQ[IBM]@not-a-date")), Dal::Exception_);
}

TEST(IndexTest, TestParserDeliveryOfParsedForms) {
    const DateTime_ fixing_time(Date_(2022, 1, 22));

    std::unique_ptr<Index_> with_date(Index::EquityParser(String_("EQ[IBM]@2022-01-24")));
    auto eq_with_date = dynamic_cast<const Index::Equity_*>(with_date.get());
    ASSERT_TRUE(eq_with_date != nullptr);
    ASSERT_EQ(eq_with_date->Delivery(fixing_time), Date_(2022, 1, 24));

    std::unique_ptr<Index_> with_delay(Index::EquityParser(String_("EQ[IBM]>3M")));
    auto eq_with_delay = dynamic_cast<const Index::Equity_*>(with_delay.get());
    ASSERT_TRUE(eq_with_delay != nullptr);
    ASSERT_EQ(eq_with_delay->Delivery(fixing_time), Date_(2022, 4, 22));
}

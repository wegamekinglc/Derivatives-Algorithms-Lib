//
// Created by wegam on 2022/1/24.
//

#include <gtest/gtest.h>
#include <dal/string/strings.hpp>
#include <dal/indice/index.hpp>
#include <dal/indice/parser/equity.hpp>

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
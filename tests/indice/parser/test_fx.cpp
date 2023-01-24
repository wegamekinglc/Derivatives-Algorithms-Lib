//
// Created by wegam on 2023/1/24.
//

#include <gtest/gtest.h>
#include <dal/indice/index.hpp>
#include <dal/string/strings.hpp>
#include <dal/indice/parser/fx.hpp>

using namespace Dal;

TEST(FxParserTest, TestFxParser) {
    String_ name = "FX[USD/JPY]";
    std::unique_ptr<Index_> index(Index::FxParser(name));
    ASSERT_EQ(index->Name(), name);
}

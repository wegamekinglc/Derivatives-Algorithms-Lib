//
// Created by wegam on 2022/1/22.
//

#include <gtest/gtest.h>
#include "dal/indice/index/indexequity.hpp"

using namespace Dal;

TEST(IndexEquityTest, TestEquityName) {
    String_ delay("3M");
    Index::Equity_ eq("HS300", nullptr, &delay);
    ASSERT_EQ(String_("EQ[HS300]>3M"), eq.Name());

    Date_ date(2022, 1, 22);
    Index::Equity_ eq2("HS300", &date);
    ASSERT_EQ(String_("EQ[HS300]@2022-01-22"), eq2.Name());
}
//
// Created by wegam on 2022/2/3.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/indice/index/ir.hpp>

using namespace Dal;

TEST(IndexTest, TestLiborName) {
    Ccy_ ccy("USD");
    TradedRate_ tenor("LIBOR3MLCH");
    Index::Libor_ libor(ccy, tenor);
    ASSERT_EQ(libor.Name(), "IR:USD,LIBOR_3M_LCH");
}

TEST(IndexTest, TestLiborNameWithStart) {
    Ccy_ ccy("USD");
    TradedRate_ tenor("LIBOR3MLCH");
    Cell_ start(Date_(2022, 2, 3));
    Index::Libor_ libor(ccy, tenor, start);
    ASSERT_EQ(libor.Name(), "IR:USD,LIBOR_3M_LCH,2022-02-03");
}

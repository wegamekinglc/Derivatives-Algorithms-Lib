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

TEST(IndexTest, TestSwapName) {
    Index::Swap_ swap(Ccy_("USD"), "10Y");
    ASSERT_EQ(swap.Name(), "IR:USD,10Y");
}

TEST(IndexTest, TestSwapNameWithStart) {
    Cell_ start(Date_(2022, 2, 3));
    Index::Swap_ swap(Ccy_("USD"), "10Y", start);
    ASSERT_EQ(swap.Name(), "IR:USD,10Y,2022-02-03");
}

TEST(IndexTest, TestDFName) {
    {
        Index::DF_ df(Ccy_("USD"), Cell_(Date_(2023, 6, 15)));
        ASSERT_EQ(df.Name(), "IR[DF]:USD,2023-06-15");
    }
    {
        Index::DF_ df(Ccy_("USD"), Cell_(String_("3M")));
        ASSERT_EQ(df.Name(), "IR[DF]:USD,3M");
    }
    {
        Cell_ start(Date_(2022, 1, 3));
        Index::DF_ df(Ccy_("USD"), Cell_(Date_(2023, 6, 15)), &start);
        ASSERT_EQ(df.Name(), "IR[DF]:USD,2022-01-03,2023-06-15");
    }
}

TEST(IndexTest, TestDFStartAndMaturityFromCellForms) {
    const DateTime_ fixing_time(Date_(2022, 2, 2));

    {
        // empty start defaults to the fixing date
        Index::DF_ df(Ccy_("USD"), Cell::FromInt(30));
        ASSERT_EQ(df.StartDate(fixing_time), Date_(2022, 2, 2));
        ASSERT_EQ(df.Maturity(fixing_time), Date_(2022, 3, 4));
    }
    {
        // date cells are used verbatim
        Cell_ start(Date_(2022, 1, 15));
        Index::DF_ df(Ccy_("USD"), Cell_(Date_(2023, 6, 15)), &start);
        ASSERT_EQ(df.StartDate(fixing_time), Date_(2022, 1, 15));
        ASSERT_EQ(df.Maturity(fixing_time), Date_(2023, 6, 15));
    }
    {
        // string cells are date increments forward of the fixing date
        Index::DF_ df(Ccy_("USD"), Cell_(String_("1W")));
        ASSERT_EQ(df.Maturity(fixing_time), Date_(2022, 2, 9));
    }
}

TEST(IndexTest, TestLiborStartDateAppliesFixingLag) {
    Ccy_ ccy("USD");
    TradedRate_ tenor("LIBOR3MLCH");
    Index::Libor_ libor(ccy, tenor);

    {
        // default conventions: two business-day lag, no holiday calendar
        const DateTime_ fixing_time(Date_(2022, 2, 2)); // Wednesday
        ASSERT_EQ(libor.StartDate(fixing_time), Date_(2022, 2, 4));
    }
    {
        // the lag rolls over the weekend
        const DateTime_ fixing_time(Date_(2022, 2, 4)); // Friday
        ASSERT_EQ(libor.StartDate(fixing_time), Date_(2022, 2, 8));
    }
    {
        // an explicit date start bypasses the fixing-lag roll, even on a weekend
        Cell_ start(Date_(2022, 2, 5)); // Saturday
        Index::Libor_ libor_with_start(ccy, tenor, start);
        const DateTime_ fixing_time(Date_(2022, 2, 2));
        ASSERT_EQ(libor_with_start.StartDate(fixing_time), Date_(2022, 2, 5));
    }
}

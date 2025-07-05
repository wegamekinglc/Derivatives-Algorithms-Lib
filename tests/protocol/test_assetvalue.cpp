//
// Created by wegam on 2024/1/18.
//

#include <dal/protocol/assetvalue.hpp>

#include <dal/platform/platform.hpp>
#include <gtest/gtest.h>

using namespace Dal;

TEST(ProtocolTest, TestUpdateTokenValue) {
    Vector_<> value_pool = {0.0, 1.0, 2.0, 3.0};
    Vector_<Handle_<IndexPath_>> index_path_pool;
    const DateTime_ date(Date_(2024, 1, 18), 0.5);
    const UpdateToken_ token(value_pool.begin(), index_path_pool.begin(), date);

    ASSERT_NEAR(token[1], value_pool[1], 1e-8);
    ASSERT_NEAR(token[3], value_pool[3], 1e-8);
}

TEST(ProtocolTest, TestUpdateTokenValueDateMask) {
    const Valuation::address_t loc = 0b0000000000000000111111111100000000000000000000000000000000000011;
    Vector_<> value_pool = {0.0, 1.0, 2.0, 3.0};
    Vector_<Handle_<IndexPath_>> index_path_pool;
    const DateTime_ date(Date_(2024, 1, 18), 0.5);
    const UpdateToken_ token(value_pool.begin(), index_path_pool.begin(), date);

    ASSERT_NEAR(token[loc], value_pool[3], 1e-8);
    ASSERT_EQ(token.LocalTime(loc), 1023);
}


TEST(ProtocolTest, TestUpdateTokenIndexPath) {
    Vector_<> value_pool;;
    Vector_<Handle_<IndexPath_>> index_path_pool;

    std::map<DateTime_, double> fixings;
    const DateTime_ dt(Date_(2024, 1, 18), 0.5);
    fixings[dt] = 1.0;

    Handle_<IndexPath_> index_path(new IndexPathHistorical_(fixings));
    index_path_pool.emplace_back(index_path);
    const DateTime_ date(Date_(2024, 1, 18), 0.5);
    UpdateToken_ token(value_pool.begin(), index_path_pool.begin(), date);

    ASSERT_NEAR(token.Index(Valuation::IndexAddress_(0)).FixInRangeProb(dt, std::make_pair<double, double>(0.0, 2.0), 0.0), 1.0, 1e-4);
    ASSERT_NEAR(token.Index(Valuation::IndexAddress_(0)).FixInRangeProb(dt, std::make_pair<double, double>(3.0, 4.0), 0.0), 0.0, 1e-4);
}


TEST(ProtocolTest, TestUpdateTokenIndexPathDateMask) {
    Vector_<> value_pool;;
    Vector_<Handle_<IndexPath_>> index_path_pool;

    std::map<DateTime_, double> fixings;
    const DateTime_ dt(Date_(2024, 1, 18), 0.5);
    fixings[dt] = 1.0;

    Handle_<IndexPath_> index_path(new IndexPathHistorical_(fixings));
    index_path_pool.emplace_back(index_path);
    const DateTime_ date(Date_(2024, 1, 18), 0.5);
    UpdateToken_ token(value_pool.begin(), index_path_pool.begin(), date);

    const Valuation::IndexAddress_ loc{0b0000000000000000111111111100000000000000000000000000000000000000};

    ASSERT_NEAR(token.Index(loc).FixInRangeProb(dt, std::make_pair<double, double>(0.0, 2.0), 0.0), 1.0, 1e-4);
    ASSERT_NEAR(token.Index(loc).FixInRangeProb(dt, std::make_pair<double, double>(3.0, 4.0), 0.0), 0.0, 1e-4);
    ASSERT_EQ(token.LocalTime(loc), 1023);
}

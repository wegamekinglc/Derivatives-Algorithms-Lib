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
    UpdateToken_ token(value_pool.begin(), index_path_pool.begin(), date);

    ASSERT_NEAR(token[1], value_pool[1], 1e-8);
    ASSERT_NEAR(token[3], value_pool[3], 1e-8);
}

TEST(ProtocolTest, TestUpdateTokenIndexPath) {

}


//
// Created by wegam on 2024/8/29.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/model/dupire.hpp>
#include <dal/storage/json.hpp>

using namespace Dal;

TEST(ModelTest, TestDupireModelData) {
    auto model_data = DupireModelData_("my_model", 100.0, 0.05, 0.01, Vector_<>(1), Vector_<>(1), Matrix_<>(1, 1));
    auto dst = JSON::WriteString(model_data);

    Handle_<Storable_> rtn = JSON::ReadString(dst, true);
    ASSERT_NEAR(std::dynamic_pointer_cast<const DupireModelData_>(rtn)->spot_, 100.0, 1e-8);
    ASSERT_NEAR(std::dynamic_pointer_cast<const DupireModelData_>(rtn)->rate_, 0.05, 1e-8);
}

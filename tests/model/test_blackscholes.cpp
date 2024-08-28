//
// Created by wegam on 2024/8/29.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/storage/json.hpp>

using namespace Dal;
using Dal::AAD::BSModelData_;

TEST(ModelTest, TestBlackScholesModelData) {
    auto model_data = BSModelData_("my_model", 100.0, 0.20, 0.05, 0.01);
    auto dst = JSON::WriteString(model_data);

    Handle_<Storable_> rtn = JSON::ReadString(dst, true);
    ASSERT_NEAR(std::dynamic_pointer_cast<const AAD::BSModelData_>(rtn)->spot_, 100.0, 1e-8);
    ASSERT_NEAR(std::dynamic_pointer_cast<const AAD::BSModelData_>(rtn)->vol_, 0.20, 1e-8);
}

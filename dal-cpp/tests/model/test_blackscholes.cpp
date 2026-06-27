//
// Created by wegam on 2024/8/29.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/storage/json.hpp>

using namespace Dal;
using Dal::BSModelData_;

TEST(ModelTest, TestBlackScholesModelData) {
    auto model_data = BSModelData_("my_model", 100.0, 0.20, 0.05, 0.01);
    auto dst = JSON::WriteString(model_data);

    Handle_<Storable_> rtn = JSON::ReadString(dst, true);
    ASSERT_NEAR(std::dynamic_pointer_cast<const BSModelData_>(rtn)->spot_, 100.0, 1e-8);
    ASSERT_NEAR(std::dynamic_pointer_cast<const BSModelData_>(rtn)->vol_, 0.20, 1e-8);
}

TEST(ModelTest, TestBlackScholesMutantModelRename) {
    BSModelData_ model_data("my_model", 100.0, 0.20, 0.05, 0.01);
    ModelData_& base = model_data;
    Vector_<Handle_<Slide_>> noSlides;
    std::unique_ptr<ModelData_> renamed(base.MutantModel(String_("renamed"), noSlides));
    auto bs = dynamic_cast<const BSModelData_*>(renamed.get());
    ASSERT_TRUE(bs != nullptr);
    ASSERT_EQ(bs->Name(), String_("renamed"));
    ASSERT_NEAR(bs->spot_, 100.0, 1e-10);
    ASSERT_NEAR(bs->vol_, 0.20, 1e-10);
    ASSERT_NEAR(bs->rate_, 0.05, 1e-10);
    ASSERT_NEAR(bs->div_, 0.01, 1e-10);
}

TEST(ModelTest, TestBlackScholesMutantModelRejectsSlides) {
    BSModelData_ model_data("my_model", 100.0, 0.20, 0.05, 0.01);
    ModelData_& base = model_data;
    Vector_<Handle_<Slide_>> slides{Handle_<Slide_>(std::make_shared<Slide_>())};
    ASSERT_THROW((void)base.MutantModel(String_("renamed"), slides), Dal::Exception_);
}

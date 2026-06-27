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

TEST(ModelTest, TestDupireMutantModelRename) {
    Vector_<> spots = {90.0, 100.0, 110.0};
    Vector_<> times = {0.25, 0.5, 1.0};
    Matrix_<> vols(3, 3, 0.2);
    DupireModelData_ model_data("my_model", 100.0, 0.05, 0.01, spots, times, vols);
    ModelData_& base = model_data;
    Vector_<Handle_<Slide_>> noSlides;
    std::unique_ptr<ModelData_> renamed(base.MutantModel(String_("renamed"), noSlides));
    auto dup = dynamic_cast<const DupireModelData_*>(renamed.get());
    ASSERT_TRUE(dup != nullptr);
    ASSERT_EQ(dup->Name(), String_("renamed"));
    ASSERT_NEAR(dup->spot_, 100.0, 1e-10);
    ASSERT_NEAR(dup->rate_, 0.05, 1e-10);
    ASSERT_NEAR(dup->repo_, 0.01, 1e-10);
    ASSERT_EQ(dup->spots_, spots);
    ASSERT_EQ(dup->times_, times);
    ASSERT_EQ(dup->vols_.Rows(), vols.Rows());
    ASSERT_EQ(dup->vols_.Cols(), vols.Cols());
    ASSERT_NEAR(dup->vols_(0, 0), 0.2, 1e-10);
}

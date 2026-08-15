//
// Created by dal-tester on 2026/8/15.
//

#include <gtest/gtest.h>

#include <dal-public/src/models.hpp>

using Dal::Matrix_;
using Dal::String_;
using Dal::Vector_;

TEST(ModelsTest, TestNewBSModelDataStoresTypeAndName) {
    const auto model = Dal::NewBSModelData(String_("dal_public_bs_model"), 100.0, 0.2, 0.05, 0.02);

    ASSERT_FALSE(model.IsEmpty());
    ASSERT_TRUE(model->Type() == "BSModelData_");
    ASSERT_TRUE(model->Name() == "dal_public_bs_model");
}

TEST(ModelsTest, TestNewDupireModelDataStoresTypeAndName) {
    const Vector_<> spots = {80.0, 100.0, 120.0};
    const Vector_<> times = {0.5, 1.0};
    Matrix_<> vols(2, 3);
    vols(0, 0) = 0.22;
    vols(0, 1) = 0.20;
    vols(0, 2) = 0.18;
    vols(1, 0) = 0.24;
    vols(1, 1) = 0.21;
    vols(1, 2) = 0.19;

    const auto model = Dal::NewDupireModelData(String_("dal_public_dupire_model"), 100.0, 0.05, 0.02, spots, times, vols);

    ASSERT_FALSE(model.IsEmpty());
    ASSERT_TRUE(model->Type() == "DupireModelData_");
    ASSERT_TRUE(model->Name() == "dal_public_dupire_model");
}

TEST(ModelsTest, TestNewBSModelDataUsableByModelFactory) {
    // the facade handle must carry everything the core model factory needs
    const auto model = Dal::NewBSModelData(String_("dal_public_bs_labels"), 100.0, 0.2, 0.05, 0.02);

    ASSERT_EQ(model->parameterLabels_.size(), 4);
    ASSERT_TRUE(model->parameterLabels_[0] == "spot");
    ASSERT_TRUE(model->parameterLabels_[1] == "vol");
    ASSERT_TRUE(model->parameterLabels_[2] == "rate");
    ASSERT_TRUE(model->parameterLabels_[3] == "div");
}

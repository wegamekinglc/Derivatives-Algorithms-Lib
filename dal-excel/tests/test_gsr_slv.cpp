//
// Created by Codex on 2026/10/2.
//

#include <gtest/gtest.h>
#include <dal-excel/src/__models_test_api.hpp>
#include <dal/storage/splat.hpp>

using namespace Dal;

TEST(ExcelGSRSLVTest, TestFactoriesSettingsAndArchive) {
    const Date_ today(2026, 10, 2);
    const auto curve = NewGSRCurveData("curve", today, "USD", {today, today.AddDays(1095)}, {0.0, -0.09}, {}, Matrix_<>(0, 0));
    MultiFactorGSRVolSettings_ vol;
    vol.factorNames_ = {"level"};
    vol.gKnotDates_ = vol.hKnotDates_ = {today};
    vol.gValues_ = Matrix_<>(1, 1, 0.02);
    vol.hValues_ = vol.correlations_ = Matrix_<>(1, 1, 1.0);
    const auto gaussian = NewMultiFactorGSRModelData("rates", curve, NewMultiFactorGSRVolData("vol", vol));
    Handle_<GSRLeverageData_> leverage;
    GSRLeverageData_New("leverage", {-0.02, 0.02}, {0.0}, Matrix_<>(2, 1, 1.0), &leverage);
    Matrix_<Cell_> settings(3, 2);
    settings(0, 0) = Cell_("kappa");
    settings(0, 1) = Cell_(0.7);
    settings(1, 0) = Cell_("volOfVol");
    settings(1, 1) = Cell_(0.4);
    settings(2, 0) = Cell_("maxStep");
    settings(2, 1) = Cell_(0.125);
    Handle_<ModelData_> model;
    GSRSLVModelData_New("smile", gaussian, leverage, {0.3}, settings, &model);
    const auto restored = handle_cast<GSRSLVModelData_>(UnSplat(Splat(*model), true));
    ASSERT_TRUE(restored);
    ASSERT_DOUBLE_EQ(restored->kappa_, 0.7);
    ASSERT_DOUBLE_EQ(restored->maxStep_, 0.125);
    ASSERT_DOUBLE_EQ(restored->varianceCorrelations_[0], 0.3);
    settings(2, 0) = Cell_("kappa");
    ASSERT_THROW(GSRSLVModelData_New("bad", gaussian, leverage, {}, settings, &model), Exception_);
    settings(2, 0) = Cell_("unknown");
    ASSERT_THROW(GSRSLVModelData_New("bad", gaussian, leverage, {}, settings, &model), Exception_);
}

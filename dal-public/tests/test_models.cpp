//
// Created by dal-tester on 2026/8/15.
//

#include <gtest/gtest.h>

#include <cmath>
#include <string>

#include <dal-public/src/models.hpp>
#include <dal/curve/yclogdf.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/model/factory.hpp>

using Dal::Matrix_;
using Dal::String_;
using Dal::Vector_;

TEST(ModelsTest, TestNewBSModelDataStoresTypeAndName) {
    const auto model = Dal::NewBSModelData(String_("dal_public_bs_model"), 100.0, 0.2, 0.05, 0.02);

    ASSERT_FALSE(model.IsEmpty());
    ASSERT_TRUE(model->Type() == "BSModelData_");
    ASSERT_TRUE(model->Name() == "dal_public_bs_model");
}

TEST(ModelsTest, TestNewBSLocalVolModelDataStoresHybridTypeAndName) {
    const Vector_<> spots = {80.0, 100.0, 120.0};
    const Vector_<> times = {0.5, 1.0};
    Matrix_<> vols(3, 2);
    vols(0, 0) = 0.22;
    vols(0, 1) = 0.20;
    vols(1, 0) = 0.24;
    vols(1, 1) = 0.21;
    vols(2, 0) = 0.19;
    vols(2, 1) = 0.18;

    const auto surface = Dal::NewLocalVolSurfaceData("surface", spots, times, vols);
    const auto bs = Dal::BSModelData_("bs", 100.0, 0.20, 0.05, 0.02);
    const auto model = Dal::NewBSLocalVolModelData("dal_public_local_vol", "EQ[A]", "USD", "W_EQ", bs, surface);

    ASSERT_FALSE(model.IsEmpty());
    ASSERT_TRUE(model->Type() == "HybridModelData_");
    ASSERT_TRUE(model->Name() == "dal_public_local_vol");
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

TEST(ModelsTest, TestNewCorrelatedBSModelDataStoresOrderedAssets) {
    Dal::CorrelatedBSSettings_ settings;
    settings.assets_ = {{"eq[AAA]", 100.0, 0.2, 0.01}, {"EQ[BBB]", 120.0, 0.3, 0.02}};
    settings.rate_ = 0.05;
    settings.correlations_ = Matrix_<>(2, 2, 0.0);
    settings.correlations_(0, 0) = settings.correlations_(1, 1) = 1.0;
    settings.correlations_(0, 1) = settings.correlations_(1, 0) = -0.3;
    const auto model = Dal::NewCorrelatedBSModelData("basket", settings);
    ASSERT_EQ(model->Type(), String_("CorrelatedBSModelData_"));
    const auto* data = dynamic_cast<const Dal::CorrelatedBSModelData_*>(model.get());
    ASSERT_NE(data, nullptr);
    ASSERT_EQ(data->indices_, (Vector_<String_>{"EQ[AAA]", "EQ[BBB]"}));
    ASSERT_EQ(std::string(data->indices_[0].c_str()), "EQ[AAA]");
    ASSERT_EQ(data->parameterLabels_,
              (Vector_<String_>{"spot:EQ[AAA]", "vol:EQ[AAA]", "div:EQ[AAA]", "spot:EQ[BBB]", "vol:EQ[BBB]", "div:EQ[BBB]", "rate"}));
}

TEST(ModelsTest, TestNewHybridModelDataStoresTypedComponents) {
    Dal::HybridSettings_ settings;
    settings.domesticCurrency_ = "USD";
    settings.components_ = {
        Dal::Handle_<Dal::HybridComponentData_>(new Dal::HybridDeterministicRateData_("usd", "USD", 0.05)),
        Dal::Handle_<Dal::HybridComponentData_>(new Dal::HybridBSEquityData_("aaa", "eq[AAA]", "USD", "W_AAA", 100.0, 0.2, 0.01)),
    };
    settings.correlation_ =
        Dal::Handle_<Dal::HybridCorrelationData_>(new Dal::HybridConstantCorrelationData_("corr", {"W_AAA"}, Matrix_<>(1, 1, 1.0)));
    const auto model = Dal::NewHybridModelData("hybrid", settings);
    ASSERT_EQ(model->Type(), String_("HybridModelData_"));
    const auto* data = dynamic_cast<const Dal::HybridModelData_*>(model.get());
    ASSERT_NE(data, nullptr);
    ASSERT_EQ(data->components_.size(), 2);
    ASSERT_EQ(data->parameterLabels_, (Vector_<String_>{"spot:EQ[AAA]", "vol:EQ[AAA]", "div:EQ[AAA]", "rate:USD"}));
}

TEST(ModelsTest, TestHybridLogDfRateSnapshotRebasesDatedCurveToModelTime) {
    const Dal::Date_ anchor(2026, 3, 27);
    const Dal::Date_ today(2026, 9, 27);
    const Dal::Date_ first(2027, 3, 27);
    const Dal::Date_ last(2027, 9, 27);
    const Vector_<Dal::Date_> curveDates{anchor, today, first, last};
    Vector_<> curveLogDF;
    for (const auto& date : curveDates)
        curveLogDF.push_back(-0.04 * (date - anchor) / 360.0);
    const Dal::DiscountLogDF_ curve("calibrated", "USD", curveDates, curveLogDF, Dal::DayBasis_("ACT_360"), Dal::LogDfScheme_::Value_::LOG_LINEAR);
    const auto component = Dal::NewHybridLogDfRateDataFromCurve("usd", curve, today, {today, first, last});
    const auto* data = dynamic_cast<const Dal::HybridLogDfRateData_*>(component.get());
    ASSERT_NE(data, nullptr);
    ASSERT_EQ(data->currency_, String_("USD"));
    ASSERT_EQ(data->times_, (Vector_<>{0.0, (first - today) / 365.0, 1.0}));
    ASSERT_NEAR(data->logDF_[1], -0.04 * (first - today) / 360.0, 1e-12);
    ASSERT_NEAR(data->logDF_[2], -0.04 * (last - today) / 360.0, 1e-12);
    ASSERT_THROW(Dal::NewHybridLogDfRateDataFromCurve("usd", curve, today, {first, last}), Dal::Exception_);
    ASSERT_THROW(Dal::NewHybridLogDfRateDataFromCurve("usd", curve, today, {today, last, first}), Dal::Exception_);
}

TEST(ModelsTest, TestGsrFactoriesPreserveCurveNodesAndModelParameters) {
    const Dal::Date_ today(2026, 9, 28);
    const Dal::Date_ year(2027, 9, 28);
    const auto curve = Dal::NewGSRCurveData("curve", today, "USD", {today, year}, {0.0, -0.03}, {}, Matrix_<>(0, 0));
    const auto vol = Dal::NewGSRVolData("vol", {today}, {0.01}, {today}, {1.0});
    const auto data = Dal::NewGSRModelData("gsr", curve, vol);
    const auto model = Dal::CreateModel<double>(data);
    ASSERT_EQ(model->NumParams(), 3);
    ASSERT_EQ(model->ParameterLabels()[0], String_("logdf:OIS:2027-09-28"));
}

TEST(ModelsTest, TestGsrCurveSnapshotUsesYieldCurveAtSelectedDates) {
    const Dal::Date_ today(2026, 9, 28);
    const Dal::Date_ year(2027, 9, 28);
    const Dal::DiscountLogDF_ discount("source", "USD", {today, year}, {0.0, -0.03}, Dal::DayBasis::Act365F(),
                                       Dal::LogDfScheme_::Value_::LOG_LINEAR);
    const Dal::CurveBlock_ source(discount);
    const auto snapshot = Dal::NewGSRCurveDataFromYieldCurve("curve", source, today, {today, year}, {});
    ASSERT_NEAR(snapshot->discountLogDF_[1], -0.03, 1e-12);
}

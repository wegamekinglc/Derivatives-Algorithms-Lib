//
// Created by Codex on 2026/9/30.
//

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include <dal/curve/tapeguard.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/model/dupire.hpp>
#include <dal/model/factory.hpp>
#include <dal/model/hybrid.hpp>
#include <dal/model/surface/lvmodel.hpp>
#include <dal/platform/platform.hpp>
#include <dal/storage/json.hpp>

using namespace Dal;

TEST(ModelTest, TestLocalVolSurfaceCopiesOwnTheirParameters) {
    const LocalVolSurfaceData_ data("vol", {100.0}, {0.0}, Matrix_<>(1, 1, 0.2));
    AAD::LocalVolSurface_<double> original(data), copied(original), assigned(data);
    assigned = original;
    for (auto* copy : {&copied, &assigned}) {
        ASSERT_NE(copy->Parameters()[0], original.Parameters()[0]);
        *copy->Parameters()[0] = 0.8;
        ASSERT_NEAR(copy->Vol(0.0, 100.0), 0.8, 1e-12);
        ASSERT_NEAR(original.Vol(0.0, 100.0), 0.2, 1e-12);
    }
}

TEST(ModelTest, TestLocalVolSurfaceInterpolatesAndRoundTrips) {
    Matrix_<> vols(2, 2);
    vols(0, 0) = 0.20;
    vols(0, 1) = 0.24;
    vols(1, 0) = 0.30;
    vols(1, 1) = 0.34;
    const LocalVolSurfaceData_ data("equity_vol", {80.0, 120.0}, {0.0, 1.0}, vols);
    const AAD::LocalVolSurface_<double> surface(data);
    ASSERT_NEAR(surface.Vol(0.5, std::sqrt(80.0 * 120.0)), 0.27, 1e-12);
    ASSERT_NEAR(surface.Vol(-1.0, 50.0), 0.20, 1e-12);
    ASSERT_NEAR(surface.Vol(2.0, 150.0), 0.34, 1e-12);
    const auto restored = handle_cast<LocalVolSurfaceData_>(JSON::ReadString(JSON::WriteString(data), false));
    ASSERT_TRUE(restored);
    ASSERT_EQ(JSON::WriteString(*restored), JSON::WriteString(data));
}

TEST(ModelTest, TestLocalVolSurfaceRejectsInvalidAxesAndVols) {
    const Matrix_<> good(2, 2, 0.20);
    ASSERT_THROW(LocalVolSurfaceData_("bad", {120.0, 80.0}, {0.0, 1.0}, good), Exception_);
    ASSERT_THROW(LocalVolSurfaceData_("bad", {80.0, 120.0}, {0.0, 0.0}, good), Exception_);
    ASSERT_THROW(LocalVolSurfaceData_("bad", {80.0, 120.0}, {0.0, 1.0}, Matrix_<>(1, 2, 0.20)), Exception_);
    ASSERT_THROW(LocalVolSurfaceData_("bad", {80.0, 120.0}, {0.0, 1.0}, Matrix_<>(2, 2, -0.01)), Exception_);
    ASSERT_THROW(LocalVolSurfaceData_("bad", {80.0, 120.0}, {0.0, 1.0}, Matrix_<>(2, 2, std::numeric_limits<double>::quiet_NaN())), Exception_);
}

TEST(ModelTest, TestLocalVolSurfaceCalibratesFromFlatImpliedVol) {
    class FlatIVS_ final : public AAD::IVS_ {
    public:
        FlatIVS_() : IVS_(100.0, 0.03, 0.01) {}
        [[nodiscard]] double ImpliedVol(double, double) const override { return 0.20; }
    };
    const FlatIVS_ ivs;
    const auto surface = CalibrateDupireLocalVolSurface("calibrated", ivs, {80.0, 100.0, 120.0}, 10.0, {0.25, 1.0}, 0.25);
    const auto restored = handle_cast<LocalVolSurfaceData_>(JSON::ReadString(JSON::WriteString(*surface), false));
    ASSERT_TRUE(restored);
    for (const double vol : restored->vols_)
        ASSERT_NEAR(vol, 0.20, 3e-6);
}

TEST(ModelTest, TestHybridLocalVolUsesInternalTimeSteps) {
    const Handle_<LocalVolSurfaceData_> surface(new LocalVolSurfaceData_("flat", {100.0}, {0.0}, Matrix_<>(1, 1, 0.20)));
    HybridSettings_ settings;
    settings.domesticCurrency_ = "USD";
    settings.components_ = {Handle_<HybridComponentData_>(new HybridLocalVolEquityData_("equity", "EQ[A]", "USD", "W_EQ", 100.0, 0.0, surface)),
                            Handle_<HybridComponentData_>(new HybridDeterministicRateData_("rate", "USD", 0.0))};
    settings.correlation_ = Handle_<HybridCorrelationData_>(new HybridConstantCorrelationData_("corr", {"W_EQ"}, Matrix_<>(1, 1, 1.0)));
    auto model = CreateModel<double>(Handle_<ModelData_>(new HybridModelData_("hybrid", settings)));
    const Vector_<> timeline{0.0, 1.0};
    Vector_<AAD::SampleDef_> definitions(2);
    definitions[1].indexNames_ = {"EQ[A]"};
    model->Allocate(timeline, definitions);
    model->Init(timeline, definitions);
    ASSERT_EQ(model->SimDim(), 12u);
    AAD::Scenario_<> path;
    AAD::AllocatePath(definitions, path);
    model->GeneratePath(Vector_<>(12, 0.0), &path);
    ASSERT_NEAR(path[1].observations_[0], 100.0 * std::exp(-0.02), 1e-12);
}

TEST(ModelTest, TestHybridFlatLocalVolMatchesBlackScholesPath) {
    const Handle_<LocalVolSurfaceData_> surface(new LocalVolSurfaceData_("flat", {80.0, 120.0}, {0.0, 1.0}, Matrix_<>(2, 2, 0.20)));
    HybridSettings_ settings;
    settings.domesticCurrency_ = "USD";
    settings.components_ = {
        Handle_<HybridComponentData_>(new HybridDeterministicRateData_("rate", "USD", 0.05)),
        Handle_<HybridComponentData_>(new HybridLocalVolEquityData_("equity", "EQ[A]", "USD", "W_EQ", 100.0, 0.01, surface, 1.0))};
    settings.correlation_ = Handle_<HybridCorrelationData_>(new HybridConstantCorrelationData_("corr", {"W_EQ"}, Matrix_<>(1, 1, 1.0)));
    auto hybrid = CreateModel<double>(Handle_<ModelData_>(new HybridModelData_("hybrid", settings)));
    AAD::BlackScholes_<> baseline(100.0, 0.20, 0.05, 0.01);
    const Vector_<> timeline{0.0, 0.5, 1.0};
    Vector_<AAD::SampleDef_> definitions(timeline.size());
    for (auto& definition : definitions)
        definition.indexNames_ = {"EQ[A]"};
    hybrid->Allocate(timeline, definitions);
    baseline.Allocate(timeline, definitions);
    hybrid->Init(timeline, definitions);
    baseline.Init(timeline, definitions);
    AAD::Scenario_<> hybridPath, baselinePath;
    AAD::AllocatePath(definitions, hybridPath);
    AAD::AllocatePath(definitions, baselinePath);
    const Vector_<> gaussian{0.3, -0.4};
    hybrid->GeneratePath(gaussian, &hybridPath);
    baseline.GeneratePath(gaussian, &baselinePath);
    for (size_t i = 0; i < timeline.size(); ++i) {
        ASSERT_NEAR(hybridPath[i].numeraire_, baselinePath[i].numeraire_, 1e-12);
        ASSERT_NEAR(hybridPath[i].observations_[0], baselinePath[i].observations_[0], 1e-12);
    }
}

TEST(ModelTest, TestHybridGsrAndLocalVolShareNumeraireAndRateObservations) {
    const Date_ today(2026, 9, 28);
    const Date_ oneYear(2027, 9, 28);
    const Date_ twoYears(2028, 9, 28);
    const Handle_<GSRCurveData_> curve(
        new GSRCurveData_("curve", today, "USD", {today, oneYear, twoYears}, {0.0, -0.03, -0.06}, {}, Matrix_<>(0, 0)));
    const Handle_<GSRVolData_> rateVol(new GSRVolData_("rate_vol", {today}, {0.02}, {today}, {1.0}));
    const Handle_<LocalVolSurfaceData_> equityVol(new LocalVolSurfaceData_("equity_vol", {100.0}, {0.0}, Matrix_<>(1, 1, 0.20)));
    HybridSettings_ settings;
    settings.domesticCurrency_ = "USD";
    settings.components_ = {
        Handle_<HybridComponentData_>(new HybridGSRRateData_("rate", "W_RATE", curve, rateVol)),
        Handle_<HybridComponentData_>(new HybridLocalVolEquityData_("equity", "EQ[A]", "USD", "W_EQ", 100.0, 0.0, equityVol, 1.0))};
    Matrix_<> correlation(2, 2, 0.0);
    correlation(0, 0) = correlation(1, 1) = 1.0;
    correlation(0, 1) = correlation(1, 0) = 0.5;
    settings.correlation_ = Handle_<HybridCorrelationData_>(new HybridConstantCorrelationData_("corr", {"W_EQ", "W_RATE"}, correlation));
    const HybridModelData_ data("hybrid", settings);
    const auto restored = handle_cast<HybridModelData_>(JSON::ReadString(JSON::WriteString(data), false));
    ASSERT_TRUE(restored);
    auto hybrid = CreateModel<double>(Handle_<ModelData_>(restored));
    auto gsr = CreateModel<double>(Handle_<ModelData_>(new GSRModelData_("gsr", curve, rateVol)));
    const Vector_<> timeline{0.0, 1.0};
    Vector_<AAD::SampleDef_> hybridDefs(2), rateDefs(2);
    hybridDefs[1].indexNames_ = {"EQ[A]", "IR[USD,DF,2028-09-28]"};
    rateDefs[1].indexNames_ = {"IR[USD,DF,2028-09-28]"};
    hybrid->Allocate(timeline, hybridDefs);
    gsr->Allocate(timeline, rateDefs);
    hybrid->Init(timeline, hybridDefs);
    gsr->Init(timeline, rateDefs);
    AAD::Scenario_<> hybridPath, ratePath;
    AAD::AllocatePath(hybridDefs, hybridPath);
    AAD::AllocatePath(rateDefs, ratePath);
    hybrid->GeneratePath({0.3, 0.4, 0.0}, &hybridPath);
    gsr->GeneratePath({0.5 * 0.3 + std::sqrt(0.75) * 0.4}, &ratePath);
    ASSERT_FALSE(hybrid->NumeraireIsDeterministic());
    ASSERT_EQ(hybrid->EvaluationDate(), today);
    ASSERT_NEAR(hybridPath[1].numeraire_ * std::exp(-0.5 * 0.02 * 0.02 / 12.0), ratePath[1].numeraire_, 1e-12);
    ASSERT_NEAR(hybridPath[1].observations_[1], ratePath[1].observations_[0], 1e-12);
    ASSERT_NEAR(hybridPath[1].observations_[0] / hybridPath[1].numeraire_, 100.0 * std::exp(-0.02 + 0.20 * 0.3), 1e-10);
}

TEST(ModelTest, TestHybridGsrLocalVolAadRisksAndRealizedCarry) {
    const Date_ today(2026, 9, 28);
    const Date_ oneYear(2027, 9, 28);
    const Date_ twoYears(2028, 9, 28);
    const Handle_<GSRCurveData_> curve(
        new GSRCurveData_("curve", today, "USD", {today, oneYear, twoYears}, {0.0, -0.03, -0.06}, {}, Matrix_<>(0, 0)));
    const Handle_<GSRVolData_> rateVol(new GSRVolData_("rate_vol", {today}, {0.02}, {today}, {1.0}));
    const Handle_<LocalVolSurfaceData_> equityVol(new LocalVolSurfaceData_("equity_vol", {100.0}, {0.0}, Matrix_<>(1, 1, 0.20)));
    HybridSettings_ settings;
    settings.domesticCurrency_ = "USD";
    settings.components_ = {
        Handle_<HybridComponentData_>(new HybridGSRRateData_("rate", "W_RATE", curve, rateVol)),
        Handle_<HybridComponentData_>(new HybridLocalVolEquityData_("equity", "EQ[A]", "USD", "W_EQ", 100.0, 0.01, equityVol, 1.0))};
    Matrix_<> correlation(2, 2, 0.0);
    correlation(0, 0) = correlation(1, 1) = 1.0;
    correlation(0, 1) = correlation(1, 0) = 0.5;
    settings.correlation_ = Handle_<HybridCorrelationData_>(new HybridConstantCorrelationData_("corr", {"W_EQ", "W_RATE"}, correlation));
    const Handle_<ModelData_> data(new HybridModelData_("hybrid", settings));
    const Vector_<> timeline{0.0, 1.0};
    Vector_<AAD::SampleDef_> definitions(2);
    definitions[1].indexNames_ = {"EQ[A]", "IR[USD,DF,2028-09-28]"};
    const Vector_<> gaussian{0.3, 0.4, 0.5};

    const TapeGuard_ guard(AAD::Tape());
    auto model = CreateModel<AAD::Number_>(data);
    ASSERT_EQ(model->ParameterLabels()[2], "lvol:EQ[A]:0:0");
    ASSERT_EQ(model->ParameterLabels()[4], "logdf:OIS:2028-09-28");
    ASSERT_EQ(model->ParameterLabels()[5], "g:2026-09-28");
    model->Allocate(timeline, definitions);
    AAD::Scenario_<AAD::Number_> path;
    AAD::AllocatePath(definitions, path);
    AAD::Rewind(*AAD::Tape());
    for (auto* parameter : model->Parameters())
        AAD::PutOnTape(*parameter);
    AAD::NewRecording(*AAD::Tape());
    model->Init(timeline, definitions);
    model->GeneratePath(gaussian, &path);
    const auto discountedSpot = path[1].observations_[0] / path[1].numeraire_;
    ASSERT_NEAR(AAD::Value(discountedSpot), 100.0 * std::exp(-0.01 - 0.5 * 0.20 * 0.20 + 0.20 * gaussian[0]), 1e-10);
    ASSERT_GT(std::abs(AAD::Value(path[1].numeraire_) - std::exp(0.03)), 1e-4);
    AAD::Number_ payoff = (path[1].observations_[0] + 10.0 * path[1].observations_[1]) / path[1].numeraire_;
    AAD::Adjoint(payoff) = 1.0;
    AAD::PropagateToStart(*AAD::Tape());

    const auto bumpedValue = [&](size_t parameter, double shift) {
        auto bumped = CreateModel<double>(data);
        bumped->Allocate(timeline, definitions);
        *bumped->Parameters()[parameter] += shift;
        bumped->Init(timeline, definitions);
        AAD::Scenario_<> bumpedPath;
        AAD::AllocatePath(definitions, bumpedPath);
        bumped->GeneratePath(gaussian, &bumpedPath);
        return (bumpedPath[1].observations_[0] + 10.0 * bumpedPath[1].observations_[1]) / bumpedPath[1].numeraire_;
    };
    constexpr double bump = 1e-5;
    for (const size_t parameter : {size_t{2}, size_t{4}, size_t{5}}) {
        const double difference = (bumpedValue(parameter, bump) - bumpedValue(parameter, -bump)) / (2.0 * bump);
        ASSERT_GT(std::abs(difference), 1e-5);
        ASSERT_NEAR(AAD::Adjoint(*model->Parameters()[parameter]), difference, 1e-5);
    }
}

TEST(ModelTest, TestHybridLocalVolAadSpotAndVolRisk) {
    const Handle_<LocalVolSurfaceData_> surface(new LocalVolSurfaceData_("flat", {100.0}, {0.0}, Matrix_<>(1, 1, 0.20)));
    HybridSettings_ settings;
    settings.domesticCurrency_ = "USD";
    settings.components_ = {Handle_<HybridComponentData_>(new HybridLocalVolEquityData_("equity", "EQ[A]", "USD", "W_EQ", 100.0, 0.0, surface, 1.0)),
                            Handle_<HybridComponentData_>(new HybridDeterministicRateData_("rate", "USD", 0.0))};
    settings.correlation_ = Handle_<HybridCorrelationData_>(new HybridConstantCorrelationData_("corr", {"W_EQ"}, Matrix_<>(1, 1, 1.0)));
    const Handle_<ModelData_> data(new HybridModelData_("hybrid", settings));
    const TapeGuard_ guard(AAD::Tape());
    auto model = CreateModel<AAD::Number_>(data);
    const Vector_<> timeline{1.0};
    Vector_<AAD::SampleDef_> definitions(1);
    definitions[0].indexNames_ = {"EQ[A]"};
    model->Allocate(timeline, definitions);
    AAD::Scenario_<AAD::Number_> path;
    AAD::AllocatePath(definitions, path);
    AAD::Rewind(*AAD::Tape());
    for (auto* parameter : model->Parameters())
        AAD::PutOnTape(*parameter);
    AAD::NewRecording(*AAD::Tape());
    model->Init(timeline, definitions);
    model->GeneratePath({0.4}, &path);
    AAD::Number_ payoff = path[0].observations_[0];
    AAD::Adjoint(payoff) = 1.0;
    AAD::PropagateToStart(*AAD::Tape());
    const double expected = 100.0 * std::exp(-0.5 * 0.20 * 0.20 + 0.20 * 0.4);
    ASSERT_NEAR(AAD::Value(payoff), expected, 1e-10);
    ASSERT_NEAR(AAD::Adjoint(*model->Parameters()[0]), expected / 100.0, 1e-10);
    ASSERT_NEAR(AAD::Adjoint(*model->Parameters()[2]), expected * (0.4 - 0.20), 1e-8);
    auto clone = model->Clone();
    ASSERT_NE(clone->Parameters()[2], model->Parameters()[2]);
}

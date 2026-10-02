//
// Created by Codex on 2026/10/2.
//

#include <gtest/gtest.h>
#include <cmath>
#include <random>
#include <dal/platform/platform.hpp>
#include <dal/curve/tapeguard.hpp>
#include <dal/model/factory.hpp>
#include <dal/model/gsreuropean.hpp>
#include <dal/model/gsrslv.hpp>
#include <dal/storage/json.hpp>

using Dal::Date_;
using Dal::Handle_;
using Dal::Matrix_;
using Dal::Vector_;

namespace {
    const Date_ TODAY(2026, 10, 2);

    Handle_<Dal::MultiFactorGSRModelData_> Gaussian(int n = 1, bool knots = false) {
        const Handle_<Dal::GSRCurveData_> curve(
            new Dal::GSRCurveData_("curve", TODAY, "USD", {TODAY, TODAY.AddDays(3650)}, {0.0, -0.3}, {}, Matrix_<>(0, 0)));
        Dal::MultiFactorGSRVolSettings_ vol;
        vol.gKnotDates_ = vol.hKnotDates_ = {TODAY};
        if (knots) {
            vol.gKnotDates_.push_back(TODAY.AddDays(120));
            vol.hKnotDates_.push_back(TODAY.AddDays(180));
        }
        vol.gValues_ = Matrix_<>(n, vol.gKnotDates_.size());
        vol.hValues_ = Matrix_<>(n, vol.hKnotDates_.size());
        vol.correlations_ = Matrix_<>(n, n, 0.3);
        for (int i = 0; i < n; ++i) {
            vol.factorNames_.push_back("factor" + Dal::String::FromInt(i));
            vol.correlations_(i, i) = 1.0;
            for (int j = 0; j < vol.gValues_.Cols(); ++j)
                vol.gValues_(i, j) = 0.02 * (1.0 + 0.2 * j) / (i + 1);
            for (int j = 0; j < vol.hValues_.Cols(); ++j)
                vol.hValues_(i, j) = (1.0 - 0.6 * i) * (1.0 - 0.2 * j);
        }
        return Handle_<Dal::MultiFactorGSRModelData_>(
            new Dal::MultiFactorGSRModelData_("gaussian", curve, Handle_<Dal::MultiFactorGSRVolData_>(new Dal::MultiFactorGSRVolData_("vol", vol))));
    }

    Handle_<Dal::GSRLeverageData_> Leverage(double level = 1.0) {
        return Handle_<Dal::GSRLeverageData_>(new Dal::GSRLeverageData_("leverage", {0.0}, {0.0}, Matrix_<>(1, 1, level)));
    }

    Vector_<Dal::AAD::SampleDef_> Definitions() {
        Vector_<Dal::AAD::SampleDef_> definitions(1);
        definitions[0].indexNames_ = {"IR[USD,DF,2028-10-01]"};
        return definitions;
    }

    struct Moments_ {
        double mean_ = 0.0, variance_ = 0.0;
    };

    template <class F_> Moments_ Simulate(Dal::AAD::GSRSLV_<>* model, int count, F_ value) {
        std::mt19937_64 generator(18371);
        std::normal_distribution<double> normal;
        Vector_<> gaussian(model->SimDim());
        double sum = 0.0, sumSquares = 0.0;
        Dal::AAD::Scenario_<> path;
        Dal::AAD::AllocatePath(Definitions(), path);
        for (int i = 0; i < count; ++i) {
            for (auto& x : gaussian)
                x = normal(generator);
            const double sample = value(gaussian, &path);
            sum += sample;
            sumSquares += sample * sample;
        }
        const double mean = sum / count;
        return {mean, std::max(0.0, sumSquares / count - mean * mean)};
    }
} // namespace

TEST(GSRSLVTest, TestGaussianStateAndConditionalDiscountMatchGSR) {
    Dal::GSRSLVSettings_ settings;
    settings.volOfVol_ = 0.0;
    settings.maxStep_ = 1.0;
    const auto gaussian = Gaussian();
    Dal::AAD::GSRSLV_<> model(Dal::GSRSLVModelData_("smile", gaussian, Leverage(), settings));
    const Vector_<> timeline{1.0};
    const auto definitions = Definitions();
    model.Allocate(timeline, definitions);
    ASSERT_EQ(model.SimDim(), 3U);
    model.Init(timeline, definitions);
    ASSERT_EQ(model.SimDim(), 3U);
    const auto state = model.StateAfter({0.7, 0.2, -0.4});
    ASSERT_NEAR(state.x_[0], 0.02 * 0.7 + 0.0004 / 2.0, 1e-14);
    ASSERT_NEAR(state.y_(0, 0), 0.0004, 1e-14);
    ASSERT_DOUBLE_EQ(state.variance_, 1.0);
    ASSERT_NEAR(state.logNumeraire_, 0.03 + 0.007 + 0.0004 / 6.0 - 0.4 * 0.02 / std::sqrt(12.0), 1e-14);
    Dal::AAD::Scenario_<> path, legacyPath;
    Dal::AAD::AllocatePath(definitions, path);
    Dal::AAD::AllocatePath(definitions, legacyPath);
    model.GeneratePath({0.7, 0.2, 0.0}, &path);
    Dal::AAD::GSR_<> legacy(*gaussian);
    legacy.Allocate(timeline, definitions);
    legacy.Init(timeline, definitions);
    legacy.GeneratePath({0.7}, &legacyPath);
    ASSERT_NEAR(path[0].observations_[0], legacyPath[0].observations_[0], 1e-14);
    ASSERT_NEAR(std::exp(-state.logNumeraire_ - 0.4 * 0.02 / std::sqrt(12.0) + 0.0004 / 24.0), 1.0 / legacyPath[0].numeraire_, 1e-14);
}

TEST(GSRSLVTest, TestLeverageInterpolationAndValidation) {
    Matrix_<> values(2, 2);
    values(0, 0) = 1.0;
    values(0, 1) = 2.0;
    values(1, 0) = 3.0;
    values(1, 1) = 4.0;
    const Handle_<Dal::GSRLeverageData_> leverage(new Dal::GSRLeverageData_("grid", {-0.02, 0.02}, {0.0, 2.0}, values));
    Dal::AAD::GSRSLV_<> model(Dal::GSRSLVModelData_("smile", Gaussian(), leverage));
    ASSERT_DOUBLE_EQ(model.LocalLeverage(1.0, 0.0), 2.5);
    ASSERT_DOUBLE_EQ(model.LocalLeverage(0.0, -0.02), 1.0);
    ASSERT_DOUBLE_EQ(model.LocalLeverage(9.0, -0.04), 2.0);
    ASSERT_DOUBLE_EQ(model.LocalLeverage(-1.0, 0.04), 3.0);
    ASSERT_THROW(Dal::GSRLeverageData_("bad", {0.0}, {-1.0}, Matrix_<>(1, 1, 1.0)), Dal::Exception_);
    ASSERT_THROW(Dal::GSRLeverageData_("bad", {0.0, 0.0}, {0.0}, Matrix_<>(2, 1, 1.0)), Dal::Exception_);
    ASSERT_THROW(Dal::GSRLeverageData_("bad", {0.0}, {0.0}, Matrix_<>(1, 1, 0.0)), Dal::Exception_);
    ASSERT_THROW(Dal::GSRLeverageData_("bad", {0.0}, {0.0}, Matrix_<>(2, 1, 1.0)), Dal::Exception_);
}

TEST(GSRSLVTest, TestLeverageScalesDriftAndCovariance) {
    Dal::GSRSLVSettings_ settings;
    settings.maxStep_ = 1.0;
    settings.varianceCorrelations_ = {0.6};
    Dal::AAD::GSRSLV_<> model(Dal::GSRSLVModelData_("smile", Gaussian(), Leverage(2.0), settings));
    const Vector_<> timeline{1.0};
    const auto definitions = Definitions();
    model.Allocate(timeline, definitions);
    model.Init(timeline, definitions);
    const auto state = model.StateAfter({0.7, -0.2, 0.3});
    ASSERT_NEAR(state.x_[0], 0.04 * 0.7 + 0.0016 / 2.0, 1e-14);
    ASSERT_NEAR(state.y_(0, 0), 0.0016, 1e-14);
    ASSERT_NEAR(state.variance_, 1.0 + 0.5 * (0.6 * 0.7 - 0.8 * 0.2), 1e-14);
    ASSERT_NEAR(state.logNumeraire_, 0.03 + 0.014 + 0.0016 / 6.0 + 0.3 * 0.04 / std::sqrt(12.0), 1e-14);
}

TEST(GSRSLVTest, TestFullTruncationKeepsLatentNegativeVariance) {
    Dal::GSRSLVSettings_ settings;
    settings.kappa_ = 0.5;
    settings.volOfVol_ = 2.0;
    settings.maxStep_ = 0.5;
    Dal::AAD::GSRSLV_<> model(Dal::GSRSLVModelData_("smile", Gaussian(), Leverage(), settings));
    const Vector_<> timeline{1.0};
    const auto definitions = Definitions();
    model.Allocate(timeline, definitions);
    model.Init(timeline, definitions);
    const auto state = model.StateAfter({0.0, -2.0, 0.0, 0.0, 20.0, 0.0});
    ASSERT_DOUBLE_EQ(state.variance_, 0.0);
    ASSERT_NEAR(state.latentVariance_, 1.25 - 2.0 * std::sqrt(2.0), 1e-14);
    ASSERT_NEAR(state.y_(0, 0), 0.0002, 1e-14);
    ASSERT_NEAR(state.x_[0], 0.00015, 1e-14);
}

TEST(GSRSLVTest, TestRejectsOverflowAndMalformedPath) {
    Dal::GSRSLVSettings_ settings;
    settings.maxStep_ = 1.0;
    settings.volOfVol_ = 1e308;
    Dal::AAD::GSRSLV_<> model(Dal::GSRSLVModelData_("smile", Gaussian(), Leverage(), settings));
    const Vector_<> timeline{1.0};
    const auto definitions = Definitions();
    model.Allocate(timeline, definitions);
    model.Init(timeline, definitions);
    ASSERT_THROW(static_cast<void>(model.StateAfter({0.0, -10.0, 0.0})), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(model.StateAfter({0.0})), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(model.StateAfter({0.0, std::numeric_limits<double>::quiet_NaN(), 0.0})), Dal::Exception_);
    Dal::AAD::Scenario_<> path;
    Dal::AAD::AllocatePath(definitions, path);
    path[0].observations_.clear();
    ASSERT_THROW(model.GeneratePath({0.0, 0.0, 0.0}, &path), Dal::Exception_);
}

TEST(GSRSLVTest, TestZeroHorizonAndInvalidGrid) {
    Dal::GSRSLVSettings_ settings;
    settings.volOfVol_ = 0.0;
    settings.maxStep_ = 1e-10;
    Dal::AAD::GSRSLV_<> model(Dal::GSRSLVModelData_("smile", Gaussian(), Leverage(), settings));
    const auto definitions = Definitions();
    model.Allocate({0.0}, definitions);
    model.Init({0.0}, definitions);
    ASSERT_EQ(model.SimDim(), 0U);
    Dal::AAD::Scenario_<> path;
    Dal::AAD::AllocatePath(definitions, path);
    model.GeneratePath({}, &path);
    ASSERT_NEAR(path[0].observations_[0], std::exp(-0.06), 1e-14);
    ASSERT_DOUBLE_EQ(path[0].numeraire_, 1.0);
    ASSERT_THROW(model.Allocate({1.0}, definitions), Dal::Exception_);
    ASSERT_THROW(model.Allocate({0.0, 0.0}, Vector_<Dal::AAD::SampleDef_>(2)), Dal::Exception_);
    ASSERT_THROW(model.Allocate({}, {}), Dal::Exception_);
    settings.maxStep_ = 1.0;
    Dal::AAD::GSRSLV_<> fractional(Dal::GSRSLVModelData_("smile", Gaussian(), Leverage(), settings));
    fractional.Allocate({0.5 / 365.0}, definitions);
    ASSERT_THROW(fractional.Init({0.5 / 365.0}, definitions), Dal::Exception_);
}

TEST(GSRSLVTest, TestBankAccountRejectsOverflowAndUnderflow) {
    Dal::GSRSLVSettings_ settings;
    settings.volOfVol_ = 0.0;
    settings.maxStep_ = 1.0;
    Dal::AAD::GSRSLV_<> model(Dal::GSRSLVModelData_("smile", Gaussian(), Leverage(), settings));
    const auto definitions = Definitions();
    model.Allocate({1.0}, definitions);
    model.Init({1.0}, definitions);
    Dal::AAD::Scenario_<> path;
    Dal::AAD::AllocatePath(definitions, path);
    ASSERT_THROW(model.GeneratePath({1e308, 0.0, 0.0}, &path), Dal::Exception_);
    ASSERT_THROW(model.GeneratePath({-1e308, 0.0, 0.0}, &path), Dal::Exception_);
}

TEST(GSRSLVTest, TestArchiveAndSingularCorrelation) {
    Dal::GSRSLVSettings_ settings;
    settings.varianceCorrelations_ = {1.0};
    const Dal::GSRSLVModelData_ data("smile", Gaussian(), Leverage(), settings);
    const auto restored = Dal::handle_cast<Dal::GSRSLVModelData_>(Dal::JSON::ReadString(Dal::JSON::WriteString(data), true));
    ASSERT_TRUE(restored);
    ASSERT_DOUBLE_EQ(restored->varianceCorrelations_[0], 1.0);
    settings.varianceCorrelations_ = {1.01};
    ASSERT_THROW(Dal::GSRSLVModelData_("bad", Gaussian(), Leverage(), settings), Dal::Exception_);
    settings.varianceCorrelations_ = {0.0, 0.0};
    ASSERT_THROW(Dal::GSRSLVModelData_("bad", Gaussian(), Leverage(), settings), Dal::Exception_);
    settings.varianceCorrelations_ = {};
    settings.maxStep_ = 0.0;
    ASSERT_THROW(Dal::GSRSLVModelData_("bad", Gaussian(), Leverage(), settings), Dal::Exception_);
}

TEST(GSRSLVTest, TestJointCorrelationRejectsIndefiniteMatrix) {
    Dal::GSRSLVSettings_ settings;
    settings.varianceCorrelations_ = {0.9, 0.9};
    ASSERT_THROW(Dal::GSRSLVModelData_("bad", Gaussian(2), Leverage(), settings), Dal::Exception_);
}

TEST(GSRSLVTest, TestGaussianLimitWithSignedLoadingsAndChangingKnots) {
    Dal::GSRSLVSettings_ settings;
    settings.volOfVol_ = 0.0;
    settings.maxStep_ = 0.1;
    const auto gaussian = Gaussian(3, true);
    Dal::AAD::GSRSLV_<> model(Dal::GSRSLVModelData_("smile", gaussian, Leverage(), settings));
    Dal::AAD::GSR_<> legacy(*gaussian);
    const Vector_<> timeline{1.0};
    const auto definitions = Definitions();
    model.Allocate(timeline, definitions);
    model.Init(timeline, definitions);
    legacy.Allocate(timeline, definitions);
    legacy.Init(timeline, definitions);
    Dal::AAD::Scenario_<> path, legacyPath;
    Dal::AAD::AllocatePath(definitions, path);
    Dal::AAD::AllocatePath(definitions, legacyPath);
    model.GeneratePath(Vector_<>(model.SimDim(), 0.0), &path);
    legacy.GeneratePath({0.0, 0.0, 0.0}, &legacyPath);
    ASSERT_NEAR(path[0].observations_[0], legacyPath[0].observations_[0], 1e-14);
    ASSERT_EQ(model.NumFactors(), 5U);
}

TEST(GSRSLVTest, TestCIRMomentsConvergeUnderRefinement) {
    Dal::GSRSLVSettings_ settings;
    settings.kappa_ = 1.3;
    settings.volOfVol_ = 0.35;
    const double exactVariance = 0.35 * 0.35 / 2.6 * (1.0 - std::exp(-2.6));
    Vector_<> varianceErrors;
    const int count = 32768;
    for (double step : {0.25, 1.0 / 128.0}) {
        settings.maxStep_ = step;
        Dal::AAD::GSRSLV_<> model(Dal::GSRSLVModelData_("smile", Gaussian(), Leverage(), settings));
        model.Allocate({1.0}, Definitions());
        model.Init({1.0}, Definitions());
        const auto moments = Simulate(&model, count, [&](const auto& normals, auto*) { return model.StateAfter(normals).variance_; });
        ASSERT_NEAR(moments.mean_, 1.0, 5.0 * std::sqrt(moments.variance_ / count));
        varianceErrors.push_back(std::abs(moments.variance_ - exactVariance));
    }
    ASSERT_LT(varianceErrors[1], varianceErrors[0]);
    ASSERT_LT(varianceErrors[1], 5.0 * exactVariance * std::sqrt(2.0 / count) + 0.001);
}

TEST(GSRSLVTest, TestDiscountedBondWithStateDependentLeverageAndCorrelation) {
    Matrix_<> values(2, 1);
    values(0, 0) = 0.8;
    values(1, 0) = 1.2;
    const Handle_<Dal::GSRLeverageData_> grid(new Dal::GSRLeverageData_("grid", {-0.05, 0.05}, {0.0}, values));
    Dal::GSRSLVSettings_ settings;
    settings.kappa_ = 0.4;
    settings.volOfVol_ = 0.9;
    settings.varianceCorrelations_ = {-0.4, 0.2};
    for (double step : {0.25, 1.0 / 64.0}) {
        settings.maxStep_ = step;
        Dal::AAD::GSRSLV_<> model(Dal::GSRSLVModelData_("smile", Gaussian(2, true), grid, settings));
        model.Allocate({1.0}, Definitions());
        model.Init({1.0}, Definitions());
        const int count = 16384;
        const auto moments = Simulate(&model, count, [&](const auto& normals, auto* path) {
            model.GeneratePath(normals, path);
            return (*path)[0].observations_[0] / (*path)[0].numeraire_;
        });
        ASSERT_NEAR(moments.mean_, std::exp(-0.06), 5.0 * std::sqrt(moments.variance_ / count));
    }
}

TEST(GSRSLVTest, TestGaussianOptionMatchesIndependentAnalyticPrice) {
    Dal::GSRSLVSettings_ settings;
    settings.volOfVol_ = 0.0;
    settings.maxStep_ = 1.0;
    const auto gaussian = Gaussian();
    Dal::AAD::GSRSLV_<> model(Dal::GSRSLVModelData_("smile", gaussian, Leverage(), settings));
    model.Allocate({1.0}, Definitions());
    model.Init({1.0}, Definitions());
    const int count = 65536;
    const auto moments = Simulate(&model, count, [&](const auto& normals, auto* path) {
        model.GeneratePath(normals, path);
        return std::max(0.0, (*path)[0].observations_[0] - 0.97) / (*path)[0].numeraire_;
    });
    const Dal::GSRBondOption_ option{TODAY.AddDays(365), TODAY.AddDays(730), 0.97, Dal::OptionType_("CALL")};
    ASSERT_NEAR(moments.mean_, Dal::PriceGSREuropeanOption(*gaussian, option).price_, 5.0 * std::sqrt(moments.variance_ / count));
}

TEST(GSRSLVTest, TestAdjointsMatchReinitializedBumpsAndCloneOwnsParameters) {
    Matrix_<> values(2, 2);
    values(0, 0) = 0.8;
    values(0, 1) = 1.0;
    values(1, 0) = 1.2;
    values(1, 1) = 1.1;
    const Handle_<Dal::GSRLeverageData_> leverage(new Dal::GSRLeverageData_("grid", {-0.05, 0.05}, {0.0, 1.0}, values));
    Dal::GSRSLVSettings_ settings;
    settings.maxStep_ = 0.25;
    settings.varianceCorrelations_ = {-0.4};
    const Handle_<Dal::ModelData_> data(new Dal::GSRSLVModelData_("smile", Gaussian(), leverage, settings));
    const Vector_<> timeline{1.0}, normals{0.7, 0.2, -0.4, -0.3, 0.5, 0.1, 0.4, -0.2, 0.3, 0.2, 0.1, -0.3};
    const auto definitions = Definitions();
    const Dal::TapeGuard_ guard(Dal::AAD::Tape());
    auto model = Dal::CreateModel<Dal::AAD::Number_>(data);
    model->Allocate(timeline, definitions);
    Dal::AAD::Scenario_<Dal::AAD::Number_> path;
    Dal::AAD::AllocatePath(definitions, path);
    Dal::AAD::Rewind(*Dal::AAD::Tape());
    for (auto* parameter : model->Parameters())
        Dal::AAD::PutOnTape(*parameter);
    Dal::AAD::NewRecording(*Dal::AAD::Tape());
    model->Init(timeline, definitions);
    model->GeneratePath(normals, &path);
    Dal::AAD::Number_ payoff = path[0].observations_[0] / path[0].numeraire_;
    Dal::AAD::Adjoint(payoff) = 1.0;
    Dal::AAD::PropagateToStart(*Dal::AAD::Tape());
    auto bumped = Dal::CreateModel<double>(data);
    bumped->Allocate(timeline, definitions);
    Dal::AAD::Scenario_<> bumpedPath;
    Dal::AAD::AllocatePath(definitions, bumpedPath);
    for (size_t i = 0; i < model->Parameters().size(); ++i) {
        const double original = *bumped->Parameters()[i];
        double prices[2];
        for (int direction = 0; direction < 2; ++direction) {
            *bumped->Parameters()[i] = original + (direction == 0 ? -1e-6 : 1e-6);
            bumped->Init(timeline, definitions);
            bumped->GeneratePath(normals, &bumpedPath);
            prices[direction] = bumpedPath[0].observations_[0] / bumpedPath[0].numeraire_;
        }
        *bumped->Parameters()[i] = original;
        ASSERT_NEAR(Dal::AAD::Adjoint(*model->Parameters()[i]), (prices[1] - prices[0]) / 2e-6, 1e-8) << model->ParameterLabels()[i];
    }
    bumped->Init(timeline, definitions);
    auto clone = bumped->Clone();
    for (size_t i = 0; i < bumped->Parameters().size(); ++i)
        ASSERT_NE(clone->Parameters()[i], bumped->Parameters()[i]);
    const double original = *bumped->Parameters().back();
    *clone->Parameters().back() = 1.4;
    clone->Init(timeline, definitions);
    clone->GeneratePath(normals, &bumpedPath);
    ASSERT_DOUBLE_EQ(*bumped->Parameters().back(), original);
}

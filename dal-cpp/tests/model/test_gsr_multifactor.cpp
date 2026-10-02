//
// Created by Codex on 2026/10/2.
//

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include <dal/platform/platform.hpp>

#include <dal/curve/tapeguard.hpp>
#include <dal/model/factory.hpp>
#include <dal/model/gsrmultidata.hpp>
#include <dal/script/simulation.hpp>
#include <dal/storage/json.hpp>

using Dal::Date_;
using Dal::Handle_;
using Dal::Matrix_;
using Dal::Vector_;

namespace {
    Handle_<Dal::GSRCurveData_> Curve() {
        const Date_ today(2026, 10, 2);
        return Handle_<Dal::GSRCurveData_>(
            new Dal::GSRCurveData_("curve", today, "USD", {today, today.AddDays(365), today.AddDays(730)}, {0.0, -0.03, -0.06}, {}, Matrix_<>(0, 0)));
    }

    Dal::MultiFactorGSRVolSettings_ Settings(size_t n) {
        Dal::MultiFactorGSRVolSettings_ result;
        for (size_t i = 0; i < n; ++i)
            result.factorNames_.push_back("factor" + Dal::String::FromInt(static_cast<int>(i)));
        result.gKnotDates_ = result.hKnotDates_ = {Date_(2026, 10, 2)};
        result.gValues_ = Matrix_<>(static_cast<int>(n), 1, 0.02);
        result.hValues_ = Matrix_<>(static_cast<int>(n), 1, 1.0);
        result.correlations_ = Matrix_<>(static_cast<int>(n), static_cast<int>(n), 0.0);
        for (size_t i = 0; i < n; ++i)
            result.correlations_(static_cast<int>(i), static_cast<int>(i)) = 1.0;
        return result;
    }

    Handle_<Dal::ModelData_> Data(const Dal::MultiFactorGSRVolSettings_& settings) {
        return Handle_<Dal::ModelData_>(new Dal::MultiFactorGSRModelData_(
            "rates", Curve(), Handle_<Dal::MultiFactorGSRVolData_>(new Dal::MultiFactorGSRVolData_("vol", settings))));
    }

    Vector_<Dal::AAD::SampleDef_> BondDefinitions(size_t n) {
        Vector_<Dal::AAD::SampleDef_> result(n);
        for (auto& definition : result)
            definition.indexNames_ = {"IR[USD,DF,2028-10-01]"};
        return result;
    }
} // namespace

TEST(GSRMultiFactorTest, TestTwoFactorBondAndConditionalDiscountMatchAnalyticValues) {
    const Date_ today(2026, 10, 2);
    const Date_ expiry = today.AddDays(365);
    const Date_ maturity = today.AddDays(730);
    const Handle_<Dal::GSRCurveData_> curve(
        new Dal::GSRCurveData_("curve", today, "USD", {today, expiry, maturity}, {0.0, -0.03, -0.06}, {}, Matrix_<>(0, 0)));
    Matrix_<> g(2, 1), h(2, 1), correlation(2, 2, 0.3);
    g(0, 0) = 0.02;
    g(1, 0) = 0.01;
    h(0, 0) = 1.0;
    h(1, 0) = 0.4;
    correlation(0, 0) = correlation(1, 1) = 1.0;
    const Handle_<Dal::MultiFactorGSRVolData_> vol(new Dal::MultiFactorGSRVolData_("vol", {"level", "slope"}, {today}, g, {today}, h, correlation));
    const Handle_<Dal::ModelData_> data(new Dal::MultiFactorGSRModelData_("rates", curve, vol));
    auto model = Dal::CreateModel<double>(data);
    ASSERT_EQ(model->NumFactors(), 2U);
    ASSERT_TRUE(model->SupportsBrownianBridge());
    const Vector_<> timeline{0.0, 1.0};
    Vector_<Dal::AAD::SampleDef_> definitions(2);
    for (auto& definition : definitions)
        definition.indexNames_ = {"IR[USD,DF," + Dal::Date::ToString(maturity) + "]"};
    model->Allocate(timeline, definitions);
    model->Init(timeline, definitions);
    Dal::AAD::Scenario_<> path;
    Dal::AAD::AllocatePath(definitions, path);
    model->GeneratePath({0.7, -0.4}, &path);
    const double state0 = 0.02 * 0.7;
    const double state1 = 0.01 * (0.3 * 0.7 + std::sqrt(1.0 - 0.3 * 0.3) * -0.4);
    const double rateVariance = 0.02 * 0.02 + 2.0 * 0.4 * 0.3 * 0.02 * 0.01 + 0.4 * 0.4 * 0.01 * 0.01;
    const double loadingState = state0 + 0.4 * state1;
    ASSERT_NEAR(path[0].observations_[0], std::exp(-0.06), 1e-12);
    ASSERT_NEAR(path[1].observations_[0], std::exp(-0.03 - loadingState - rateVariance), 1e-12);
    ASSERT_NEAR(path[1].numeraire_, std::exp(0.03 + 0.125 * rateVariance + 0.5 * loadingState), 1e-12);
} // namespace

TEST(GSRMultiFactorTest, TestThreeIndependentFactorsWithDifferentKnotsMatchAnalyticIntegrals) {
    auto settings = Settings(3);
    settings.gKnotDates_.push_back(Date_(2026, 10, 2).AddDays(120));
    settings.hKnotDates_.push_back(Date_(2026, 10, 2).AddDays(200));
    settings.gValues_ = Matrix_<>(3, 2);
    settings.hValues_ = Matrix_<>(3, 2);
    for (int i = 0; i < 3; ++i) {
        settings.gValues_(i, 0) = 0.01 * (i + 1);
        settings.gValues_(i, 1) = 0.015 * (3 - i);
        settings.hValues_(i, 0) = 0.8 - 0.5 * i;
        settings.hValues_(i, 1) = 0.4 - 0.3 * i;
    }
    auto model = Dal::CreateModel<double>(Data(settings));
    const Vector_<> timeline{0.0, 1.0};
    const auto definitions = BondDefinitions(2);
    model->Allocate(timeline, definitions);
    model->Init(timeline, definitions);
    Dal::AAD::Scenario_<> path;
    Dal::AAD::AllocatePath(definitions, path);
    const Vector_<> gaussian{0.7, -0.4, 1.1};
    model->GeneratePath(gaussian, &path);
    const double a = 120.0 / 365.0, b = 200.0 / 365.0;
    double bondExponent = -0.03, logNumeraire = 0.03;
    for (int i = 0; i < 3; ++i) {
        const double g0 = settings.gValues_(i, 0), g1 = settings.gValues_(i, 1);
        const double h0 = settings.hValues_(i, 0), h1 = settings.hValues_(i, 1);
        const double variance = g0 * g0 * a + g1 * g1 * (1.0 - a);
        const double covariance = g0 * g0 * (h0 * (b * a - 0.5 * a * a) + h1 * (1.0 - b) * a) +
                                  g1 * g1 * (0.5 * h0 * (b - a) * (b - a) + h1 * (1.0 - b) * (b - a) + 0.5 * h1 * (1.0 - b) * (1.0 - b));
        const double normalLoading = covariance / std::sqrt(variance);
        bondExponent -= h1 * (std::sqrt(variance) * gaussian[i] + covariance) + 0.5 * h1 * h1 * variance;
        logNumeraire += 0.5 * normalLoading * normalLoading + normalLoading * gaussian[i];
    }
    ASSERT_NEAR(path[1].observations_[0], std::exp(bondExponent), 1e-12);
    ASSERT_NEAR(path[1].numeraire_, std::exp(logNumeraire), 1e-12);
}

TEST(GSRMultiFactorTest, TestSingularCorrelationAndZeroFactorsDoNotIntroduceVolatility) {
    auto settings = Settings(3);
    settings.correlations_(0, 1) = settings.correlations_(1, 0) = -1.0;
    settings.gValues_(2, 0) = 0.0;
    settings.hValues_(0, 0) = settings.hValues_(1, 0) = 1.0;
    auto model = Dal::CreateModel<double>(Data(settings));
    const auto definitions = BondDefinitions(2);
    model->Allocate({0.0, 1.0}, definitions);
    model->Init({0.0, 1.0}, definitions);
    Dal::AAD::Scenario_<> path;
    Dal::AAD::AllocatePath(definitions, path);
    model->GeneratePath({7.0, -20.0, 30.0}, &path);
    ASSERT_NEAR(path[1].observations_[0], std::exp(-0.03), 1e-12);
    ASSERT_NEAR(path[1].numeraire_, std::exp(0.03), 1e-12);
    ASSERT_EQ(model->SimDim(), 3U);
}

TEST(GSRMultiFactorTest, TestCovarianceFactorPreservesSmallPositiveVarianceAndRejectsIndefiniteMatrices) {
    Matrix_<> covariance(3, 3, 0.0);
    covariance(0, 0) = 1.0;
    covariance(1, 1) = 1e-30;
    const auto lower = Dal::AAD::CovarianceFactor(covariance);
    ASSERT_EQ(lower(0, 0), 1.0);
    ASSERT_NEAR(lower(1, 1), 1e-15, 1e-30);
    ASSERT_EQ(lower(2, 2), 0.0);
    covariance(0, 2) = covariance(2, 0) = 0.1;
    ASSERT_THROW(Dal::AAD::CovarianceFactor(covariance), Dal::Exception_);
}

TEST(GSRMultiFactorTest, TestMultiFactorOneFactorPathsMatchLegacyAndCloneOwnsParameters) {
    const auto settings = Settings(1);
    auto model = Dal::CreateModel<double>(Data(settings));
    const Handle_<Dal::GSRVolData_> vol(new Dal::GSRVolData_("legacy", settings.gKnotDates_, {0.02}, settings.hKnotDates_, {1.0}));
    auto legacy = Dal::CreateModel<double>(Handle_<Dal::ModelData_>(new Dal::GSRModelData_("legacy", Curve(), vol)));
    const Vector_<> timeline{0.0, 120.0 / 365.0, 1.0};
    const auto definitions = BondDefinitions(3);
    Dal::AAD::Scenario_<> path, legacyPath;
    Dal::AAD::AllocatePath(definitions, path);
    Dal::AAD::AllocatePath(definitions, legacyPath);
    for (auto* candidate : {model.get(), legacy.get()}) {
        candidate->Allocate(timeline, definitions);
        candidate->Init(timeline, definitions);
    }
    model->GeneratePath({0.3, -0.7}, &path);
    legacy->GeneratePath({0.3, -0.7}, &legacyPath);
    for (size_t i = 0; i < path.size(); ++i) {
        ASSERT_NEAR(path[i].numeraire_, legacyPath[i].numeraire_, 1e-14);
        ASSERT_NEAR(path[i].observations_[0], legacyPath[i].observations_[0], 1e-14);
    }
    auto clone = model->Clone();
    ASSERT_NE(clone->Parameters()[2], model->Parameters()[2]);
    *clone->Parameters()[2] = 0.05;
    ASSERT_EQ(*model->Parameters()[2], 0.02);
    clone->Init(timeline, definitions);
    clone->GeneratePath({0.3, -0.7}, &legacyPath);
    ASSERT_GT(std::abs(legacyPath.back().observations_[0] - path.back().observations_[0]), 1e-5);
}

TEST(GSRMultiFactorTest, TestInputValidationRejectsMalformedFactorsParametersAndCorrelations) {
    auto settings = Settings(2);
    auto invalid = settings;
    invalid.factorNames_[1] = "FACTOR0";
    ASSERT_THROW(Dal::MultiFactorGSRVolData_("invalid", invalid), Dal::Exception_);
    invalid = settings;
    invalid.gValues_(0, 0) = -0.01;
    ASSERT_THROW(Dal::MultiFactorGSRVolData_("invalid", invalid), Dal::Exception_);
    invalid = settings;
    invalid.hValues_(0, 0) = std::numeric_limits<double>::infinity();
    ASSERT_THROW(Dal::MultiFactorGSRVolData_("invalid", invalid), Dal::Exception_);
    invalid = settings;
    invalid.correlations_(0, 1) = invalid.correlations_(1, 0) = 1.01;
    ASSERT_THROW(Dal::MultiFactorGSRVolData_("invalid", invalid), Dal::Exception_);
    invalid = settings;
    invalid.correlations_(0, 1) = 0.2;
    ASSERT_THROW(Dal::MultiFactorGSRVolData_("invalid", invalid), Dal::Exception_);
    invalid = settings;
    invalid.gValues_ = Matrix_<>(1, 1, 0.02);
    ASSERT_THROW(Dal::MultiFactorGSRVolData_("invalid", invalid), Dal::Exception_);
    invalid = settings;
    invalid.gKnotDates_.push_back(invalid.gKnotDates_.front());
    invalid.gValues_ = Matrix_<>(2, 2, 0.02);
    ASSERT_THROW(Dal::MultiFactorGSRVolData_("invalid", invalid), Dal::Exception_);
    auto model = Dal::CreateModel<double>(Data(settings));
    const auto definitions = BondDefinitions(2);
    model->Allocate({0.0, 1.0}, definitions);
    ASSERT_THROW(model->Init({0.0, 1.0}, Vector_<Dal::AAD::SampleDef_>(1)), Dal::Exception_);
    model->Init({0.0, 1.0}, definitions);
    ASSERT_THROW(model->GeneratePath({0.0}, nullptr), Dal::Exception_);
    ASSERT_FALSE(model->ValidParameterValue(2, -0.01));
    ASSERT_TRUE(model->ValidParameterValue(4, -1.0));
}

TEST(GSRMultiFactorTest, TestMultiFactorAdjointsMatchReinitialisedParameterBumps) {
    auto settings = Settings(2);
    settings.hValues_(1, 0) = -0.4;
    settings.correlations_(0, 1) = settings.correlations_(1, 0) = 0.3;
    const auto data = Data(settings);
    const Vector_<> timeline{0.0, 1.0}, gaussian{0.7, -0.4};
    const auto definitions = BondDefinitions(2);
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
    model->GeneratePath(gaussian, &path);
    Dal::AAD::Number_ payoff = path[1].observations_[0] / path[1].numeraire_;
    Dal::AAD::Adjoint(payoff) = 1.0;
    Dal::AAD::PropagateToStart(*Dal::AAD::Tape());
    auto bumped = Dal::CreateModel<double>(data);
    bumped->Allocate(timeline, definitions);
    Dal::AAD::Scenario_<> bumpedPath;
    Dal::AAD::AllocatePath(definitions, bumpedPath);
    for (size_t i = 0; i < model->Parameters().size(); ++i) {
        const double original = *bumped->Parameters()[i];
        double values[2];
        for (int direction = 0; direction < 2; ++direction) {
            *bumped->Parameters()[i] = original + (direction == 0 ? -1e-6 : 1e-6);
            bumped->Init(timeline, definitions);
            bumped->GeneratePath(gaussian, &bumpedPath);
            values[direction] = bumpedPath[1].observations_[0] / bumpedPath[1].numeraire_;
        }
        *bumped->Parameters()[i] = original;
        ASSERT_NEAR(Dal::AAD::Adjoint(*model->Parameters()[i]), (values[1] - values[0]) / 2e-6, 1e-8) << model->ParameterLabels()[i];
    }
}

TEST(GSRMultiFactorTest, TestArchiveAndBrownianBridgeRepriceDiscountedBond) {
    auto settings = Settings(2);
    settings.hValues_(1, 0) = 0.4;
    const auto original = Data(settings);
    const auto restored = Dal::handle_cast<Dal::ModelData_>(Dal::JSON::ReadString(Dal::JSON::WriteString(*original), false));
    auto model = Dal::CreateModel<double>(restored);
    ASSERT_EQ(model->NumFactors(), 2U);
    ASSERT_EQ(model->ParameterLabels(), Dal::CreateModel<double>(original)->ParameterLabels());
    const Dal::Script::ScriptProductData_ product("bond", {Dal::Cell_(Date_(2027, 10, 2))}, {"pay PAYS FIX(IR[USD,DF,2028-10-01])"});
    Dal::Script::ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = Date_(2026, 10, 2);
    Dal::Script::MonteCarloSettings_ simulation;
    simulation.useBb_ = true;
    const auto result = Dal::Script::MCSimulation<double>(product, restored, 16384, valuation, simulation);
    ASSERT_NEAR(result.aggregated_ / 16384.0, std::exp(-0.06), 5e-5);
}

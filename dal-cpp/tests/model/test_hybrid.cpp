//
// Created by Codex on 2026/9/27.
//

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <future>
#include <limits>
#include <string>

#include <dal/curve/tapeguard.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/model/factory.hpp>
#include <dal/model/hybrid.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/simulation.hpp>
#include <dal/storage/json.hpp>

using namespace Dal;

namespace {
    Matrix_<> HybridCorrelation(double rho = 0.35) {
        Matrix_<> correlations(2, 2, rho);
        correlations(0, 0) = correlations(1, 1) = 1.0;
        return correlations;
    }

    HybridSettings_ TwoEquitySettings(bool reverseComponents = false) {
        HybridSettings_ settings;
        settings.domesticCurrency_ = "USD";
        settings.components_ = {
            Handle_<HybridComponentData_>(new HybridDeterministicRateData_("usd", "USD", 0.05)),
            Handle_<HybridComponentData_>(new HybridBSEquityData_("aaa", "EQ[AAA]", "USD", "W_AAA", 100.0, 0.2, 0.01)),
            Handle_<HybridComponentData_>(new HybridBSEquityData_("bbb", "EQ[BBB]", "USD", "W_BBB", 120.0, 0.3, 0.02)),
        };
        if (reverseComponents)
            std::reverse(settings.components_.begin(), settings.components_.end());
        settings.correlation_ = Handle_<HybridCorrelationData_>(new HybridConstantCorrelationData_("corr", {"W_AAA", "W_BBB"}, HybridCorrelation()));
        return settings;
    }

    Handle_<ModelData_> HybridData(const HybridSettings_& settings) { return Handle_<ModelData_>(new HybridModelData_("hybrid", settings)); }

    void ExpectHybridError(const HybridSettings_& settings, const String_& message) {
        try {
            static_cast<void>(CreateModel<double>(HybridData(settings)));
            FAIL() << "invalid hybrid setup accepted";
        } catch (const Exception_& error) {
            ASSERT_NE(std::string(error.what()).find(message.c_str()), std::string::npos) << "Expected: " << message << "; actual: " << error.what();
        }
    }
} // namespace

TEST(ModelTest, TestHybridTwoEquitiesMatchCorrelatedBSPath) {
    const auto settings = TwoEquitySettings();
    const Handle_<ModelData_> data(std::make_shared<HybridModelData_>("hybrid", settings));
    auto hybrid = CreateModel<double>(data);
    AAD::CorrelatedBlackScholes_<> baseline({"EQ[AAA]", "EQ[BBB]"}, {100.0, 120.0}, {0.2, 0.3}, {0.01, 0.02}, 0.05, HybridCorrelation());
    const Vector_<> timeline{0.0, 0.5, 1.0};
    Vector_<AAD::SampleDef_> definitions(3);
    for (auto& definition : definitions)
        definition.indexNames_ = {"EQ[BBB]", "EQ[AAA]"};
    hybrid->Allocate(timeline, definitions);
    baseline.Allocate(timeline, definitions);
    hybrid->Init(timeline, definitions);
    baseline.Init(timeline, definitions);
    AAD::Scenario_<> hybridPath;
    AAD::Scenario_<> baselinePath;
    AAD::AllocatePath(definitions, hybridPath);
    AAD::AllocatePath(definitions, baselinePath);
    AAD::InitializePath(hybridPath);
    AAD::InitializePath(baselinePath);
    const Vector_<> gaussian{0.4, -0.7, 0.2, 0.5};
    hybrid->GeneratePath(gaussian, &hybridPath);
    baseline.GeneratePath(gaussian, &baselinePath);
    for (size_t date = 0; date < timeline.size(); ++date) {
        ASSERT_NEAR(hybridPath[date].numeraire_, baselinePath[date].numeraire_, 1e-12);
        for (size_t output = 0; output < 2; ++output)
            ASSERT_NEAR(hybridPath[date].observations_[output], baselinePath[date].observations_[output], 1e-12);
    }
}

TEST(ModelTest, TestHybridComponentOrderAndNamedCorrelationAreStable) {
    auto forward = CreateModel<double>(HybridData(TwoEquitySettings()));
    auto reversedSettings = TwoEquitySettings(true);
    reversedSettings.correlation_ =
        Handle_<HybridCorrelationData_>(new HybridConstantCorrelationData_("corr", {"W_BBB", "W_AAA"}, HybridCorrelation()));
    auto reversed = CreateModel<double>(HybridData(reversedSettings));
    const Vector_<> timeline{0.0, 0.5, 1.0};
    Vector_<AAD::SampleDef_> definitions(3);
    for (auto& definition : definitions)
        definition.indexNames_ = {"EQ[BBB]", "EQ[AAA]"};
    for (auto* model : {forward.get(), reversed.get()}) {
        model->Allocate(timeline, definitions);
        model->Init(timeline, definitions);
    }
    auto forwardPath = AAD::Scenario_<>();
    auto reversedPath = AAD::Scenario_<>();
    AAD::AllocatePath(definitions, forwardPath);
    AAD::AllocatePath(definitions, reversedPath);
    AAD::InitializePath(forwardPath);
    AAD::InitializePath(reversedPath);
    const Vector_<> gaussian{0.2, -0.3, 0.7, 0.4};
    forward->GeneratePath(gaussian, &forwardPath);
    reversed->GeneratePath(gaussian, &reversedPath);
    for (size_t date = 0; date < timeline.size(); ++date)
        for (size_t slot = 0; slot < 2; ++slot)
            ASSERT_DOUBLE_EQ(forwardPath[date].observations_[slot], reversedPath[date].observations_[slot]);
    ASSERT_EQ(forward->ParameterLabels(), reversed->ParameterLabels());
}

TEST(ModelTest, TestHybridThreeFactorCorrelationFollowsNames) {
    auto settings = TwoEquitySettings();
    settings.components_.push_back(Handle_<HybridComponentData_>(new HybridBSEquityData_("ccc", "EQ[CCC]", "USD", "W_CCC", 80.0, 0.25, 0.0)));
    Matrix_<> correlations(3, 3, 0.0);
    for (int i = 0; i < 3; ++i)
        correlations(i, i) = 1.0;
    correlations(0, 1) = correlations(1, 0) = 0.2;
    correlations(0, 2) = correlations(2, 0) = 0.3;
    correlations(1, 2) = correlations(2, 1) = 0.4;
    settings.correlation_ = Handle_<HybridCorrelationData_>(new HybridConstantCorrelationData_("corr", {"W_AAA", "W_BBB", "W_CCC"}, correlations));
    auto forward = CreateModel<double>(HybridData(settings));

    std::reverse(settings.components_.begin(), settings.components_.end());
    Matrix_<> reordered(3, 3, 0.0);
    const std::array<int, 3> oldSlots{2, 0, 1};
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            reordered(i, j) = correlations(oldSlots[i], oldSlots[j]);
    settings.correlation_ = Handle_<HybridCorrelationData_>(new HybridConstantCorrelationData_("corr", {"W_CCC", "W_AAA", "W_BBB"}, reordered));
    auto reversed = CreateModel<double>(HybridData(settings));

    const Vector_<> timeline{0.5, 1.0};
    Vector_<AAD::SampleDef_> definitions(2);
    for (auto& definition : definitions)
        definition.indexNames_ = {"EQ[CCC]", "EQ[AAA]", "EQ[BBB]"};
    AAD::Scenario_<> forwardPath;
    AAD::Scenario_<> reversedPath;
    AAD::AllocatePath(definitions, forwardPath);
    AAD::AllocatePath(definitions, reversedPath);
    AAD::InitializePath(forwardPath);
    AAD::InitializePath(reversedPath);
    for (auto* model : {forward.get(), reversed.get()}) {
        model->Allocate(timeline, definitions);
        model->Init(timeline, definitions);
    }
    const Vector_<> gaussian{0.4, -0.7, 0.2, -0.3, 0.1, 0.8};
    forward->GeneratePath(gaussian, &forwardPath);
    reversed->GeneratePath(gaussian, &reversedPath);
    for (size_t sample = 0; sample < timeline.size(); ++sample)
        for (size_t output = 0; output < 3; ++output)
            ASSERT_DOUBLE_EQ(forwardPath[sample].observations_[output], reversedPath[sample].observations_[output]);
}

TEST(ModelTest, TestHybridArchiveAndFactoryRoundTrip) {
    const HybridModelData_ data("hybrid", TwoEquitySettings());
    const auto restored = handle_cast<HybridModelData_>(JSON::ReadString(JSON::WriteString(data), false));
    ASSERT_TRUE(restored);
    ASSERT_EQ(restored->components_.size(), 3);
    ASSERT_EQ(restored->parameterLabels_, data.parameterLabels_);
    auto model = CreateModel<double>(Handle_<ModelData_>(restored));
    ASSERT_EQ(model->NumFactors(), 2);
    ASSERT_TRUE(model->NumeraireIsDeterministic());
    const Vector_<> timeline{1.0};
    Vector_<AAD::SampleDef_> definitions(1);
    definitions[0].indexNames_ = {"EQ[AAA]", "EQ[BBB]"};
    model->Allocate(timeline, definitions);
    model->Init(timeline, definitions);
    AAD::Scenario_<> path;
    AAD::AllocatePath(definitions, path);
    AAD::InitializePath(path);
    model->GeneratePath({0.0, 0.0}, &path);
    ASSERT_NEAR(path[0].numeraire_, std::exp(0.05), 1e-12);
    auto bridge = Script::CreateRNG("sobol", *model, true);
    ASSERT_EQ(bridge->NDim(), 2);
}

TEST(ModelTest, TestHybridRejectsInvalidSetupBeforePathGeneration) {
    auto settings = TwoEquitySettings();
    settings.components_.push_back(Handle_<HybridComponentData_>(new HybridBSEquityData_("aaa", "EQ[CCC]", "USD", "W_CCC", 90.0, 0.2, 0.0)));
    ExpectHybridError(settings, "DuplicateHybridComponent");
    settings = TwoEquitySettings();
    settings.components_.push_back(Handle_<HybridComponentData_>(new HybridDeterministicRateData_("other_rate", "USD", 0.06)));
    ExpectHybridError(settings, "InvalidHybridNumeraire");
    settings = TwoEquitySettings();
    settings.components_.erase(settings.components_.begin());
    ExpectHybridError(settings, "InvalidHybridNumeraire");
    settings = TwoEquitySettings();
    settings.components_[1] = Handle_<HybridComponentData_>(new HybridBSEquityData_("aaa", "EQ[AAA]", "EUR", "W_AAA", 100.0, 0.2, 0.01));
    ExpectHybridError(settings, "InvalidHybridCurrency");
    settings = TwoEquitySettings();
    settings.components_[2] = Handle_<HybridComponentData_>(new HybridBSEquityData_("bbb", "EQ[AAA]", "USD", "W_BBB", 120.0, 0.3, 0.02));
    ExpectHybridError(settings, "DuplicateHybridObservable");
    settings = TwoEquitySettings();
    settings.components_[2] = Handle_<HybridComponentData_>(new HybridBSEquityData_("bbb", "EQ[BBB]", "USD", "W_AAA", 120.0, 0.3, 0.02));
    ExpectHybridError(settings, "DuplicateHybridFactor");
    settings = TwoEquitySettings();
    settings.correlation_ = Handle_<HybridCorrelationData_>(new HybridConstantCorrelationData_("corr", {"W_AAA", "W_MISSING"}, HybridCorrelation()));
    ExpectHybridError(settings, "missing factor");
    settings = TwoEquitySettings();
    settings.correlation_ = Handle_<HybridCorrelationData_>(new HybridConstantCorrelationData_("corr", {"W_AAA", "W_AAA"}, HybridCorrelation()));
    ExpectHybridError(settings, "duplicate factor");
    settings = TwoEquitySettings();
    settings.correlation_ = Handle_<HybridCorrelationData_>(new HybridConstantCorrelationData_("corr", {"W_AAA", "W_BBB"}, HybridCorrelation(1.0)));
    ExpectHybridError(settings, "positive definite");
    auto model = CreateModel<double>(HybridData(TwoEquitySettings()));
    const Vector_<> timeline{1.0};
    Vector_<AAD::SampleDef_> definitions(1);
    definitions[0].indexNames_ = {"EQ[CCC]"};
    ASSERT_THROW(model->Allocate(timeline, definitions), Exception_);
}

TEST(ModelTest, TestHybridRejectsParametersChangedAfterConstruction) {
    const auto check = [](size_t parameterSlot, double invalidValue) {
        auto model = CreateModel<double>(HybridData(TwoEquitySettings()));
        *model->Parameters()[parameterSlot] = invalidValue;
        const Vector_<> timeline{1.0};
        Vector_<AAD::SampleDef_> definitions(1);
        model->Allocate(timeline, definitions);
        ASSERT_THROW(model->Init(timeline, definitions), Exception_);
    };
    check(0, -1.0);
    check(1, -0.2);
    check(6, std::numeric_limits<double>::quiet_NaN());
}

TEST(ModelTest, TestHybridAadComponentRisksAndCloneOwnership) {
    const TapeGuard_ guard(AAD::Tape());
    auto original = CreateModel<AAD::Number_>(HybridData(TwoEquitySettings()));
    auto model = original->Clone();
    ASSERT_EQ(model->NumParams(), 7);
    for (size_t i = 0; i < model->NumParams(); ++i)
        ASSERT_NE(model->Parameters()[i], original->Parameters()[i]);
    const Vector_<> timeline{1.0};
    Vector_<AAD::SampleDef_> definitions(1);
    definitions[0].indexNames_ = {"EQ[AAA]", "EQ[BBB]"};
    model->Allocate(timeline, definitions);
    AAD::Scenario_<AAD::Number_> path;
    AAD::AllocatePath(definitions, path);
    AAD::InitializePath(path);
    AAD::Rewind(*AAD::Tape());
    for (auto* parameter : model->Parameters())
        AAD::PutOnTape(*parameter);
    AAD::NewRecording(*AAD::Tape());
    model->Init(timeline, definitions);
    model->GeneratePath({0.4, -0.7}, &path);
    AAD::Number_ payoff = path[0].observations_[0] * path[0].observations_[1] / path[0].numeraire_;
    const double value = AAD::Value(payoff);
    AAD::Adjoint(payoff) = 1.0;
    AAD::PropagateToStart(*AAD::Tape());
    ASSERT_NEAR(AAD::Adjoint(*model->Parameters()[0]), value / 100.0, 1e-8);
    ASSERT_NEAR(AAD::Adjoint(*model->Parameters()[1]), value * (0.4 - 0.2), 1e-8);
    ASSERT_NEAR(AAD::Adjoint(*model->Parameters()[3]), value / 120.0, 1e-8);
    ASSERT_NEAR(AAD::Adjoint(*model->Parameters()[4]), value * (0.35 * 0.4 - std::sqrt(1.0 - 0.35 * 0.35) * 0.7 - 0.3), 1e-8);
    ASSERT_NEAR(AAD::Adjoint(*model->Parameters()[2]), -value, 1e-8);
    ASSERT_NEAR(AAD::Adjoint(*model->Parameters()[5]), -value, 1e-8);
    ASSERT_NEAR(AAD::Adjoint(*model->Parameters()[6]), value, 1e-8);
}

TEST(ModelTest, TestHybridOneEquityMatchesLegacyBlackScholes) {
    auto settings = TwoEquitySettings();
    settings.components_.pop_back();
    settings.correlation_ = Handle_<HybridCorrelationData_>(new HybridConstantCorrelationData_("corr", {"W_AAA"}, Matrix_<>(1, 1, 1.0)));
    auto hybrid = CreateModel<double>(HybridData(settings));
    AAD::BlackScholes_<> legacy(100.0, 0.2, 0.05, 0.01);
    const Vector_<> timeline{0.0, 0.4, 1.0};
    Vector_<AAD::SampleDef_> definitions(3);
    for (auto& definition : definitions)
        definition.indexNames_ = {"EQ[AAA]"};
    hybrid->Allocate(timeline, definitions);
    legacy.Allocate(timeline, definitions);
    hybrid->Init(timeline, definitions);
    legacy.Init(timeline, definitions);
    AAD::Scenario_<> hybridPath;
    AAD::Scenario_<> legacyPath;
    AAD::AllocatePath(definitions, hybridPath);
    AAD::AllocatePath(definitions, legacyPath);
    AAD::InitializePath(hybridPath);
    AAD::InitializePath(legacyPath);
    for (const Vector_<> gaussian : {Vector_<>{0.3, -0.8}, Vector_<>{-0.5, 0.6}}) {
        hybrid->GeneratePath(gaussian, &hybridPath);
        legacy.GeneratePath(gaussian, &legacyPath);
        for (size_t sample = 0; sample < timeline.size(); ++sample) {
            ASSERT_NEAR(hybridPath[sample].observations_[0], legacyPath[sample].observations_[0], 1e-12);
            ASSERT_NEAR(hybridPath[sample].numeraire_, legacyPath[sample].numeraire_, 1e-12);
        }
    }
}

TEST(ModelTest, TestHybridTwoEquityMomentsAndEuropeanPayoff) {
    auto model = CreateModel<double>(HybridData(TwoEquitySettings()));
    const Vector_<> timeline{1.25};
    Vector_<AAD::SampleDef_> definitions(1);
    definitions[0].indexNames_ = {"EQ[AAA]", "EQ[BBB]"};
    model->Allocate(timeline, definitions);
    model->Init(timeline, definitions);
    AAD::Scenario_<> path;
    AAD::AllocatePath(definitions, path);
    AAD::InitializePath(path);
    constexpr std::array<double, 7> nodes{-3.750439717725742, -2.366759410734542, -1.154405394739968, 0.0,
                                          1.154405394739968,  2.366759410734542,  3.750439717725742};
    constexpr std::array<double, 7> weights{0.0005482688559722182, 0.03075712396758651, 0.2401231786050127,   0.4571428571428571,
                                            0.2401231786050127,    0.03075712396758651, 0.0005482688559722182};
    double meanA = 0.0;
    double meanB = 0.0;
    double meanLogA = 0.0;
    double meanLogB = 0.0;
    double meanLogProduct = 0.0;
    double european = 0.0;
    for (size_t i = 0; i < nodes.size(); ++i)
        for (size_t j = 0; j < nodes.size(); ++j) {
            const double weight = weights[i] * weights[j];
            model->GeneratePath({nodes[i], nodes[j]}, &path);
            const double a = path[0].observations_[0];
            const double b = path[0].observations_[1];
            meanA += weight * a;
            meanB += weight * b;
            meanLogA += weight * std::log(a);
            meanLogB += weight * std::log(b);
            meanLogProduct += weight * std::log(a) * std::log(b);
            european += weight * a * b / path[0].numeraire_;
        }
    ASSERT_NEAR(meanA, 100.0 * std::exp((0.05 - 0.01) * 1.25), 1e-6);
    ASSERT_NEAR(meanB, 120.0 * std::exp((0.05 - 0.02) * 1.25), 1e-6);
    ASSERT_NEAR(meanLogProduct - meanLogA * meanLogB, 0.2 * 0.3 * 0.35 * 1.25, 1e-11);
    ASSERT_NEAR(european, 100.0 * 120.0 * std::exp((0.05 - 0.01 - 0.02 + 0.2 * 0.3 * 0.35) * 1.25), 1e-5);
}

TEST(ModelTest, TestHybridClonesAreDeterministicAcrossThreads) {
    auto original = CreateModel<double>(HybridData(TwoEquitySettings()));
    const Vector_<> timeline{0.0, 0.5, 1.0};
    Vector_<AAD::SampleDef_> definitions(3);
    for (auto& definition : definitions)
        definition.indexNames_ = {"EQ[AAA]", "EQ[BBB]"};
    original->Allocate(timeline, definitions);
    original->Init(timeline, definitions);
    auto clone = original->Clone();
    const auto run = [&](const AAD::Model_<double>* model) {
        AAD::Scenario_<> path;
        AAD::AllocatePath(definitions, path);
        AAD::InitializePath(path);
        double sum = 0.0;
        for (size_t i = 0; i < 100; ++i) {
            model->GeneratePath({0.01 * static_cast<double>(i), -0.2, 0.3, -0.1}, &path);
            sum += path.back().observations_[0] + path.back().observations_[1];
        }
        return sum;
    };
    auto future = std::async(std::launch::async, run, clone.get());
    ASSERT_DOUBLE_EQ(run(original.get()), future.get());
}

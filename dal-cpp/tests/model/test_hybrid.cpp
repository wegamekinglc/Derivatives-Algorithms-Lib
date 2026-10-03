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

TEST(ModelTest, TestHybridFlatLogDfRateMatchesConstantRatePath) {
    auto curveSettings = TwoEquitySettings();
    curveSettings.components_[0] = Handle_<HybridComponentData_>(new HybridLogDfRateData_("usd", "USD", {0.0, 0.5, 1.5}, {0.0, -0.025, -0.075}));
    auto curve = CreateModel<double>(HybridData(curveSettings));
    auto constant = CreateModel<double>(HybridData(TwoEquitySettings()));
    const Vector_<> timeline{0.0, 0.25, 1.0};
    Vector_<AAD::SampleDef_> definitions(timeline.size());
    for (auto& definition : definitions) {
        definition.numeraire_ = true;
        definition.indexNames_ = {"EQ[AAA]", "EQ[BBB]"};
    }
    for (auto* model : {curve.get(), constant.get()}) {
        model->Allocate(timeline, definitions);
        model->Init(timeline, definitions);
    }
    AAD::Scenario_<> curvePath, constantPath;
    AAD::AllocatePath(definitions, curvePath);
    AAD::AllocatePath(definitions, constantPath);
    const Vector_<> gaussian{0.4, -0.7, 0.2, 0.5};
    curve->GeneratePath(gaussian, &curvePath);
    constant->GeneratePath(gaussian, &constantPath);
    for (size_t sample = 0; sample < timeline.size(); ++sample) {
        ASSERT_NEAR(curvePath[sample].numeraire_, constantPath[sample].numeraire_, 1e-10);
        for (size_t asset = 0; asset < 2; ++asset)
            ASSERT_NEAR(curvePath[sample].observations_[asset], constantPath[sample].observations_[asset], 1e-10);
    }
    ASSERT_EQ(curve->ParameterLabels().back(), String_("logdf:USD:2"));
}

TEST(ModelTest, TestHybridNonFlatLogDfDrivesNumeraireAndEquityCarry) {
    auto settings = TwoEquitySettings();
    settings.components_[0] =
        Handle_<HybridComponentData_>(new HybridLogDfRateData_("usd", "USD", {0.0, 0.5, 1.0, 2.0}, {0.0, -0.01, -0.035, -0.10}));
    settings.components_[1] = Handle_<HybridComponentData_>(new HybridBSEquityData_("aaa", "EQ[AAA]", "USD", "W_AAA", 100.0, 0.0, 0.0));
    settings.components_[2] = Handle_<HybridComponentData_>(new HybridBSEquityData_("bbb", "EQ[BBB]", "USD", "W_BBB", 120.0, 0.0, 0.0));
    auto model = CreateModel<double>(HybridData(settings));
    const Vector_<> timeline{0.0, 0.25, 0.75, 1.5, 2.5};
    const std::array<double, 5> expectedLogDF{0.0, -0.005, -0.0225, -0.0675, -0.1325};
    Vector_<AAD::SampleDef_> definitions(timeline.size());
    for (auto& definition : definitions) {
        definition.numeraire_ = true;
        definition.indexNames_ = {"EQ[AAA]", "EQ[BBB]"};
    }
    model->Allocate(timeline, definitions);
    model->Init(timeline, definitions);
    AAD::Scenario_<> path;
    AAD::AllocatePath(definitions, path);
    model->GeneratePath(Vector_<>(model->SimDim(), 0.0), &path);
    for (size_t sample = 0; sample < timeline.size(); ++sample) {
        const double numeraire = std::exp(-expectedLogDF[sample]);
        ASSERT_NEAR(path[sample].numeraire_, numeraire, 1e-10);
        ASSERT_NEAR(path[sample].observations_[0], 100.0 * numeraire, 1e-10);
        ASSERT_NEAR(path[sample].observations_[1], 120.0 * numeraire, 1e-10);
    }
}

TEST(ModelTest, TestHybridLogDfInterpolationSchemesAndTailMatchCurveEngine) {
    const Vector_<> nodes{0.0, 0.5, 1.0, 2.0};
    const Vector_<> logDF{0.0, -0.01, -0.04, -0.10};
    const Vector_<> timeline{0.25, 0.75, 1.5, 2.5};
    Vector_<AAD::SampleDef_> definitions(timeline.size());
    for (auto& definition : definitions)
        definition.numeraire_ = true;
    for (const String_& scheme : {String_("LOG_LINEAR"), String_("LOG_CUBIC_NATURAL"), String_("MIXED")}) {
        auto settings = TwoEquitySettings();
        settings.components_[0] = Handle_<HybridComponentData_>(new HybridLogDfRateData_("usd", "USD", nodes, logDF, scheme));
        auto model = CreateModel<double>(HybridData(settings));
        model->Allocate(timeline, definitions);
        model->Init(timeline, definitions);
        AAD::Scenario_<> path;
        AAD::AllocatePath(definitions, path);
        model->GeneratePath(Vector_<>(model->SimDim(), 0.0), &path);
        const LogDfInterpolation_ interpolation(nodes, LogDfScheme_(scheme));
        for (size_t i = 0; i < timeline.size(); ++i)
            ASSERT_NEAR(path[i].numeraire_, std::exp(-interpolation.Evaluate(logDF, timeline[i])), 1e-12) << scheme;
    }
}

TEST(ModelTest, TestHybridLogDfDataRejectsInvalidNodes) {
    const auto expectInvalid = [](const Vector_<>& times, const Vector_<>& logDF, const String_& scheme, const std::string& message) {
        try {
            static_cast<void>(HybridLogDfRateData_("usd", "USD", times, logDF, scheme));
            FAIL() << "invalid hybrid curve accepted";
        } catch (const Exception_& error) {
            ASSERT_NE(std::string(error.what()).find(message), std::string::npos) << error.what();
        }
    };
    expectInvalid({0.0}, {0.0}, "LOG_LINEAR", "equal length of at least two");
    expectInvalid({0.0, 1.0}, {0.0}, "LOG_LINEAR", "equal length of at least two");
    expectInvalid({0.1, 1.0}, {0.0, -0.02}, "LOG_LINEAR", "first node");
    expectInvalid({0.0, 1.0}, {0.01, -0.02}, "LOG_LINEAR", "first node");
    expectInvalid({0.0, 1.0, 1.0}, {0.0, -0.02, -0.04}, "LOG_LINEAR", "strictly increasing");
    expectInvalid({0.0, 1.0, 0.5}, {0.0, -0.02, -0.04}, "LOG_LINEAR", "strictly increasing");
    expectInvalid({0.0, std::numeric_limits<double>::infinity()}, {0.0, -0.02}, "LOG_LINEAR", "non-finite");
    expectInvalid({0.0, 1.0}, {0.0, std::numeric_limits<double>::quiet_NaN()}, "LOG_LINEAR", "non-finite");
    expectInvalid({0.0, 1.0}, {0.0, -0.02}, "LOG_CUBIC_NATURAL", "at least 3");
    auto settings = TwoEquitySettings();
    settings.components_[0] = Handle_<HybridComponentData_>(new HybridLogDfRateData_("usd", "EUR", {0.0, 1.0}, {0.0, -0.02}));
    ExpectHybridError(settings, "InvalidHybridCurrency");
    ASSERT_NO_THROW(HybridLogDfRateData_("usd", "USD", {0.0, 1.0}, {0.0, 0.02}));
}

TEST(ModelTest, TestHybridLogDfArchiveAndRiskLabelsRoundTrip) {
    auto settings = TwoEquitySettings();
    settings.components_[0] =
        Handle_<HybridComponentData_>(new HybridLogDfRateData_("usd", "USD", {0.0, 0.5, 1.0}, {0.0, -0.01, -0.03}, "LOG_CUBIC_NATURAL"));
    const HybridModelData_ data("hybrid_curve", settings);
    const auto restored = handle_cast<HybridModelData_>(JSON::ReadString(JSON::WriteString(data), false));
    ASSERT_TRUE(restored);
    ASSERT_EQ(restored->parameterLabels_, data.parameterLabels_);
    ASSERT_EQ(restored->parameterLabels_.back(), String_("logdf:USD:2"));
    auto model = CreateModel<double>(Handle_<ModelData_>(restored));
    ASSERT_TRUE(model->NumeraireIsDeterministic());
    ASSERT_EQ(model->NumParams(), 8);
}

TEST(ModelTest, TestHybridLogDfAadNodeRisksMatchCommonPathDifferences) {
    auto settings = TwoEquitySettings();
    settings.components_.pop_back();
    settings.components_[0] = Handle_<HybridComponentData_>(new HybridLogDfRateData_("usd", "USD", {0.0, 0.5, 1.5}, {0.0, -0.01, -0.06}));
    settings.components_[1] = Handle_<HybridComponentData_>(new HybridBSEquityData_("aaa", "EQ[AAA]", "USD", "W_AAA", 100.0, 0.0, 0.0));
    settings.correlation_ = Handle_<HybridCorrelationData_>(new HybridConstantCorrelationData_("corr", {"W_AAA"}, Matrix_<>(1, 1, 1.0)));
    const auto data = HybridData(settings);
    const Vector_<> timeline{0.75};
    Vector_<AAD::SampleDef_> definitions(1);
    definitions[0].numeraire_ = true;
    definitions[0].indexNames_ = {"EQ[AAA]"};
    const TapeGuard_ guard(AAD::Tape());
    auto model = CreateModel<AAD::Number_>(data);
    model->Allocate(timeline, definitions);
    AAD::Scenario_<AAD::Number_> path;
    AAD::AllocatePath(definitions, path);
    AAD::Rewind(*AAD::Tape());
    for (auto* parameter : model->Parameters())
        AAD::PutOnTape(*parameter);
    AAD::NewRecording(*AAD::Tape());
    model->Init(timeline, definitions);
    model->GeneratePath({0.0}, &path);
    AAD::Number_ payoff = (path[0].observations_[0] - 90.0) / path[0].numeraire_;
    AAD::Adjoint(payoff) = 1.0;
    AAD::PropagateToStart(*AAD::Tape());
    const double discount = std::exp(-0.0225);
    ASSERT_NEAR(AAD::Value(payoff), 100.0 - 90.0 * discount, 1e-10);
    ASSERT_NEAR(AAD::Adjoint(*model->Parameters()[3]), -90.0 * discount * 0.75, 1e-8);
    ASSERT_NEAR(AAD::Adjoint(*model->Parameters()[4]), -90.0 * discount * 0.25, 1e-8);
    auto bumpedPrice = [&](size_t parameter, double bump) {
        auto bumped = CreateModel<double>(data);
        *bumped->Parameters()[parameter] += bump;
        bumped->Allocate(timeline, definitions);
        bumped->Init(timeline, definitions);
        AAD::Scenario_<> bumpedPath;
        AAD::AllocatePath(definitions, bumpedPath);
        bumped->GeneratePath({0.0}, &bumpedPath);
        return (bumpedPath[0].observations_[0] - 90.0) / bumpedPath[0].numeraire_;
    };
    constexpr double BUMP = 1e-5;
    for (size_t node = 3; node < 5; ++node)
        ASSERT_NEAR(AAD::Adjoint(*model->Parameters()[node]), (bumpedPrice(node, BUMP) - bumpedPrice(node, -BUMP)) / (2.0 * BUMP), 1e-7);
    auto clone = model->Clone();
    ASSERT_NE(clone->Parameters()[3], model->Parameters()[3]);
    ASSERT_NE(clone->Parameters()[4], model->Parameters()[4]);
    AAD::Rewind(*AAD::Tape());
    for (auto* parameter : clone->Parameters())
        AAD::PutOnTape(*parameter);
    AAD::NewRecording(*AAD::Tape());
    clone->Init(timeline, definitions);
    AAD::InitializePath(path);
    clone->GeneratePath({0.0}, &path);
    AAD::Number_ clonedPayoff = (path[0].observations_[0] - 90.0) / path[0].numeraire_;
    AAD::Adjoint(clonedPayoff) = 1.0;
    AAD::PropagateToStart(*AAD::Tape());
    ASSERT_NEAR(AAD::Adjoint(*clone->Parameters()[3]), -90.0 * discount * 0.75, 1e-8);
    ASSERT_NEAR(AAD::Adjoint(*clone->Parameters()[4]), -90.0 * discount * 0.25, 1e-8);
}

TEST(ModelTest, TestHybridLogDfAadTwoStepCarryCancelsIntermediateNode) {
    auto settings = TwoEquitySettings();
    settings.components_[0] = Handle_<HybridComponentData_>(new HybridLogDfRateData_("usd", "USD", {0.0, 0.5, 1.0}, {0.0, -0.01, -0.05}));
    settings.components_[1] = Handle_<HybridComponentData_>(new HybridBSEquityData_("aaa", "EQ[AAA]", "USD", "W_AAA", 100.0, 0.0, 0.0));
    settings.components_[2] = Handle_<HybridComponentData_>(new HybridBSEquityData_("bbb", "EQ[BBB]", "USD", "W_BBB", 120.0, 0.0, 0.0));
    const TapeGuard_ guard(AAD::Tape());
    auto model = CreateModel<AAD::Number_>(HybridData(settings));
    const Vector_<> timeline{0.5, 1.0};
    Vector_<AAD::SampleDef_> definitions(2);
    for (auto& definition : definitions) {
        definition.numeraire_ = true;
        definition.indexNames_ = {"EQ[AAA]", "EQ[BBB]"};
    }
    model->Allocate(timeline, definitions);
    AAD::Scenario_<AAD::Number_> path;
    AAD::AllocatePath(definitions, path);
    AAD::Rewind(*AAD::Tape());
    for (auto* parameter : model->Parameters())
        AAD::PutOnTape(*parameter);
    AAD::NewRecording(*AAD::Tape());
    model->Init(timeline, definitions);
    AAD::InitializePath(path);
    AAD::Mark(*AAD::Tape());
    for (int i = 0; i < 2; ++i) {
        AAD::RewindToMark(*AAD::Tape());
        model->GeneratePath({0.0, 0.0, 0.0, 0.0}, &path);
        AAD::Number_ payoff = path[1].observations_[0] * path[1].observations_[1] / path[1].numeraire_;
        AAD::Adjoint(payoff) = 1.0;
        AAD::PropagateToMark(*AAD::Tape());
        ASSERT_NEAR(AAD::Value(payoff), 12000.0 * std::exp(0.05), 1e-8);
    }
    AAD::PropagateMarkToStart(*AAD::Tape());
    ASSERT_NEAR(AAD::Adjoint(*model->Parameters()[6]), 0.0, 1e-8);
    ASSERT_NEAR(AAD::Adjoint(*model->Parameters()[7]), -2.0 * 12000.0 * std::exp(0.05), 1e-8);
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

namespace {
    Handle_<GSRCurveData_> HybridRateCurve() {
        const Date_ today(2026, 10, 2);
        return Handle_<GSRCurveData_>(new GSRCurveData_("curve", today, "USD", {today, today.AddDays(365), today.AddDays(730), today.AddDays(1095)},
                                                        {0.0, -0.03, -0.06, -0.09}, {}, Matrix_<>(0, 0)));
    }

    Handle_<MultiFactorGSRVolData_> HybridRateVol(double levelSlopeCorrelation) {
        const Date_ today(2026, 10, 2);
        Matrix_<> g(2, 1), h(2, 1), correlation(2, 2, levelSlopeCorrelation);
        g(0, 0) = 0.02;
        g(1, 0) = 0.01;
        h(0, 0) = 1.0;
        h(1, 0) = 0.4;
        correlation(0, 0) = correlation(1, 1) = 1.0;
        return Handle_<MultiFactorGSRVolData_>(new MultiFactorGSRVolData_("vol", {"level", "slope"}, {today}, g, {today}, h, correlation));
    }

    HybridSettings_ MultiFactorRateSettings(double levelSlopeCorrelation) {
        HybridSettings_ settings;
        settings.domesticCurrency_ = "USD";
        settings.components_ = {Handle_<HybridComponentData_>(
                                    new HybridGSRRateData_("rate", {"W_LEVEL", "W_SLOPE"}, HybridRateCurve(), HybridRateVol(levelSlopeCorrelation))),
                                Handle_<HybridComponentData_>(new HybridBSEquityData_("equity", "EQ[AAA]", "USD", "W_EQ", 100.0, 0.2, 0.01))};
        Matrix_<> correlations(3, 3, 0.0);
        correlations(0, 0) = correlations(1, 1) = correlations(2, 2) = 1.0;
        correlations(1, 2) = correlations(2, 1) = levelSlopeCorrelation;
        settings.correlation_ =
            Handle_<HybridCorrelationData_>(new HybridConstantCorrelationData_("corr", {"W_EQ", "W_LEVEL", "W_SLOPE"}, correlations));
        return settings;
    }
} // namespace

TEST(ModelTest, TestHybridMultiFactorGSRRateMatchesStandalonePath) {
    auto standalone = CreateModel<double>(Handle_<ModelData_>(new MultiFactorGSRModelData_("rates", HybridRateCurve(), HybridRateVol(0.3))));
    auto hybrid = CreateModel<double>(HybridData(MultiFactorRateSettings(0.3)));
    ASSERT_EQ(hybrid->NumFactors(), 3U);
    const Date_ maturity(2028, 10, 2);
    const Vector_<> timeline{1.0, 2.0};
    Vector_<AAD::SampleDef_> rateDefinitions(2), hybridDefinitions(2);
    for (auto& definition : rateDefinitions)
        definition.indexNames_ = {"IR[USD,DF," + Date::ToString(maturity) + "]"};
    for (auto& definition : hybridDefinitions) {
        definition.numeraire_ = true;
        definition.indexNames_ = {"IR[USD,DF," + Date::ToString(maturity) + "]"};
    }
    standalone->Allocate(timeline, rateDefinitions);
    standalone->Init(timeline, rateDefinitions);
    hybrid->Allocate(timeline, hybridDefinitions);
    hybrid->Init(timeline, hybridDefinitions);
    AAD::Scenario_<> ratePath, hybridPath;
    AAD::AllocatePath(rateDefinitions, ratePath);
    AAD::AllocatePath(hybridDefinitions, hybridPath);
    // Hybrid steps consume [W_EQ, W_LEVEL, W_SLOPE] in registry order; with zero equity-rate
    // correlation the correlated rate factors equal the standalone raw Gaussians pathwise.
    const Vector_<> rateGaussian{0.4, -0.7, 0.2, 0.3};
    const Vector_<> hybridGaussian{0.5, 0.4, -0.7, 0.5, 0.2, 0.3};
    standalone->GeneratePath(rateGaussian, &ratePath);
    hybrid->GeneratePath(hybridGaussian, &hybridPath);
    for (size_t sample = 0; sample < timeline.size(); ++sample) {
        ASSERT_NEAR(hybridPath[sample].numeraire_, ratePath[sample].numeraire_, 1e-12);
        ASSERT_NEAR(hybridPath[sample].observations_[0], ratePath[sample].observations_[0], 1e-12);
    }
}

TEST(ModelTest, TestHybridMultiFactorGSRRequiresMatchingFactorCorrelations) {
    auto settings = MultiFactorRateSettings(0.3);
    Matrix_<> correlations(3, 3, 0.0);
    correlations(0, 0) = correlations(1, 1) = correlations(2, 2) = 1.0;
    settings.correlation_ = Handle_<HybridCorrelationData_>(new HybridConstantCorrelationData_("corr", {"W_EQ", "W_LEVEL", "W_SLOPE"}, correlations));
    auto hybrid = CreateModel<double>(HybridData(settings));
    const Vector_<> timeline{1.0, 2.0};
    Vector_<AAD::SampleDef_> definitions(2);
    for (auto& definition : definitions)
        definition.indexNames_ = {"EQ[AAA]"};
    hybrid->Allocate(timeline, definitions);
    ASSERT_THROW(hybrid->Init(timeline, definitions), Exception_);
}

TEST(ModelTest, TestHybridMultiFactorGSRArchiveRoundTrip) {
    const auto settings = MultiFactorRateSettings(0.3);
    const auto original = Handle_<HybridModelData_>(new HybridModelData_("hybrid", settings));
    const auto restored = handle_cast<HybridModelData_>(JSON::ReadString(JSON::WriteString(*original), false));
    ASSERT_TRUE(restored);
    ASSERT_EQ(restored->parameterLabels_, original->parameterLabels_);
    auto model = CreateModel<double>(Handle_<ModelData_>(restored));
    ASSERT_EQ(model->NumFactors(), 3U);
    ASSERT_EQ(model->ParameterLabels(), CreateModel<double>(Handle_<ModelData_>(original))->ParameterLabels());
    const auto* rateData = dynamic_cast<const HybridGSRRateData_*>(restored->components_[0].get());
    ASSERT_TRUE(rateData && rateData->multiVol_);
    ASSERT_EQ(rateData->FactorNames(), (Vector_<String_>{"W_LEVEL", "W_SLOPE"}));
    ASSERT_EQ(rateData->multiVol_->factorNames_, (Vector_<String_>{"level", "slope"}));
}

namespace {
    Handle_<GSRSLVModelData_> HybridSLVData() {
        const Date_ today(2026, 10, 2);
        const Handle_<GSRCurveData_> curve(new GSRCurveData_("curve", today, "USD", {today, today.AddDays(3650)}, {0.0, -0.3}, {}, Matrix_<>(0, 0)));
        MultiFactorGSRVolSettings_ vol;
        vol.factorNames_ = {"B_RATE"};
        vol.gKnotDates_ = vol.hKnotDates_ = {today};
        vol.gValues_ = Matrix_<>(1, 1, 0.02);
        vol.hValues_ = Matrix_<>(1, 1, 1.0);
        vol.correlations_ = Matrix_<>(1, 1, 1.0);
        const Handle_<MultiFactorGSRModelData_> gaussian(
            new MultiFactorGSRModelData_("gaussian", curve, Handle_<MultiFactorGSRVolData_>(new MultiFactorGSRVolData_("vol", vol))));
        const Handle_<GSRLeverageData_> leverage(new GSRLeverageData_("leverage", {0.0}, {0.0}, Matrix_<>(1, 1, 1.0)));
        GSRSLVSettings_ settings;
        settings.kappa_ = 1.0;
        settings.volOfVol_ = 0.5;
        settings.varianceCorrelations_ = {0.3};
        settings.maxStep_ = 1.5;
        return Handle_<GSRSLVModelData_>(new GSRSLVModelData_("smile", gaussian, leverage, settings));
    }

    HybridSettings_ SLVRateSettings(double rateVolCorrelation) {
        HybridSettings_ settings;
        settings.domesticCurrency_ = "USD";
        settings.components_ = {Handle_<HybridComponentData_>(new HybridGSRSLVRateData_("rate", "C_VOL", "D_BRIDGE", HybridSLVData())),
                                Handle_<HybridComponentData_>(new HybridBSEquityData_("equity", "EQ[AAA]", "USD", "A_EQ", 100.0, 0.2, 0.01))};
        const Vector_<String_> names{"A_EQ", "B_RATE", "C_VOL", "D_BRIDGE"};
        Matrix_<> correlations(4, 4, 0.0);
        for (int i = 0; i < 4; ++i)
            correlations(i, i) = 1.0;
        correlations(1, 2) = correlations(2, 1) = rateVolCorrelation;
        settings.correlation_ = Handle_<HybridCorrelationData_>(new HybridConstantCorrelationData_("corr", names, correlations));
        return settings;
    }
} // namespace

TEST(ModelTest, TestHybridGSRSLVRateMatchesStandalonePath) {
    AAD::GSRSLV_<> standalone(*HybridSLVData());
    auto hybrid = CreateModel<double>(HybridData(SLVRateSettings(0.3)));
    ASSERT_EQ(hybrid->NumFactors(), 4U);
    const Date_ maturity(2028, 10, 2);
    const Vector_<> timeline{1.0, 2.0};
    Vector_<AAD::SampleDef_> definitions(2);
    for (auto& definition : definitions) {
        definition.numeraire_ = true;
        definition.indexNames_ = {"IR[USD,DF," + Date::ToString(maturity) + "]"};
    }
    standalone.Allocate(timeline, definitions);
    standalone.Init(timeline, definitions);
    hybrid->Allocate(timeline, definitions);
    hybrid->Init(timeline, definitions);
    ASSERT_EQ(standalone.SimDim(), 6U);
    ASSERT_EQ(hybrid->SimDim(), 8U);
    AAD::Scenario_<> standalonePath, hybridPath;
    AAD::AllocatePath(definitions, standalonePath);
    AAD::AllocatePath(definitions, hybridPath);
    // The SLV integration grid is {0, 1, 2} in both models: no interior knots and maxStep covers
    // each span. With the equity factor uncorrelated, the hybrid named factors carry the same
    // correlated rate and variance drivers as the standalone Cholesky, so paths agree exactly.
    const Vector_<> standaloneGaussian{0.4, 0.2, 0.7, -0.3, 0.5, 0.1};
    const Vector_<> hybridGaussian{0.6, 0.4, 0.2, 0.7, -0.2, -0.3, 0.5, 0.1};
    standalone.GeneratePath(standaloneGaussian, &standalonePath);
    hybrid->GeneratePath(hybridGaussian, &hybridPath);
    for (size_t sample = 0; sample < timeline.size(); ++sample) {
        ASSERT_NEAR(hybridPath[sample].numeraire_, standalonePath[sample].numeraire_, 1e-12);
        ASSERT_NEAR(hybridPath[sample].observations_[0], standalonePath[sample].observations_[0], 1e-12);
    }
}

TEST(ModelTest, TestHybridGSRSLVRateRequiresMatchingDriverCorrelations) {
    auto hybrid = CreateModel<double>(HybridData(SLVRateSettings(0.0)));
    const Vector_<> timeline{1.0, 2.0};
    Vector_<AAD::SampleDef_> definitions(2);
    for (auto& definition : definitions)
        definition.indexNames_ = {"EQ[AAA]"};
    hybrid->Allocate(timeline, definitions);
    ASSERT_THROW(hybrid->Init(timeline, definitions), Exception_);
}

TEST(ModelTest, TestHybridGSRSLVRateArchiveRoundTrip) {
    const auto settings = SLVRateSettings(0.3);
    const auto original = Handle_<HybridModelData_>(new HybridModelData_("hybrid", settings));
    const auto restored = handle_cast<HybridModelData_>(JSON::ReadString(JSON::WriteString(*original), false));
    ASSERT_TRUE(restored);
    ASSERT_EQ(restored->parameterLabels_, original->parameterLabels_);
    auto model = CreateModel<double>(Handle_<ModelData_>(restored));
    ASSERT_EQ(model->NumFactors(), 4U);
    ASSERT_EQ(model->ParameterLabels(), CreateModel<double>(Handle_<ModelData_>(original))->ParameterLabels());
    ASSERT_NE(std::find(model->ParameterLabels().begin(), model->ParameterLabels().end(), String_("kappa")), model->ParameterLabels().end());
}

TEST(ModelTest, TestHybridRejectsSingularMultiFactorGSRKernel) {
    const Date_ today(2026, 10, 2);
    const Handle_<GSRCurveData_> curve(new GSRCurveData_("curve", today, "USD", {today, today.AddDays(365)}, {0.0, -0.03}, {}, Matrix_<>(0, 0)));
    Matrix_<> g(2, 1, 0.02), h(2, 1, 1.0), correlation(2, 2, 1.0);
    const Handle_<MultiFactorGSRVolData_> vol(new MultiFactorGSRVolData_("vol", {"level", "slope"}, {today}, g, {today}, h, correlation));
    auto settings = MultiFactorRateSettings(0.3);
    settings.components_[0] = Handle_<HybridComponentData_>(new HybridGSRRateData_("rate", {"W_LEVEL", "W_SLOPE"}, curve, vol));
    EXPECT_THROW(static_cast<void>(CreateModel<double>(HybridData(settings))), Exception_);
}

TEST(ModelTest, TestAssembleHybridCorrelationMatchesComponentBlocks) {
    auto settings = MultiFactorRateSettings(0.3);
    settings.correlation_ = AssembleHybridCorrelation("corr", settings.components_, {HybridFactorLink_{"W_EQ", "W_LEVEL", -0.2}});
    auto hybrid = CreateModel<double>(HybridData(settings));
    ASSERT_EQ(hybrid->NumFactors(), 3U);
    const Vector_<> timeline{1.0, 2.0};
    Vector_<AAD::SampleDef_> definitions(2);
    for (auto& definition : definitions) {
        definition.numeraire_ = true;
        definition.indexNames_ = {"EQ[AAA]", "IR[USD,DF,2028-10-02]"};
    }
    hybrid->Allocate(timeline, definitions);
    hybrid->Init(timeline, definitions);
    AAD::Scenario_<> path;
    AAD::AllocatePath(definitions, path);
    ASSERT_NO_THROW(hybrid->GeneratePath({0.4, -0.7, 0.2, 0.5, 0.2, 0.3}, &path));
}

TEST(ModelTest, TestAssembleHybridCorrelationAssemblesSLVDriverBlock) {
    auto settings = SLVRateSettings(0.3);
    settings.correlation_ = AssembleHybridCorrelation("corr", settings.components_);
    auto hybrid = CreateModel<double>(HybridData(settings));
    ASSERT_EQ(hybrid->NumFactors(), 4U);
    const Vector_<> timeline{1.0, 2.0};
    Vector_<AAD::SampleDef_> definitions(2);
    for (auto& definition : definitions) {
        definition.numeraire_ = true;
        definition.indexNames_ = {"EQ[AAA]", "IR[USD,DF,2028-10-02]"};
    }
    hybrid->Allocate(timeline, definitions);
    hybrid->Init(timeline, definitions);
    AAD::Scenario_<> path;
    AAD::AllocatePath(definitions, path);
    ASSERT_NO_THROW(hybrid->GeneratePath({0.6, 0.4, 0.2, 0.7, -0.2, -0.3, 0.5, 0.1}, &path));
}

TEST(ModelTest, TestAssembleHybridCorrelationRejectsInvalidLinks) {
    const auto settings = MultiFactorRateSettings(0.3);
    { // unknown factor
        ASSERT_THROW(static_cast<void>(AssembleHybridCorrelation("corr", settings.components_, {HybridFactorLink_{"W_EQ", "NOPE", 0.1}})),
                     Exception_);
    }
    { // link over an intra-block pair (already specified by the kernel)
        ASSERT_THROW(static_cast<void>(AssembleHybridCorrelation("corr", settings.components_, {HybridFactorLink_{"W_LEVEL", "W_SLOPE", 0.0}})),
                     Exception_);
    }
    { // duplicate link
        ASSERT_THROW(static_cast<void>(AssembleHybridCorrelation(
                         "corr", settings.components_, {HybridFactorLink_{"W_EQ", "W_LEVEL", 0.1}, HybridFactorLink_{"W_LEVEL", "W_EQ", 0.2}})),
                     Exception_);
    }
}

TEST(ModelTest, TestHybridSLVRejectsOffDayGridLeverageBreakpoints) {
    const Date_ today(2026, 10, 2);
    const Handle_<GSRCurveData_> curve(new GSRCurveData_("curve", today, "USD", {today, today.AddDays(3650)}, {0.0, -0.3}, {}, Matrix_<>(0, 0)));
    MultiFactorGSRVolSettings_ vol;
    vol.factorNames_ = {"B_RATE"};
    vol.gKnotDates_ = vol.hKnotDates_ = {today};
    vol.gValues_ = Matrix_<>(1, 1, 0.02);
    vol.hValues_ = Matrix_<>(1, 1, 1.0);
    vol.correlations_ = Matrix_<>(1, 1, 1.0);
    const Handle_<MultiFactorGSRModelData_> gaussian(
        new MultiFactorGSRModelData_("gaussian", curve, Handle_<MultiFactorGSRVolData_>(new MultiFactorGSRVolData_("vol", vol))));
    // 182.5 days falls between calendar days
    const Handle_<GSRLeverageData_> leverage(new GSRLeverageData_("leverage", {0.0}, {0.0, 182.5 / 365.0}, Matrix_<>(1, 2, 1.0)));
    const Handle_<GSRSLVModelData_> slv(new GSRSLVModelData_("smile", gaussian, leverage, GSRSLVSettings_()));
    HybridSettings_ settings;
    settings.domesticCurrency_ = "USD";
    settings.components_ = {Handle_<HybridComponentData_>(new HybridGSRSLVRateData_("rate", "C_VOL", "D_BRIDGE", slv)),
                            Handle_<HybridComponentData_>(new HybridBSEquityData_("equity", "EQ[AAA]", "USD", "A_EQ", 100.0, 0.2, 0.01))};
    settings.correlation_ = AssembleHybridCorrelation("corr", settings.components_);
    ASSERT_THROW(static_cast<void>(CreateModel<double>(HybridData(settings))), Exception_);
}

//
// Created by Codex on 2026/10/9.
//

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <optional>

#include <dal/math/aad/native.hpp>
#include <dal/math/aad/recording.hpp>
#include <dal/model/blackscholessteps.hpp>
#include <dal/platform/platform.hpp>

using Dal::Vector_;
namespace AAD = Dal::AAD;

namespace {
    struct StepRisk_ {
        double value_;
        Vector_<> gradient_;
        double stateRisk_;
    };

    StepRisk_ StepPaymentRisk(const Vector_<>& timeline,
                              const Vector_<AAD::SampleDef_>& definitions,
                              const Vector_<>& values,
                              const Vector_<>& gaussian,
                              bool full,
                              const std::optional<std::pair<size_t, double>>& boundary = {}) {
        const auto mode = AAD::SetNumResultsForAAD(false, 1);
        AAD::RecordingScope_ recording;
        Vector_<AAD::Number_> parameters(values.size());
        for (size_t i = 0; i < values.size(); ++i)
            recording.RegisterInput(parameters[i], values[i]);
        AAD::Number_ initial;
        if (boundary)
            recording.RegisterInput(initial, boundary->second);
        recording.StartRecording();
        AAD::Number_ root;
        if (full) {
            AAD::BlackScholes_<AAD::Number_> model(parameters[0], parameters[1], parameters[2], parameters[3]);
            model.Allocate(timeline, definitions);
            model.Init(timeline, definitions);
            AAD::Scenario_<AAD::Number_> path;
            AAD::AllocatePath(definitions, path);
            AAD::InitializePath(path);
            model.GeneratePath(gaussian, &path);
            root = path.back().spot_ * path.back().discounts_[0] / path.back().numeraire_;
        } else {
            const AAD::BlackScholesStepPlan_ plan(timeline, definitions);
            AAD::Number_ logSpot = boundary ? initial : plan.InitialLogSpot(parameters);
            AAD::Sample_<AAD::Number_> sample;
            for (size_t i = boundary ? boundary->first : 0; i < timeline.size(); ++i) {
                auto step = plan.Advance(i, logSpot, parameters, gaussian);
                logSpot = std::move(step.logSpot_);
                sample = std::move(step.sample_);
            }
            root = sample.spot_ * sample.discounts_[0] / sample.numeraire_;
        }
        recording.FinishRecording();
        AAD::NativeOperations_::AddSeed(root, 1.0);
        recording.Reverse();
        StepRisk_ result{AAD::Value(root), Vector_<>(parameters.size()), boundary ? AAD::Adjoint(initial) : 0.0};
        for (size_t i = 0; i < parameters.size(); ++i)
            result.gradient_[i] = AAD::Adjoint(parameters[i]);
        recording.Close();
        return result;
    }

    Vector_<AAD::SampleDef_> PaymentDefinitions(const Vector_<>& timeline) {
        Vector_<AAD::SampleDef_> result(timeline.size());
        for (size_t i = 0; i < timeline.size(); ++i) {
            result[i].indexNames_ = {"EQ[DAL196_TEST]"};
            result[i].discountMats_ = {timeline[i] + 0.7};
        }
        return result;
    }
} // namespace

TEST(BlackScholesStepTest, TestIndependentIrregularSampleOracle) {
    const Vector_<Vector_<>> timelines{{0.0, 0.125, 0.75, 1.8}, {0.125, 0.75, 1.8}};
    const Vector_<> parameters{100.0, 0.2, 0.05, 0.01};
    const Vector_<> gaussian{0.3, -0.6, 0.9};
    for (const auto& timeline : timelines) {
        Vector_<AAD::SampleDef_> definitions(timeline.size());
        for (size_t i = 0; i < timeline.size(); ++i) {
            definitions[i].numeraire_ = i != 1;
            definitions[i].indexNames_ = {"EQ[DAL196_TEST]"};
            definitions[i].discountMats_ = {timeline[i] + 0.2, timeline[i] + 0.7};
        }
        const AAD::BlackScholesStepPlan_ plan(timeline, definitions);
        ASSERT_EQ(plan.Samples(), timeline.size());
        ASSERT_EQ(plan.SimDim(), gaussian.size());
        double logSpot = plan.InitialLogSpot(parameters);
        double brownian = 0.0;
        size_t draw = 0;
        for (size_t i = 0; i < timeline.size(); ++i) {
            const double previousTime = i == 0 ? 0.0 : timeline[i - 1];
            if (timeline[i] > 0.0)
                brownian += std::sqrt(timeline[i] - previousTime) * gaussian[draw++];
            const auto step = plan.Advance(i, logSpot, parameters, gaussian);
            logSpot = step.logSpot_;
            const double expectedSpot = parameters[0] * std::exp((parameters[2] - parameters[3] - 0.5 * parameters[1] * parameters[1]) * timeline[i] +
                                                                 parameters[1] * brownian);
            ASSERT_NEAR(step.sample_.spot_, expectedSpot, 1e-10);
            ASSERT_EQ(step.sample_.observations_.size(), 1);
            ASSERT_NEAR(step.sample_.observations_[0], expectedSpot, 1e-10);
            ASSERT_NEAR(step.sample_.numeraire_, definitions[i].numeraire_ ? std::exp(parameters[2] * timeline[i]) : 1.0, 1e-12);
            ASSERT_EQ(step.sample_.discounts_.size(), 2);
            ASSERT_NEAR(step.sample_.discounts_[0], std::exp(-parameters[2] * 0.2), 1e-12);
            ASSERT_NEAR(step.sample_.discounts_[1], std::exp(-parameters[2] * 0.7), 1e-12);
        }
        ASSERT_EQ(draw, gaussian.size());
    }
}

TEST(BlackScholesStepTest, TestNativeAnalyticAndFullGraphRisksAtFreshPoints) {
    const Vector_<> timeline{0.0, 0.125, 0.75, 1.8};
    const auto definitions = PaymentDefinitions(timeline);
    const Vector_<> gaussian{0.3, -0.6, 0.9};
    const double brownian = std::sqrt(0.125) * gaussian[0] + std::sqrt(0.625) * gaussian[1] + std::sqrt(1.05) * gaussian[2];
    const Vector_<Vector_<>> points{{120.0, 0.24, 0.03, 0.01}, {83.0, 0.0, -0.02, -0.01}};
    for (const auto& parameters : points) {
        const auto stepped = StepPaymentRisk(timeline, definitions, parameters, gaussian, false);
        const auto full = StepPaymentRisk(timeline, definitions, parameters, gaussian, true);
        const double expected = parameters[0] * std::exp(-parameters[3] * timeline.back() - 0.5 * parameters[1] * parameters[1] * timeline.back() +
                                                         parameters[1] * brownian - parameters[2] * 0.7);
        const Vector_<> gradient{expected / parameters[0], expected * (brownian - parameters[1] * timeline.back()), -0.7 * expected,
                                 -timeline.back() * expected};
        ASSERT_NEAR(stepped.value_, expected, 1e-10);
        ASSERT_NEAR(full.value_, expected, 1e-10);
        for (size_t i = 0; i < gradient.size(); ++i) {
            ASSERT_NEAR(stepped.gradient_[i], gradient[i], 1e-10);
            ASSERT_NEAR(full.gradient_[i], gradient[i], 1e-10);
        }
    }
}

TEST(BlackScholesStepTest, TestOwnedPlanAndRestoredIndependentBoundary) {
    const Vector_<> originalTimeline{0.0, 0.125, 0.75, 1.8};
    const auto originalDefinitions = PaymentDefinitions(originalTimeline);
    auto timeline = originalTimeline;
    auto definitions = originalDefinitions;
    const AAD::BlackScholesStepPlan_ plan(timeline, definitions);
    timeline[2] = 9.0;
    definitions[3].discountMats_[0] = 99.0;
    const Vector_<> parameters{100.0, 0.2, 0.05, 0.01};
    const Vector_<> gaussian{0.3, -0.6, 0.9};
    double logSpot = plan.InitialLogSpot(parameters);
    for (size_t i = 0; i < 2; ++i)
        logSpot = plan.Advance(i, logSpot, parameters, gaussian).logSpot_;
    const double remainingTime = originalTimeline.back() - originalTimeline[1];
    const double remainingBrownian = std::sqrt(0.625) * gaussian[1] + std::sqrt(1.05) * gaussian[2];
    const auto resumed = StepPaymentRisk(originalTimeline, originalDefinitions, parameters, gaussian, false, std::make_pair(size_t(2), logSpot));
    const double expected = std::exp(logSpot + (parameters[2] - parameters[3] - 0.5 * parameters[1] * parameters[1]) * remainingTime +
                                     parameters[1] * remainingBrownian - parameters[2] * (originalTimeline.back() + 0.7));
    const Vector_<> gradient{0.0, expected * (remainingBrownian - parameters[1] * remainingTime), -expected * (originalTimeline[1] + 0.7),
                             -expected * remainingTime};
    ASSERT_NEAR(resumed.value_, expected, 1e-10);
    ASSERT_NEAR(resumed.stateRisk_, expected, 1e-10);
    for (size_t i = 0; i < gradient.size(); ++i)
        ASSERT_NEAR(resumed.gradient_[i], gradient[i], 1e-10);
    for (size_t i = 2; i < originalTimeline.size(); ++i) {
        const auto step = plan.Advance(i, logSpot, parameters, gaussian);
        logSpot = step.logSpot_;
        if (i + 1 == originalTimeline.size()) {
            ASSERT_NEAR(step.sample_.spot_ * step.sample_.discounts_[0] / step.sample_.numeraire_, expected, 1e-10);
        }
    }
}

TEST(BlackScholesStepTest, TestTimeZeroRetainsExactSpotAndConsumesNoDraw) {
    const AAD::BlackScholesStepPlan_ plan({0.0}, PaymentDefinitions({0.0}));
    ASSERT_EQ(plan.SimDim(), 0);
    for (double spot : {1e-200, 1e200}) {
        const Vector_<> parameters{spot, 0.0, 0.0, 0.0};
        const double logSpot = plan.InitialLogSpot(parameters);
        const auto step = plan.Advance(0, logSpot, parameters, {});
        ASSERT_DOUBLE_EQ(step.sample_.spot_, spot);
        ASSERT_DOUBLE_EQ(step.sample_.observations_[0], spot);
        ASSERT_DOUBLE_EQ(step.logSpot_, logSpot);
    }
}

TEST(BlackScholesStepTest, TestInvalidRequestAdmissionAndRecovery) {
    const Vector_<> timeline{0.0, 0.25};
    const auto definitions = PaymentDefinitions(timeline);
    const AAD::BlackScholesStepPlan_ plan(timeline, definitions);
    const Vector_<> parameters{100.0, 0.2, 0.05, 0.01};
    const double logSpot = plan.InitialLogSpot(parameters);
    ASSERT_THROW((void)plan.InitialLogSpot(Vector_<>{}), Dal::Exception_);
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    const Vector_<Vector_<>> invalid{{0.0, 0.2, 0.05, 0.01},   {100.0, -0.2, 0.05, 0.01}, {nan, 0.2, 0.05, 0.01},
                                     {100.0, inf, 0.05, 0.01}, {100.0, 0.2, nan, 0.01},   {100.0, 0.2, 0.05, inf}};
    for (const auto& point : invalid) {
        ASSERT_THROW((void)plan.InitialLogSpot(point), Dal::Exception_);
        ASSERT_THROW((void)plan.Advance(1, logSpot, point, {0.0}), Dal::Exception_);
    }
    ASSERT_THROW((void)plan.Advance(2, logSpot, parameters, {0.0}), Dal::Exception_);
    ASSERT_THROW((void)plan.Advance(1, logSpot, parameters, {}), Dal::Exception_);
    ASSERT_THROW((void)plan.Advance(1, inf, parameters, {0.0}), Dal::Exception_);
    ASSERT_THROW((void)plan.Advance(1, logSpot, parameters, {nan}), Dal::Exception_);
    ASSERT_THROW((void)plan.Advance(1, logSpot, parameters, {inf}), Dal::Exception_);
    ASSERT_THROW(AAD::BlackScholesStepPlan_({}, {}), Dal::Exception_);
    ASSERT_THROW(AAD::BlackScholesStepPlan_(timeline, {}), Dal::Exception_);
    ASSERT_THROW(AAD::BlackScholesStepPlan_({0.0, 0.0}, definitions), Dal::Exception_);
    ASSERT_THROW(AAD::BlackScholesStepPlan_({0.0, -0.25}, definitions), Dal::Exception_);
    ASSERT_THROW(AAD::BlackScholesStepPlan_({0.0, nan}, definitions), Dal::Exception_);
    auto invalidDefinitions = definitions;
    invalidDefinitions[1].discountMats_ = {0.1};
    ASSERT_THROW(AAD::BlackScholesStepPlan_(timeline, invalidDefinitions), Dal::Exception_);
    invalidDefinitions[1].discountMats_ = {inf};
    ASSERT_THROW(AAD::BlackScholesStepPlan_(timeline, invalidDefinitions), Dal::Exception_);
    const auto healthy = plan.Advance(1, logSpot, parameters, {0.0});
    ASSERT_NEAR(healthy.sample_.spot_, 100.0 * std::exp(0.02 * 0.25), 1e-10);
}

TEST(BlackScholesStepTest, TestNonfiniteBoundaryRejectedBeforeReturn) {
    const Vector_<> timeline{0.0, 2.0, 4.0};
    const AAD::BlackScholesStepPlan_ plan(timeline, PaymentDefinitions(timeline));
    const Vector_<> parameters{1.0, 1e154, 0.0, 0.0};
    const Vector_<> gaussian{0.0, 0.0};
    const auto first = plan.Advance(1, plan.InitialLogSpot(parameters), parameters, gaussian);
    ASSERT_TRUE(std::isfinite(first.logSpot_));
    ASSERT_DOUBLE_EQ(first.sample_.spot_, 0.0);
    ASSERT_THROW((void)plan.Advance(2, first.logSpot_, parameters, gaussian), Dal::Exception_);
    const auto healthy = plan.Advance(2, 0.0, parameters, gaussian);
    ASSERT_TRUE(std::isfinite(healthy.logSpot_));
    ASSERT_DOUBLE_EQ(healthy.sample_.spot_, 0.0);
}

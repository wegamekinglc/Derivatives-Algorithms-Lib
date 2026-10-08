//
// Created by Codex on 2026/10/8.
//

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <limits>

#include <dal/math/aad/native.hpp>
#include <dal/math/aad/sampledthetastep.hpp>
#include <dal/platform/platform.hpp>

#include "sampledthetastepfixture.hpp"

using namespace Dal;
using namespace Dal::AAD;

namespace {
    PDE::SampledThetaStepInputs_ AnalyticInputs() {
        PDE::SampledThetaStepInputs_ inputs;
        inputs.x_ = {0.0, 1.0, 2.0};
        inputs.rates_ = {0.1};
        inputs.drifts_ = {0.2};
        inputs.variances_ = {0.4};
        inputs.dt_ = 0.2;
        inputs.theta_ = 0.5;
        inputs.oldValues_ = Matrix_<>(3, 2);
        const double values[] = {1.0, -1.0, 2.0, 0.5, 3.0, 2.0};
        std::copy(std::begin(values), std::end(values), inputs.oldValues_.Data());
        inputs.externalBoundaries_ = {true, false};
        inputs.externalValues_ = Matrix_<>(2, 2);
        const double external[] = {4.0, 3.0, 5.0, -2.0};
        std::copy(std::begin(external), std::end(external), inputs.externalValues_.Data());
        return inputs;
    }

    template <class T_> T_ ActiveField(const T_& all, int group, int activity) { return activity == 7 || activity == group ? all : T_{}; }

    SampledThetaStepBindings_ SelectActivity(const SampledThetaStepBindings_& all, int activity) {
        return {ActiveField(all.rates_, 0, activity),     ActiveField(all.drifts_, 1, activity),         ActiveField(all.variances_, 2, activity),
                ActiveField(all.oldValues_, 3, activity), ActiveField(all.externalValues_, 4, activity), ActiveField(all.dt_, 5, activity),
                ActiveField(all.theta_, 6, activity)};
    }

    void RemoveOverriddenPassiveFields(PDE::SampledThetaStepInputs_* inputs, const SampledThetaStepBindings_& active) {
        const double invalid = std::numeric_limits<double>::quiet_NaN();
        if (!active.rates_.empty())
            inputs->rates_.clear();
        if (!active.drifts_.empty())
            inputs->drifts_.clear();
        if (!active.variances_.empty())
            inputs->variances_.clear();
        if (!active.oldValues_.Empty())
            inputs->oldValues_.Clear();
        if (!active.externalValues_.Empty())
            inputs->externalValues_.Clear();
        if (active.dt_)
            inputs->dt_ = invalid;
        if (active.theta_)
            inputs->theta_ = invalid;
    }

    Number_ AnalyticObjective(const Matrix_<Number_>& solution, const SampledThetaStepBindings_& inputs) {
        const double weights[] = {0.3, -0.4, 2.0, -0.75, -0.7, 0.2};
        Number_ objective = inputs.rates_[0] + 2.0 * *inputs.dt_ - 3.0 * *inputs.theta_;
        for (int row = 0; row < 3; ++row)
            for (int layer = 0; layer < 2; ++layer)
                objective += weights[2 * row + layer] * solution(row, layer);
        return objective;
    }

    double ChannelScale(size_t channel) {
        const double scales[] = {1.0, -2.0, 0.0, 3.0};
        return scales[channel % 4];
    }

    void VerifyAnalyticRisks(const SampledThetaStepBindings_& all, int activity, size_t channels) {
        const Number_* inputs[] = {&all.rates_[0],
                                   &all.drifts_[0],
                                   &all.variances_[0],
                                   &all.oldValues_(0, 0),
                                   &all.oldValues_(0, 1),
                                   &all.oldValues_(1, 0),
                                   &all.oldValues_(1, 1),
                                   &all.oldValues_(2, 0),
                                   &all.oldValues_(2, 1),
                                   &all.externalValues_(0, 0),
                                   &all.externalValues_(0, 1),
                                   &all.externalValues_(1, 0),
                                   &all.externalValues_(1, 1),
                                   &*all.dt_,
                                   &*all.theta_};
        const int groups[] = {0, 1, 2, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 5, 6};
        const double fullRisks[] = {76.0 / 245.0, 1.0 / 42.0,   211.0 / 1470.0, 2.0 / 105.0, -1.0 / 140.0, 38.0 / 21.0,   -19.0 / 28.0,  -41.0 / 70.0,
                                    11.0 / 70.0,  67.0 / 210.0, -57.0 / 140.0,  0.0,         0.0,          289.0 / 147.0, -865.0 / 294.0};
        const double directRisks[] = {1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 2.0, -3.0};
        for (size_t input = 0; input < std::size(inputs); ++input) {
            const double expected = activity == 7 || activity == groups[input] ? fullRisks[input] : directRisks[input];
            for (size_t channel = 0; channel < channels; ++channel) {
                const double risk = NativeOperations_::ReadAdjoint(*inputs[input], channel);
                ASSERT_NEAR(risk, expected * ChannelScale(channel), 1e-13);
                if (expected == 0.0 || ChannelScale(channel) == 0.0) {
                    ASSERT_EQ(risk, 0.0);
                }
            }
        }
    }

    void CheckAnalyticActivity(int activity, size_t width) {
        SCOPED_TRACE(activity);
        SCOPED_TRACE(width);
        Clear(*Tape());
        const size_t channels = std::max(size_t(1), width);
        auto mode = SetNumResultsForAAD(width != 0, channels);
        RecordingScope_ scope;
        auto numeric = AnalyticInputs();
        const auto all = DalTest::NativePDE::RegisterBindings(&scope, numeric);
        const auto selected = SelectActivity(all, activity);
        RemoveOverriddenPassiveFields(&numeric, selected);
        scope.StartRecording();
        const auto before = Tape()->nodes_.OccupiedSlots();
        const auto result = SampledThetaStepWithAccuracy(&scope, numeric, selected, LinearSolveAccuracyPolicy_{1e-14, 1e-14});
        ASSERT_EQ(Tape()->nodes_.OccupiedSlots() - before, 6);
        Number_ objective = AnalyticObjective(result.solution_, all);
        scope.FinishRecording();
        scope.ClearAdjoints();
        for (size_t channel = 0; channel < channels; ++channel)
            NativeOperations_::SetSeed(objective, ChannelScale(channel), channel);
        const auto reports = ReverseWithSolveAccuracy(&scope);
        ASSERT_NEAR(Value(objective), 257.0 / 280.0, 1e-13);
        ASSERT_NEAR(Value(result.solution_(1, 0)), 71.0 / 35.0, 1e-13);
        ASSERT_NEAR(Value(result.solution_(1, 1)), 41.0 / 70.0, 1e-13);
        ASSERT_NO_FATAL_FAILURE(VerifyAnalyticRisks(all, activity, channels));
        const auto& report = reports.Report(result.event_);
        ASSERT_EQ(report.transposeBackwardErrors_.Rows(), 2);
        ASSERT_EQ(report.transposeBackwardErrors_.Cols(), static_cast<int>(channels));
        ASSERT_EQ(reports.IsMulti(), width != 0);
        for (double error : report.transposeBackwardErrors_)
            ASSERT_LE(error, 1e-14);
        scope.Close();
        Clear(*Tape());
    }
} // namespace

TEST(AADSampledThetaStepTest, TestIndependentFieldActivitiesAndLayerChannelAxes) {
    for (size_t width : {0U, 1U, 4U, 8U})
        for (int activity = 0; activity < 8; ++activity)
            ASSERT_NO_FATAL_FAILURE(CheckAnalyticActivity(activity, width));
}

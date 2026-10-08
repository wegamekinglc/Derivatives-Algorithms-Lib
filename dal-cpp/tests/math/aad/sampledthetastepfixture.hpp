//
// Created by Codex on 2026/10/8.
//

#pragma once

#include <dal/math/aad/native.hpp>
#include <dal/math/aad/sampledthetastep.hpp>

namespace DalTest::NativePDE {
    inline Dal::Vector_<Dal::AAD::Number_> RegisterField(Dal::AAD::RecordingScope_* scope, const Dal::Vector_<>& values) {
        Dal::Vector_<Dal::AAD::Number_> result(values.size());
        for (size_t index = 0; index < values.size(); ++index)
            scope->RegisterInput(result[index], values[index]);
        return result;
    }

    inline Dal::Matrix_<Dal::AAD::Number_> RegisterField(Dal::AAD::RecordingScope_* scope, const Dal::Matrix_<>& values) {
        Dal::Matrix_<Dal::AAD::Number_> result(values.Rows(), values.Cols());
        for (int row = 0; row < values.Rows(); ++row)
            for (int column = 0; column < values.Cols(); ++column)
                scope->RegisterInput(result(row, column), values(row, column));
        return result;
    }

    inline Dal::AAD::Number_ RegisterScalar(Dal::AAD::RecordingScope_* scope, double value) {
        Dal::AAD::Number_ result;
        scope->RegisterInput(result, value);
        return result;
    }

    inline Dal::AAD::SampledThetaStepBindings_ RegisterBindings(Dal::AAD::RecordingScope_* scope, const Dal::PDE::SampledThetaStepInputs_& values) {
        return {RegisterField(scope, values.rates_),     RegisterField(scope, values.drifts_),         RegisterField(scope, values.variances_),
                RegisterField(scope, values.oldValues_), RegisterField(scope, values.externalValues_), RegisterScalar(scope, values.dt_),
                RegisterScalar(scope, values.theta_)};
    }

    inline Dal::PDE::SampledThetaStepInputs_ IdentityInputs(int n = 3, int layers = 1, double theta = 0.0) {
        Dal::PDE::SampledThetaStepInputs_ inputs;
        inputs.x_ = Dal::Vector_<>(n);
        for (int row = 0; row < n; ++row)
            inputs.x_[row] = row;
        inputs.rates_ = inputs.drifts_ = inputs.variances_ = Dal::Vector_<>(n - 2, 0.0);
        inputs.dt_ = 0.25;
        inputs.theta_ = theta;
        inputs.oldValues_ = Dal::Matrix_<>(n, layers, 1.0);
        return inputs;
    }

    inline double RecoveryRisk() {
        using namespace Dal::AAD;
        Clear(*Tape());
        auto mode = SetNumResultsForAAD(false, 1);
        RecordingScope_ scope;
        Number_ input;
        scope.RegisterInput(input, 0.0);
        scope.StartRecording();
        SampledThetaStepBindings_ active;
        active.rates_ = {input};
        auto result = SampledThetaStepWithAccuracy(&scope, IdentityInputs(), active, Dal::LinearSolveAccuracyPolicy_{0.0, 0.0});
        scope.FinishRecording();
        scope.ClearAdjoints();
        NativeOperations_::SetSeed(result.solution_(1, 0), 1.0);
        scope.Reverse();
        const double risk = NativeOperations_::ReadAdjoint(input);
        scope.Close();
        Clear(*Tape());
        return risk;
    }
} // namespace DalTest::NativePDE

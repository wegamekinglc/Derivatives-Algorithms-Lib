//
// Created by Codex on 2026/10/8.
//

#pragma once

#include <optional>

#include <dal/math/aad/linearsolveaccuracy.hpp>
#include <dal/math/pde/sampledthetastep.hpp>

namespace Dal::AAD {
    struct SampledThetaStepBindings_ {
        Vector_<Number_> rates_, drifts_, variances_;
        Matrix_<Number_> oldValues_, externalValues_;
        std::optional<Number_> dt_, theta_;
    };

    struct SampledThetaStepDiagnostics_ {
        Vector_<> forwardBackwardErrors_;
        LinearSolveAccuracyPolicy_ policy_;
    };

    struct CheckedSampledThetaStepResult_ {
        Matrix_<Number_> solution_;
        SampledThetaStepDiagnostics_ diagnostics_;
        SolveAccuracyEvent_ event_;
    };

    [[nodiscard]] CheckedSampledThetaStepResult_ SampledThetaStepWithAccuracy(RecordingScope_* recording,
                                                                              const PDE::SampledThetaStepInputs_& numericInputs,
                                                                              const SampledThetaStepBindings_& activeBindings,
                                                                              const LinearSolveAccuracyPolicy_& policy,
                                                                              double relativePivotTolerance = 64.0 *
                                                                                                              std::numeric_limits<double>::epsilon());
} // namespace Dal::AAD

//
// Created by Codex on 2026/10/8.
//

#pragma once

#include <limits>

#include <dal/math/aad/linearsolveaccuracy.hpp>
#include <dal/math/optimization/implicitroot.hpp>

namespace Dal::AAD {
    struct ImplicitRootDiagnostics_ {
        Vector_<> residuals_;
        ImplicitRootAccuracyPolicy_ policy_;
        double reciprocalConditionInfinity_ = 0.0;
    };

    struct CheckedImplicitRootResult_ {
        Vector_<Number_> parameters_;
        ImplicitRootDiagnostics_ diagnostics_;
        SolveAccuracyEvent_ event_;
    };

    [[nodiscard]] CheckedImplicitRootResult_ ImplicitRootWithAccuracy(RecordingScope_* recording,
                                                                      const ImplicitRootEquation_& equation,
                                                                      const Vector_<>& candidate,
                                                                      const Vector_<Number_>& inputs,
                                                                      const ImplicitRootAccuracyPolicy_& policy,
                                                                      double relativePivotTolerance = 64.0 * std::numeric_limits<double>::epsilon());
} // namespace Dal::AAD

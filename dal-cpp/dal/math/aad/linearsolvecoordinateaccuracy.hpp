//
// Created by Codex on 2026/10/8.
//

#pragma once

#include <dal/math/aad/linearsolveaccuracy.hpp>
#include <dal/math/matrix/linearsolvecoordinates.hpp>

namespace Dal::AAD {
    [[nodiscard]] CheckedLinearSolveResult_ LinearSolveWithAccuracy(RecordingScope_* recording,
                                                                    const LinearSolveCoordinates_& coordinates,
                                                                    const Vector_<Number_>& parameters,
                                                                    const Matrix_<Number_>& rhs,
                                                                    const LinearSolveAccuracyPolicy_& policy,
                                                                    double relativePivotTolerance = 64.0 * std::numeric_limits<double>::epsilon());
    [[nodiscard]] CheckedLinearSolveResult_ LinearSolveWithAccuracy(RecordingScope_* recording,
                                                                    const LinearSolveCoordinates_& coordinates,
                                                                    const Vector_<>& parameters,
                                                                    const Matrix_<Number_>& rhs,
                                                                    const LinearSolveAccuracyPolicy_& policy,
                                                                    double relativePivotTolerance = 64.0 * std::numeric_limits<double>::epsilon());
    [[nodiscard]] CheckedLinearSolveResult_ LinearSolveWithAccuracy(RecordingScope_* recording,
                                                                    const LinearSolveCoordinates_& coordinates,
                                                                    const Vector_<Number_>& parameters,
                                                                    const Matrix_<>& rhs,
                                                                    const LinearSolveAccuracyPolicy_& policy,
                                                                    double relativePivotTolerance = 64.0 * std::numeric_limits<double>::epsilon());
} // namespace Dal::AAD

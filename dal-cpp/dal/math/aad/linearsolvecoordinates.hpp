//
// Created by Codex on 2026/10/8.
//

#pragma once

#include <limits>

#include <dal/math/aad/recording.hpp>
#include <dal/math/matrix/linearsolvecoordinates.hpp>

namespace Dal::AAD {
    [[nodiscard]] Matrix_<Number_> LinearSolve(RecordingScope_* recording,
                                               const LinearSolveCoordinates_& coordinates,
                                               const Vector_<Number_>& parameters,
                                               const Matrix_<Number_>& rhs,
                                               double relativePivotTolerance = 64.0 * std::numeric_limits<double>::epsilon());
    [[nodiscard]] Matrix_<Number_> LinearSolve(RecordingScope_* recording,
                                               const LinearSolveCoordinates_& coordinates,
                                               const Vector_<>& parameters,
                                               const Matrix_<Number_>& rhs,
                                               double relativePivotTolerance = 64.0 * std::numeric_limits<double>::epsilon());
    [[nodiscard]] Matrix_<Number_> LinearSolve(RecordingScope_* recording,
                                               const LinearSolveCoordinates_& coordinates,
                                               const Vector_<Number_>& parameters,
                                               const Matrix_<>& rhs,
                                               double relativePivotTolerance = 64.0 * std::numeric_limits<double>::epsilon());
} // namespace Dal::AAD

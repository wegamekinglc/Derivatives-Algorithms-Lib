//
// Created by Codex on 2026/10/8.
//

#pragma once

#include <dal/math/matrix/linearsolveaccuracy.hpp>
#include <dal/math/matrix/linearsolvecoordinates.hpp>

namespace Dal {
    struct CheckedCoordinateLinearSolveAdjoints_ {
        CoordinateLinearSolveAdjoints_ adjoints_;
        Vector_<> transposeBackwardErrors_;
    };

    class CheckedCoordinateLinearSolve_ {
        LinearSolveCoordinates_ coordinates_;
        CheckedLinearSolve_ solve_;

    public:
        CheckedCoordinateLinearSolve_(const LinearSolveCoordinates_& coordinates,
                                      const Vector_<>& parameters,
                                      const Matrix_<>& rhs,
                                      const LinearSolveAccuracyPolicy_& policy,
                                      double relativePivotTolerance = 64.0 * std::numeric_limits<double>::epsilon());
        [[nodiscard]] const LinearSolveCoordinates_& Coordinates() const { return coordinates_; }
        [[nodiscard]] const Matrix_<>& Solution() const { return solve_.Solution(); }
        [[nodiscard]] const LinearSolveDiagnostics_& Diagnostics() const { return solve_.Diagnostics(); }
        [[nodiscard]] const LinearSolveAccuracyPolicy_& Policy() const { return solve_.Policy(); }
        [[nodiscard]] CheckedCoordinateLinearSolveAdjoints_ Reverse(const Matrix_<>& solutionAdjoints) const;
        [[nodiscard]] CheckedCoordinateLinearSolveAdjoints_ ReverseRhs(const Matrix_<>& solutionAdjoints) const;
    };
} // namespace Dal

//
// Created by Codex on 2026/10/7.
//

#pragma once

#include <dal/math/matrix/linearsolvepullback.hpp>

namespace Dal {
    struct LinearSolveDiagnostics_ {
        double reciprocalConditionInfinity_ = 0.0;
        Vector_<> componentwiseBackwardErrors_;
    };

    [[nodiscard]] Vector_<> LinearSolveBackwardErrors(const SquareMatrix_<>& matrix, const Matrix_<>& rhs, const Matrix_<>& solution);

    class DiagnosedLinearSolve_ {
        LinearSolvePullback_ solve_;
        LinearSolveDiagnostics_ diagnostics_;

    public:
        DiagnosedLinearSolve_(const SquareMatrix_<>& matrix,
                              const Matrix_<>& rhs,
                              double relativePivotTolerance = 64.0 * std::numeric_limits<double>::epsilon());
        [[nodiscard]] const LinearSolvePullback_& Solve() const { return solve_; }
        [[nodiscard]] const LinearSolveDiagnostics_& Diagnostics() const { return diagnostics_; }
    };
} // namespace Dal

//
// Created by Codex on 2026/10/8.
//

#pragma once

#include <limits>

#include <dal/math/matrix/linearsolvediagnostics.hpp>

namespace Dal {
    struct LinearSolveAccuracyPolicy_ {
        double forwardBackwardErrorLimit_ = 0.0;
        double transposeBackwardErrorLimit_ = 0.0;
    };

    struct CheckedLinearSolveAdjoints_ {
        LinearSolveAdjoints_ adjoints_;
        Vector_<> transposeBackwardErrors_;
    };

    class CheckedLinearSolve_ {
        LinearSolveAccuracyPolicy_ policy_;
        SquareMatrix_<> transpose_;
        DiagnosedLinearSolve_ solve_;

        [[nodiscard]] Vector_<> TransposeErrors(const Matrix_<>& seeds, const Matrix_<>& adjoints) const;

    public:
        CheckedLinearSolve_(const SquareMatrix_<>& matrix,
                            const Matrix_<>& rhs,
                            const LinearSolveAccuracyPolicy_& policy,
                            double relativePivotTolerance = 64.0 * std::numeric_limits<double>::epsilon());
        [[nodiscard]] const Matrix_<>& Solution() const { return solve_.Solve().Solution(); }
        [[nodiscard]] const LinearSolveDiagnostics_& Diagnostics() const { return solve_.Diagnostics(); }
        [[nodiscard]] const LinearSolveAccuracyPolicy_& Policy() const { return policy_; }
        [[nodiscard]] CheckedLinearSolveAdjoints_ Reverse(const Matrix_<>& solutionAdjoints) const;
        [[nodiscard]] CheckedLinearSolveAdjoints_ ReverseRhs(const Matrix_<>& solutionAdjoints) const;
    };
} // namespace Dal

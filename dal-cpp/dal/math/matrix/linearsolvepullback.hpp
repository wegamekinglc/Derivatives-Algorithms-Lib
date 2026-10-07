//
// Created by Codex on 2026/10/7.
//

#pragma once

#include <limits>

#include <dal/math/matrix/squarematrix.hpp>

namespace Dal {
    struct LinearSolveAdjoints_ {
        SquareMatrix_<> matrix_;
        Matrix_<> rhs_;
    };

    class LinearSolvePullback_ {
        SquareMatrix_<> factors_;
        Vector_<int> swaps_;
        double scale_ = 0.0;
        double scaledMinimumPivot_ = std::numeric_limits<double>::max();
        Matrix_<> solution_;

        void TriangularSolve(bool transpose, bool unitDiagonal, Matrix_<>* result) const;
        [[nodiscard]] Matrix_<> Solve(const Matrix_<>& rhs, bool transpose) const;

    public:
        LinearSolvePullback_(const SquareMatrix_<>& matrix,
                             const Matrix_<>& rhs,
                             double relativePivotTolerance = 64.0 * std::numeric_limits<double>::epsilon());
        [[nodiscard]] const Matrix_<>& Solution() const { return solution_; }
        [[nodiscard]] double ScaledMinimumPivot() const { return scaledMinimumPivot_; }
        [[nodiscard]] LinearSolveAdjoints_ Reverse(const Matrix_<>& solutionAdjoints) const;
    };
} // namespace Dal

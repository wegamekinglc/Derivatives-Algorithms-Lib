//
// Created by Codex on 2026/10/7.
//

#include <dal/platform/platform.hpp>

#include <algorithm>
#include <cmath>

#include <dal/math/matrix/linearsolvepullback.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal {
    namespace {
        void ValidateFinite(const Matrix_<>& matrix, const char* message) {
            for (const double value : matrix)
                REQUIRE(std::isfinite(value), message);
        }

        int FactorColumn(SquareMatrix_<>* factors, int pivot, double tolerance) {
            const int n = factors->Rows();
            int best = pivot;
            for (int row = pivot + 1; row < n; ++row)
                if (std::abs((*factors)(row, pivot)) > std::abs((*factors)(best, pivot)))
                    best = row;
            REQUIRE(std::abs((*factors)(best, pivot)) > tolerance, "Linear solve normalized pivot is at or below the relative pivot tolerance");
            if (best != pivot)
                for (int col = 0; col < n; ++col)
                    std::swap((*factors)(best, col), (*factors)(pivot, col));
            for (int row = pivot + 1; row < n; ++row) {
                (*factors)(row, pivot) /= (*factors)(pivot, pivot);
                for (int col = pivot + 1; col < n; ++col) {
                    (*factors)(row, col) -= (*factors)(row, pivot) * (*factors)(pivot, col);
                    REQUIRE(std::isfinite((*factors)(row, col)), "Linear solve factorization overflow");
                }
            }
            return best;
        }

        void ApplySwaps(const Vector_<int>& swaps, bool reverse, int rhsColumn, Matrix_<>* result) {
            for (int step = 0; step < swaps.size(); ++step) {
                const int row = reverse ? swaps.size() - 1 - step : step;
                if (row != swaps[row])
                    std::swap((*result)(row, rhsColumn), (*result)(swaps[row], rhsColumn));
            }
        }

        bool NeedsLateScaling(const Matrix_<>::ConstCol_& rhs, double scale) {
            const double threshold = scale * std::numeric_limits<double>::min();
            return std::any_of(rhs.begin(), rhs.end(), [threshold](double value) { return value != 0.0 && std::abs(value) < threshold; });
        }

        void ScaleColumn(int column, double scale, Matrix_<>* result, const char* message) {
            for (int row = 0; row < result->Rows(); ++row) {
                const double value = (*result)(row, column);
                const double scaled = value / scale;
                REQUIRE(std::isfinite(scaled), message);
                REQUIRE(scaled != 0.0 || value == 0.0, message);
                (*result)(row, column) = scaled;
            }
        }
    } // namespace

    LinearSolvePullback_::LinearSolvePullback_(const SquareMatrix_<>& matrix, const Matrix_<>& rhs, double relativePivotTolerance)
        : factors_(matrix), swaps_(matrix.Rows()) {
        const int n = matrix.Rows();
        REQUIRE(n > 0, "Linear solve matrix must have positive size");
        REQUIRE(rhs.Rows() == n && rhs.Cols() > 0, "Linear solve RHS must have matrix-size rows and at least one column");
        REQUIRE(std::isfinite(relativePivotTolerance) && relativePivotTolerance > 0.0 && relativePivotTolerance < 1.0,
                "Linear solve relative pivot tolerance must be finite and strictly between zero and one");
        ValidateFinite(matrix, "Linear solve matrix entries must be finite");
        ValidateFinite(rhs, "Linear solve RHS entries must be finite");
        for (const double value : static_cast<const Matrix_<>&>(matrix))
            scale_ = std::max(scale_, std::abs(value));
        REQUIRE(scale_ > 0.0, "Linear solve matrix is singular: all entries are zero");
        for (int row = 0; row < n; ++row)
            for (int col = 0; col < n; ++col)
                factors_(row, col) /= scale_;
        for (int pivot = 0; pivot < n; ++pivot) {
            swaps_[pivot] = FactorColumn(&factors_, pivot, relativePivotTolerance);
            scaledMinimumPivot_ = std::min(scaledMinimumPivot_, std::abs(factors_(pivot, pivot)));
        }
        solution_ = Solve(rhs, false);
    }

    void LinearSolvePullback_::TriangularSolve(bool transpose, bool unitDiagonal, int rhsColumn, Matrix_<>* result, const char* message) const {
        const int n = factors_.Rows();
        const bool ascending = unitDiagonal != transpose;
        for (int step = 0; step < n; ++step) {
            const int row = ascending ? step : n - 1 - step;
            double value = (*result)(row, rhsColumn);
            for (int colStep = 0; colStep < step; ++colStep) {
                const int col = ascending ? colStep : n - 1 - colStep;
                const double coefficient = transpose ? factors_(col, row) : factors_(row, col);
                value -= coefficient * (*result)(col, rhsColumn);
                REQUIRE(std::isfinite(value), message);
            }
            if (!unitDiagonal)
                value /= factors_(row, row);
            REQUIRE(std::isfinite(value), message);
            (*result)(row, rhsColumn) = value;
        }
    }

    Matrix_<> LinearSolvePullback_::Solve(const Matrix_<>& rhs, bool transpose) const {
        Matrix_<> result(rhs);
        const char* message = transpose ? "Linear solve transpose substitution/result overflow" : "Linear solve forward substitution/result overflow";
        const char* scaleMessage =
            transpose ? "Linear solve transpose scaling loses numerical range" : "Linear solve forward scaling loses numerical range";
        for (int column = 0; column < rhs.Cols(); ++column) {
            const bool lateScaling = NeedsLateScaling(rhs.Col(column), scale_);
            if (!lateScaling)
                ScaleColumn(column, scale_, &result, scaleMessage);
            if (!transpose)
                ApplySwaps(swaps_, false, column, &result);
            TriangularSolve(transpose, !transpose, column, &result, message);
            TriangularSolve(transpose, transpose, column, &result, message);
            if (transpose)
                ApplySwaps(swaps_, true, column, &result);
            if (lateScaling)
                ScaleColumn(column, scale_, &result, scaleMessage);
        }
        return result;
    }

    LinearSolveAdjoints_ LinearSolvePullback_::Reverse(const Matrix_<>& solutionAdjoints) const {
        REQUIRE(solutionAdjoints.Rows() == solution_.Rows() && solutionAdjoints.Cols() == solution_.Cols(),
                "Linear solve seed shape must match the solution shape");
        ValidateFinite(solutionAdjoints, "Linear solve seed entries must be finite");
        const int n = factors_.Rows();
        LinearSolveAdjoints_ result{SquareMatrix_<>(n), Solve(solutionAdjoints, true)};
        for (int row = 0; row < n; ++row)
            for (int col = 0; col < n; ++col) {
                double value = 0.0;
                for (int rhs = 0; rhs < solution_.Cols(); ++rhs) {
                    value -= result.rhs_(row, rhs) * solution_(col, rhs);
                    REQUIRE(std::isfinite(value), "Linear solve matrix adjoint accumulation overflow");
                }
                result.matrix_(row, col) = value;
            }
        return result;
    }
} // namespace Dal

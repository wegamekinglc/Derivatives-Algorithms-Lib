//
// Created by Codex on 2026/10/7.
//

#include <dal/platform/platform.hpp>

#include <algorithm>
#include <cmath>

#include <dal/math/matrix/linearsolvediagnostics.hpp>
#include <dal/math/matrix/physicalrowerrorinternal.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal {
    namespace {
        void ValidateFinite(const Matrix_<>& matrix, const char* message) {
            for (double value : matrix)
                REQUIRE(std::isfinite(value), message);
        }

        double RowBackwardError(const SquareMatrix_<>& matrix, const Matrix_<>& rhs, const Matrix_<>& solution, int row, int column) {
            return LinearSolveDetail::PhysicalRowBackwardError(rhs(row, column), matrix.Rows(), [&](int entry) {
                return LinearSolveDetail::ScaledProduct_(matrix(row, entry), solution(entry, column));
            });
        }

        double InfinityNorm(const Matrix_<>& matrix, double scale) {
            double norm = 0.0;
            for (int row = 0; row < matrix.Rows(); ++row) {
                double sum = 0.0;
                for (int column = 0; column < matrix.Cols(); ++column)
                    sum += std::abs(matrix(row, column) / scale);
                norm = std::max(norm, sum);
            }
            return norm;
        }
    } // namespace

    Vector_<> LinearSolveBackwardErrors(const SquareMatrix_<>& matrix, const Matrix_<>& rhs, const Matrix_<>& solution) {
        REQUIRE(matrix.Rows() > 0 && rhs.Rows() == matrix.Rows() && rhs.Cols() > 0,
                "Linear solve backward error requires a nonempty square matrix and compatible RHS");
        REQUIRE(solution.Rows() == rhs.Rows() && solution.Cols() == rhs.Cols(), "Linear solve backward error solution shape must match the RHS");
        ValidateFinite(matrix, "Linear solve backward error matrix entries must be finite");
        ValidateFinite(rhs, "Linear solve backward error RHS entries must be finite");
        ValidateFinite(solution, "Linear solve backward error solution entries must be finite");
        Vector_<> errors(rhs.Cols(), 0.0);
        for (int column = 0; column < rhs.Cols(); ++column)
            for (int row = 0; row < matrix.Rows(); ++row)
                errors[column] = std::max(errors[column], RowBackwardError(matrix, rhs, solution, row, column));
        return errors;
    }

    DiagnosedLinearSolve_::DiagnosedLinearSolve_(const SquareMatrix_<>& matrix, const Matrix_<>& rhs, double relativePivotTolerance)
        : solve_(matrix, rhs, relativePivotTolerance) {
        diagnostics_.componentwiseBackwardErrors_ = LinearSolveBackwardErrors(matrix, rhs, solve_.Solution());
        SquareMatrix_<> identity(matrix.Rows());
        for (int row = 0; row < matrix.Rows(); ++row)
            identity(row, row) = solve_.scale_;
        const auto inverse = solve_.Solve(identity, false);
        double inverseScale = 0.0;
        for (double value : inverse)
            inverseScale = std::max(inverseScale, std::abs(value));
        REQUIRE(inverseScale > 0.0, "Linear solve diagnostic inverse norm must be positive");
        const double matrixNorm = InfinityNorm(matrix, solve_.scale_);
        const double inverseNorm = InfinityNorm(inverse, inverseScale);
        diagnostics_.reciprocalConditionInfinity_ = std::min(1.0, ((1.0 / matrixNorm) / inverseNorm) / inverseScale);
    }
} // namespace Dal

#pragma once

#include <cmath>

#include <dal/math/matrix/squarematrix.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal::LinearSolveDetail {
    inline SquareMatrix_<> MatrixAdjoints(const Matrix_<>& solution, const Matrix_<>& rhsAdjoints) {
        SquareMatrix_<> result(solution.Rows());
        for (int row = 0; row < result.Rows(); ++row)
            for (int column = 0; column < result.Rows(); ++column) {
                double value = 0.;
                for (int rhs = 0; rhs < solution.Cols(); ++rhs) {
                    value -= rhsAdjoints(row, rhs) * solution(column, rhs);
                    REQUIRE(std::isfinite(value), "Linear solve matrix adjoint accumulation overflow");
                }
                result(row, column) = value;
            }
        return result;
    }
} // namespace Dal::LinearSolveDetail

//
// Created by Codex on 2026/10/8.
//

#pragma once

#include <cmath>

#include <dal/math/matrix/linearsolvecoordinates.hpp>
#include <dal/platform/platform.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal::Detail {
    FORCE_INLINE double LinearSolveCoordinateEntryContribution(const Matrix_<>& rhsAdjoints, const Matrix_<>& solution, int row, int column) {
        double value = 0.0;
        for (int rhs = 0; rhs < solution.Cols(); ++rhs) {
            value -= rhsAdjoints(row, rhs) * solution(column, rhs);
            REQUIRE(std::isfinite(value), "Linear solve coordinate adjoint accumulation overflow");
        }
        return value;
    }

    FORCE_INLINE Vector_<>
    LinearSolveCoordinateAdjoints(const LinearSolveCoordinates_& coordinates, const Matrix_<>& solution, const Matrix_<>& rhsAdjoints) {
        Vector_<> result(coordinates.Count());
        size_t index = 0;
        for (int row = 0; row < coordinates.Size(); ++row) {
            const int end = coordinates.RowEnd(row);
            for (int column = coordinates.RowBegin(row); column < end; ++column) {
                double value = LinearSolveCoordinateEntryContribution(rhsAdjoints, solution, row, column);
                if (coordinates.IsSymmetric() && row != column)
                    value += LinearSolveCoordinateEntryContribution(rhsAdjoints, solution, column, row);
                REQUIRE(std::isfinite(value), "Linear solve paired coordinate adjoint accumulation overflow");
                result[index++] = value;
            }
        }
        return result;
    }
} // namespace Dal::Detail

//
// Created by Codex on 2026/10/8.
//

#pragma once

#include <cstddef>
#include <utility>

#include <dal/math/matrix/linearsolvepullback.hpp>

namespace Dal {
    class LinearSolveCoordinates_ {
        int size_;
        int below_;
        int above_;
        bool symmetric_;
        size_t count_;

        LinearSolveCoordinates_(int size, int below, int above, bool symmetric);
        [[nodiscard]] size_t RowOffset(int row) const;

    public:
        [[nodiscard]] static LinearSolveCoordinates_ Symmetric(int size);
        [[nodiscard]] static LinearSolveCoordinates_ Banded(int size, int below, int above);
        [[nodiscard]] int Size() const { return size_; }
        [[nodiscard]] size_t Count() const { return count_; }
        [[nodiscard]] bool IsSymmetric() const { return symmetric_; }
        [[nodiscard]] int RowBegin(int row) const;
        [[nodiscard]] int RowEnd(int row) const;
        [[nodiscard]] std::pair<int, int> Location(size_t index) const;
        [[nodiscard]] SquareMatrix_<> Expand(const Vector_<>& parameters) const;
    };

    struct CoordinateLinearSolveAdjoints_ {
        Vector_<> coordinates_;
        Matrix_<> rhs_;
    };

    class CoordinateLinearSolvePullback_ {
        LinearSolveCoordinates_ coordinates_;
        LinearSolvePullback_ solve_;

    public:
        CoordinateLinearSolvePullback_(const LinearSolveCoordinates_& coordinates,
                                       const Vector_<>& parameters,
                                       const Matrix_<>& rhs,
                                       double relativePivotTolerance = 64.0 * std::numeric_limits<double>::epsilon());
        [[nodiscard]] const LinearSolveCoordinates_& Coordinates() const { return coordinates_; }
        [[nodiscard]] const Matrix_<>& Solution() const { return solve_.Solution(); }
        [[nodiscard]] double ScaledMinimumPivot() const { return solve_.ScaledMinimumPivot(); }
        [[nodiscard]] Matrix_<> ReverseRhs(const Matrix_<>& solutionAdjoints) const;
        [[nodiscard]] CoordinateLinearSolveAdjoints_ Reverse(const Matrix_<>& solutionAdjoints) const;
    };
} // namespace Dal

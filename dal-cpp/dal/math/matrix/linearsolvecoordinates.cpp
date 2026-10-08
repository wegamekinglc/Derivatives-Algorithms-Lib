//
// Created by Codex on 2026/10/8.
//

#include <dal/platform/platform.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

#include <dal/math/matrix/linearsolvecoordinates.hpp>
#include <dal/math/matrix/linearsolvecoordinatesinternal.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal {
    LinearSolveCoordinates_::LinearSolveCoordinates_(int size, int below, int above, bool symmetric)
        : size_(size), below_(below), above_(above), symmetric_(symmetric), count_(0) {
        REQUIRE(size > 0, "Linear solve coordinates require positive matrix size");
        const auto n = static_cast<size_t>(size);
        const auto maxElements = static_cast<size_t>(std::numeric_limits<std::ptrdiff_t>::max()) / sizeof(double);
        REQUIRE(n <= maxElements / n, "Linear solve coordinates dense matrix extent exceeds the allocator range");
        REQUIRE(below >= 0 && below < size, "Linear solve coordinates lower bandwidth must be between zero and size minus one");
        REQUIRE(above >= 0 && above < size, "Linear solve coordinates upper bandwidth must be between zero and size minus one");
        count_ = RowOffset(size);
    }

    LinearSolveCoordinates_ LinearSolveCoordinates_::Symmetric(int size) { return LinearSolveCoordinates_(size, 0, 0, true); }

    LinearSolveCoordinates_ LinearSolveCoordinates_::Banded(int size, int below, int above) {
        return LinearSolveCoordinates_(size, below, above, false);
    }

    size_t LinearSolveCoordinates_::RowOffset(int row) const {
        const auto r = static_cast<size_t>(row);
        if (symmetric_)
            return r * (r + 1) / 2;
        const auto lowerRows = static_cast<size_t>(std::min(row, below_));
        const auto upperRows = static_cast<size_t>(std::max(0, row - (size_ - above_)));
        const auto lowerMissing = lowerRows * static_cast<size_t>(below_) - lowerRows * (lowerRows == 0 ? 0 : lowerRows - 1) / 2;
        const auto upperMissing = upperRows * (upperRows + 1) / 2;
        return r * (1 + static_cast<size_t>(below_) + static_cast<size_t>(above_)) - lowerMissing - upperMissing;
    }

    int LinearSolveCoordinates_::RowBegin(int row) const {
        REQUIRE(row >= 0 && row < size_, "Linear solve coordinates row index is outside the matrix");
        return symmetric_ ? 0 : std::max(0, row - below_);
    }

    int LinearSolveCoordinates_::RowEnd(int row) const {
        REQUIRE(row >= 0 && row < size_, "Linear solve coordinates row index is outside the matrix");
        return symmetric_ ? row + 1 : row + std::min(above_, size_ - 1 - row) + 1;
    }

    std::pair<int, int> LinearSolveCoordinates_::Location(size_t index) const {
        REQUIRE(index < count_, "Linear solve coordinate index is outside the parameter vector");
        int low = 0, high = size_;
        while (low + 1 < high) {
            const int middle = low + (high - low) / 2;
            if (index < RowOffset(middle))
                high = middle;
            else
                low = middle;
        }
        return {low, RowBegin(low) + static_cast<int>(index - RowOffset(low))};
    }

    SquareMatrix_<> LinearSolveCoordinates_::Expand(const Vector_<>& parameters) const {
        REQUIRE(parameters.size() == count_, "Linear solve coordinate parameter count does not match the layout");
        SquareMatrix_<> result(size_);
        size_t index = 0;
        for (int row = 0; row < size_; ++row) {
            const int end = RowEnd(row);
            for (int column = RowBegin(row); column < end; ++column) {
                const double value = parameters[index++];
                REQUIRE(std::isfinite(value), "Linear solve coordinate parameters must be finite");
                result(row, column) = value;
                if (symmetric_)
                    result(column, row) = value;
            }
        }
        return result;
    }

    CoordinateLinearSolvePullback_::CoordinateLinearSolvePullback_(const LinearSolveCoordinates_& coordinates,
                                                                   const Vector_<>& parameters,
                                                                   const Matrix_<>& rhs,
                                                                   double relativePivotTolerance)
        : coordinates_(coordinates), solve_(coordinates.Expand(parameters), rhs, relativePivotTolerance) {}

    Matrix_<> CoordinateLinearSolvePullback_::ReverseRhs(const Matrix_<>& solutionAdjoints) const { return solve_.ReverseRhs(solutionAdjoints); }

    CoordinateLinearSolveAdjoints_ CoordinateLinearSolvePullback_::Reverse(const Matrix_<>& solutionAdjoints) const {
        auto rhs = ReverseRhs(solutionAdjoints);
        auto coordinates = Detail::LinearSolveCoordinateAdjoints(coordinates_, Solution(), rhs);
        return {std::move(coordinates), std::move(rhs)};
    }
} // namespace Dal

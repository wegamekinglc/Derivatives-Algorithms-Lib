//
// Created by Codex on 2026/10/8.
//

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <future>
#include <limits>
#include <vector>

#include <dal/math/buffercapacity.hpp>
#include <dal/math/matrix/linearsolvecoordinates.hpp>
#include <dal/platform/platform.hpp>
#include <dal/utilities/exceptions.hpp>

using Dal::BufferCapacityBudget_;
using Dal::BufferCapacityScope_;
using Dal::CoordinateLinearSolveAdjoints_;
using Dal::CoordinateLinearSolvePullback_;
using Dal::LinearSolveCoordinates_;
using Dal::Matrix_;
using Dal::SquareMatrix_;
using Dal::Vector_;

namespace {
    bool Included(bool symmetric, int below, int above, int row, int column) {
        if (symmetric)
            return column <= row;
        return row - column <= below && column - row <= above;
    }

    std::vector<std::pair<int, int>> ExpectedLocations(int n, bool symmetric, int below, int above) {
        std::vector<std::pair<int, int>> result;
        for (int row = 0; row < n; ++row)
            for (int column = 0; column < n; ++column)
                if (Included(symmetric, below, above, row, column))
                    result.emplace_back(row, column);
        return result;
    }

    void CheckExpanded(const LinearSolveCoordinates_& layout, const SquareMatrix_<>& expanded, const Vector_<>& parameters) {
        for (size_t index = 0; index < parameters.size(); ++index) {
            const auto location = layout.Location(index);
            ASSERT_DOUBLE_EQ(expanded(location.first, location.second), parameters[index]);
            if (layout.IsSymmetric())
                ASSERT_DOUBLE_EQ(expanded(location.second, location.first), parameters[index]);
        }
    }

    void CheckFixedZeros(const SquareMatrix_<>& matrix, int below, int above) {
        for (int row = 0; row < matrix.Rows(); ++row)
            for (int column = 0; column < matrix.Cols(); ++column)
                if (!Included(false, below, above, row, column))
                    ASSERT_DOUBLE_EQ(matrix(row, column), 0.0);
    }

    void CheckLayout(int n, bool symmetric, int below, int above) {
        const auto expected = ExpectedLocations(n, symmetric, below, above);
        const auto layout = symmetric ? LinearSolveCoordinates_::Symmetric(n) : LinearSolveCoordinates_::Banded(n, below, above);
        ASSERT_EQ(layout.Size(), n);
        ASSERT_EQ(layout.IsSymmetric(), symmetric);
        ASSERT_EQ(layout.Count(), expected.size());
        Vector_<> parameters(expected.size());
        for (size_t index = 0; index < expected.size(); ++index) {
            ASSERT_EQ(layout.Location(index), expected[index]);
            parameters[index] = static_cast<double>(index) - 0.5;
        }
        const auto expanded = layout.Expand(parameters);
        ASSERT_NO_FATAL_FAILURE(CheckExpanded(layout, expanded, parameters));
        if (!symmetric)
            ASSERT_NO_FATAL_FAILURE(CheckFixedZeros(expanded, below, above));
        for (int row = 0; row < n; ++row) {
            ASSERT_EQ(layout.RowBegin(row), symmetric ? 0 : std::max(0, row - below));
            ASSERT_EQ(layout.RowEnd(row), symmetric ? row + 1 : std::min(n, row + above + 1));
        }
    }

    Matrix_<> Augmented(const SquareMatrix_<>& matrix, const Matrix_<>& rhs) {
        const int n = matrix.Rows();
        Matrix_<> result(n, n + rhs.Cols());
        for (int row = 0; row < n; ++row) {
            for (int column = 0; column < n; ++column)
                result(row, column) = matrix(row, column);
            for (int column = 0; column < rhs.Cols(); ++column)
                result(row, n + column) = rhs(row, column);
        }
        return result;
    }

    int LargestPivot(const Matrix_<>& augmented, int pivot) {
        int best = pivot;
        for (int row = pivot + 1; row < augmented.Rows(); ++row)
            if (std::abs(augmented(row, pivot)) > std::abs(augmented(best, pivot)))
                best = row;
        return best;
    }

    void ReducePivot(Matrix_<>* augmented, int pivot) {
        const int best = LargestPivot(*augmented, pivot);
        for (int column = 0; column < augmented->Cols(); ++column)
            std::swap((*augmented)(pivot, column), (*augmented)(best, column));
        const double divisor = (*augmented)(pivot, pivot);
        for (int column = 0; column < augmented->Cols(); ++column)
            (*augmented)(pivot, column) /= divisor;
        for (int row = 0; row < augmented->Rows(); ++row) {
            if (row == pivot)
                continue;
            const double factor = (*augmented)(row, pivot);
            for (int column = 0; column < augmented->Cols(); ++column)
                (*augmented)(row, column) -= factor * (*augmented)(pivot, column);
        }
    }

    Matrix_<> IndependentSolve(const SquareMatrix_<>& matrix, const Matrix_<>& rhs) {
        auto augmented = Augmented(matrix, rhs);
        for (int pivot = 0; pivot < matrix.Rows(); ++pivot)
            ReducePivot(&augmented, pivot);
        Matrix_<> result(rhs.Rows(), rhs.Cols());
        for (int row = 0; row < rhs.Rows(); ++row)
            for (int column = 0; column < rhs.Cols(); ++column)
                result(row, column) = augmented(row, matrix.Rows() + column);
        return result;
    }

    void CheckMatrix(const Matrix_<>& actual, const Matrix_<>& expected) {
        ASSERT_EQ(actual.Rows(), expected.Rows());
        ASSERT_EQ(actual.Cols(), expected.Cols());
        for (int row = 0; row < actual.Rows(); ++row)
            for (int column = 0; column < actual.Cols(); ++column)
                ASSERT_NEAR(actual(row, column), expected(row, column), 1e-10);
    }

    double Objective(const Matrix_<>& solution, const Matrix_<>& seed) {
        double result = 0.0;
        for (int row = 0; row < solution.Rows(); ++row)
            for (int column = 0; column < solution.Cols(); ++column)
                result += solution(row, column) * seed(row, column);
        return result;
    }

    Vector_<> Parameters(const LinearSolveCoordinates_& layout) {
        Vector_<> result(layout.Count());
        for (size_t index = 0; index < result.size(); ++index) {
            const auto location = layout.Location(index);
            result[index] = location.first == location.second ? 6.0 + location.first : 0.1 * (1 + 2 * location.first - location.second);
        }
        return result;
    }

    Matrix_<> Values(int n, int columns, double shift) {
        Matrix_<> result(n, columns);
        for (int row = 0; row < n; ++row)
            for (int column = 0; column < columns; ++column)
                result(row, column) = shift + 0.5 * row - 0.75 * column;
        return result;
    }

    Matrix_<> Scaled(const Matrix_<>& source, double scale) {
        Matrix_<> result(source.Rows(), source.Cols());
        for (int row = 0; row < source.Rows(); ++row)
            for (int column = 0; column < source.Cols(); ++column)
                result(row, column) = source(row, column) * scale;
        return result;
    }

    void CheckCoordinateDifferences(
        const LinearSolveCoordinates_& layout, const Vector_<>& parameters, const Matrix_<>& rhs, const Matrix_<>& seed, const Vector_<>& adjoints) {
        for (double step : {1e-5, 5e-6, 2e-5}) {
            SCOPED_TRACE(step);
            for (size_t index = 0; index < parameters.size(); ++index) {
                auto plus = parameters, minus = parameters;
                plus[index] += step;
                minus[index] -= step;
                const double difference =
                    (Objective(IndependentSolve(layout.Expand(plus), rhs), seed) - Objective(IndependentSolve(layout.Expand(minus), rhs), seed)) /
                    (2.0 * step);
                ASSERT_NEAR(adjoints[index], difference, 2e-8);
            }
        }
    }

    void CheckRhsDifferences(const SquareMatrix_<>& matrix, const Matrix_<>& rhs, const Matrix_<>& seed, const Matrix_<>& adjoints) {
        for (double step : {1e-5, 5e-6, 2e-5})
            for (int row = 0; row < rhs.Rows(); ++row)
                for (int column = 0; column < rhs.Cols(); ++column) {
                    auto plus = rhs, minus = rhs;
                    plus(row, column) += step;
                    minus(row, column) -= step;
                    const double difference =
                        (Objective(IndependentSolve(matrix, plus), seed) - Objective(IndependentSolve(matrix, minus), seed)) / (2.0 * step);
                    ASSERT_NEAR(adjoints(row, column), difference, 2e-8);
                }
    }

    Matrix_<> DirectionalRhs(const Matrix_<>& deltaB, const SquareMatrix_<>& deltaA, const Matrix_<>& solution) {
        auto result = deltaB;
        for (int row = 0; row < result.Rows(); ++row)
            for (int rhs = 0; rhs < result.Cols(); ++rhs)
                for (int column = 0; column < deltaA.Rows(); ++column)
                    result(row, rhs) -= deltaA(row, column) * solution(column, rhs);
        return result;
    }

    void CheckDirectional(const LinearSolveCoordinates_& layout,
                          const Vector_<>& parameters,
                          const Matrix_<>& solution,
                          const Matrix_<>& seed,
                          const CoordinateLinearSolveAdjoints_& adjoints) {
        Vector_<> deltaP(parameters.size());
        double contraction = 0.0;
        for (size_t index = 0; index < deltaP.size(); ++index) {
            deltaP[index] = 0.2 - 0.03 * index;
            contraction += deltaP[index] * adjoints.coordinates_[index];
        }
        const auto deltaB = Values(seed.Rows(), seed.Cols(), -0.2);
        contraction += Objective(deltaB, adjoints.rhs_);
        const auto deltaX = IndependentSolve(layout.Expand(parameters), DirectionalRhs(deltaB, layout.Expand(deltaP), solution));
        ASSERT_NEAR(Objective(deltaX, seed), contraction, 1e-10);
    }

    void CheckSample(const LinearSolveCoordinates_& layout) {
        const auto parameters = Parameters(layout);
        const auto rhs = Values(layout.Size(), 2, 1.5), seed = Values(layout.Size(), 2, -0.4);
        const CoordinateLinearSolvePullback_ solve(layout, parameters, rhs);
        const auto matrix = layout.Expand(parameters);
        ASSERT_NO_FATAL_FAILURE(CheckMatrix(solve.Solution(), IndependentSolve(matrix, rhs)));
        const auto adjoints = solve.Reverse(seed);
        ASSERT_NO_FATAL_FAILURE(CheckCoordinateDifferences(layout, parameters, rhs, seed, adjoints.coordinates_));
        ASSERT_NO_FATAL_FAILURE(CheckRhsDifferences(matrix, rhs, seed, adjoints.rhs_));
        ASSERT_NO_FATAL_FAILURE(CheckDirectional(layout, parameters, solve.Solution(), seed, adjoints));
    }

    void CheckScaled(const CoordinateLinearSolveAdjoints_& actual, const CoordinateLinearSolveAdjoints_& expected, double scale) {
        ASSERT_EQ(actual.coordinates_.size(), expected.coordinates_.size());
        for (size_t index = 0; index < actual.coordinates_.size(); ++index)
            ASSERT_NEAR(actual.coordinates_[index], scale * expected.coordinates_[index], 1e-10);
        ASSERT_NO_FATAL_FAILURE(CheckMatrix(actual.rhs_, Scaled(expected.rhs_, scale)));
    }
} // namespace

TEST(LinearSolvePullbackTest, TestSymmetricCoordinatesHavePairedOffDiagonalGradient) {
    const auto coordinates = LinearSolveCoordinates_::Symmetric(2);
    const Vector_<> parameters = {2.0, 1.0, 3.0};
    Matrix_<> rhs(2, 1), seed(2, 1);
    rhs(0, 0) = 1.0;
    rhs(1, 0) = 2.0;
    seed(0, 0) = 1.0;
    seed(1, 0) = -2.0;
    const CoordinateLinearSolvePullback_ solve(coordinates, parameters, rhs);
    ASSERT_NEAR(solve.Solution()(0, 0), 0.2, 1e-10);
    ASSERT_NEAR(solve.Solution()(1, 0), 0.6, 1e-10);
    const auto adjoints = solve.Reverse(seed);
    ASSERT_EQ(adjoints.coordinates_.size(), 3U);
    ASSERT_NEAR(adjoints.coordinates_[0], -0.2, 1e-10);
    ASSERT_NEAR(adjoints.coordinates_[1], -0.4, 1e-10);
    ASSERT_NEAR(adjoints.coordinates_[2], 0.6, 1e-10);
    ASSERT_NEAR(adjoints.rhs_(0, 0), 1.0, 1e-10);
    ASSERT_NEAR(adjoints.rhs_(1, 0), -1.0, 1e-10);
}

TEST(LinearSolvePullbackTest, TestCoordinateOrderAndCountsExhaustSmallLayouts) {
    for (int n = 1; n <= 7; ++n) {
        ASSERT_NO_FATAL_FAILURE(CheckLayout(n, true, 0, 0));
        for (int below = 0; below < n; ++below)
            for (int above = 0; above < n; ++above)
                ASSERT_NO_FATAL_FAILURE(CheckLayout(n, false, below, above));
    }
}

TEST(LinearSolvePullbackTest, TestBandedCoordinatesHaveNoBoundaryPadding) {
    const auto layout = LinearSolveCoordinates_::Banded(4, 1, 2);
    ASSERT_EQ(layout.Count(), 12U);
    const auto matrix = layout.Expand(Vector_<>(layout.Count(), 2.0));
    ASSERT_DOUBLE_EQ(matrix(0, 3), 0.0);
    ASSERT_DOUBLE_EQ(matrix(2, 0), 0.0);
    ASSERT_DOUBLE_EQ(matrix(3, 0), 0.0);
    ASSERT_DOUBLE_EQ(matrix(3, 1), 0.0);
    ASSERT_EQ(layout.Location(11), std::make_pair(3, 3));
}

TEST(LinearSolvePullbackTest, TestCoordinateInvalidBoundariesRejectBeforeAllocation) {
    const auto layout = LinearSolveCoordinates_::Banded(3, 1, 0);
    const Vector_<> wrongCount = {1.0, 2.0};
    BufferCapacityBudget_ budget(0);
    BufferCapacityScope_ capacity(&budget);
    ASSERT_THROW(static_cast<void>(LinearSolveCoordinates_::Symmetric(0)), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(LinearSolveCoordinates_::Banded(-1, 0, 0)), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(LinearSolveCoordinates_::Symmetric(std::numeric_limits<int>::max())), Dal::Exception_);
    for (int width : {-1, 3}) {
        ASSERT_THROW(static_cast<void>(LinearSolveCoordinates_::Banded(3, width, 0)), Dal::Exception_);
        ASSERT_THROW(static_cast<void>(LinearSolveCoordinates_::Banded(3, 0, width)), Dal::Exception_);
    }
    ASSERT_THROW(static_cast<void>(layout.RowBegin(-1)), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(layout.RowEnd(3)), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(layout.Location(layout.Count())), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(layout.Location(std::numeric_limits<size_t>::max())), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(layout.Expand(wrongCount)), Dal::Exception_);
    ASSERT_EQ(budget.PeakCapacityBytes(), 0U);
}

TEST(LinearSolvePullbackTest, TestLargeCoordinateMetadataUsesWideCountsWithoutAllocation) {
    if (sizeof(size_t) < 8)
        GTEST_SKIP();
    BufferCapacityBudget_ budget(0);
    BufferCapacityScope_ capacity(&budget);
    const auto symmetric = LinearSolveCoordinates_::Symmetric(65536);
    ASSERT_EQ(symmetric.Count(), 2147516416ULL);
    ASSERT_EQ(symmetric.Location(symmetric.Count() - 1), std::make_pair(65535, 65535));
    const auto full = LinearSolveCoordinates_::Banded(65536, 65535, 65535);
    ASSERT_EQ(full.Count(), 4294967296ULL);
    ASSERT_EQ(full.Location(full.Count() - 1), std::make_pair(65535, 65535));
    ASSERT_EQ(budget.PeakCapacityBytes(), 0U);
}

TEST(LinearSolvePullbackTest, TestSymmetricCoordinatesAcceptIndefinitePivotedSystems) {
    const auto layout = LinearSolveCoordinates_::Symmetric(2);
    Matrix_<> rhs(2, 1), seed(2, 1);
    rhs(0, 0) = 4.0;
    rhs(1, 0) = 5.0;
    seed(0, 0) = 1.0;
    seed(1, 0) = -3.0;
    const CoordinateLinearSolvePullback_ solve(layout, Vector_<>{0.0, 2.0, 1.0}, rhs);
    ASSERT_NEAR(solve.Solution()(0, 0), 1.5, 1e-10);
    ASSERT_NEAR(solve.Solution()(1, 0), 2.0, 1e-10);
    const auto adjoints = solve.Reverse(seed);
    ASSERT_NEAR(adjoints.coordinates_[0], 2.625, 1e-10);
    ASSERT_NEAR(adjoints.coordinates_[1], 2.75, 1e-10);
    ASSERT_NEAR(adjoints.coordinates_[2], -1.0, 1e-10);
}

TEST(LinearSolvePullbackTest, TestCoordinateMultipleRhsDifferencesAndDirectionalIdentity) {
    ASSERT_NO_FATAL_FAILURE(CheckSample(LinearSolveCoordinates_::Symmetric(5)));
    for (const auto widths : {std::make_pair(0, 0), std::make_pair(0, 2), std::make_pair(2, 0), std::make_pair(1, 2), std::make_pair(4, 4)})
        ASSERT_NO_FATAL_FAILURE(CheckSample(LinearSolveCoordinates_::Banded(5, widths.first, widths.second)));
}

TEST(LinearSolvePullbackTest, TestCoordinateCacheOwnsSnapshotsAndRepeatedReverseResults) {
    auto layout = LinearSolveCoordinates_::Banded(3, 1, 2);
    auto parameters = Parameters(layout);
    auto rhs = Values(3, 2, 1.5), seed = Values(3, 2, -0.4);
    const CoordinateLinearSolvePullback_ solve(layout, parameters, rhs);
    const auto reference = solve.Reverse(seed);
    const auto solution = solve.Solution();
    layout = LinearSolveCoordinates_::Symmetric(1);
    parameters.Fill(0.0);
    rhs(0, 0) = std::numeric_limits<double>::infinity();
    ASSERT_EQ(solve.Coordinates().Count(), 8U);
    ASSERT_FALSE(solve.Coordinates().IsSymmetric());
    ASSERT_NO_FATAL_FAILURE(CheckMatrix(solve.Solution(), solution));
    ASSERT_NO_FATAL_FAILURE(CheckScaled(solve.Reverse(seed), reference, 1.0));
    ASSERT_NO_FATAL_FAILURE(CheckScaled(solve.Reverse(Scaled(seed, -2.0)), reference, -2.0));
    ASSERT_NO_FATAL_FAILURE(CheckScaled(solve.Reverse(Matrix_<>(3, 2)), reference, 0.0));
    const auto copied = solve;
    ASSERT_NO_FATAL_FAILURE(CheckScaled(copied.Reverse(seed), reference, 1.0));
}

TEST(LinearSolvePullbackTest, TestCoordinateReverseStorageScalesWithParameters) {
    const auto layout = LinearSolveCoordinates_::Banded(16, 0, 0);
    const auto parameters = Parameters(layout);
    const auto rhs = Values(16, 2, 1.5), seed = Values(16, 2, -0.4);
    const CoordinateLinearSolvePullback_ solve(layout, parameters, rhs);
    const size_t bytes = (layout.Count() + 16U * 2U) * sizeof(double);
    BufferCapacityBudget_ budget(bytes);
    BufferCapacityScope_ capacity(&budget);
    {
        const auto result = solve.Reverse(seed);
        ASSERT_EQ(result.coordinates_.size(), layout.Count());
        ASSERT_EQ(budget.CapacityBytes(), bytes);
    }
    ASSERT_EQ(budget.CapacityBytes(), 0U);
    ASSERT_EQ(budget.PeakCapacityBytes(), bytes);
    ASSERT_LT(bytes, (16U * 16U + 16U * 2U) * sizeof(double));
}

TEST(LinearSolvePullbackTest, TestCoordinateReverseOneByteShortRefundsAndRecovers) {
    const auto layout = LinearSolveCoordinates_::Banded(16, 0, 0);
    const auto parameters = Parameters(layout);
    const auto rhs = Values(16, 2, 1.5), seed = Values(16, 2, -0.4);
    const CoordinateLinearSolvePullback_ solve(layout, parameters, rhs);
    const auto expected = solve.ReverseRhs(seed);
    const size_t bytes = (layout.Count() + 16U * 2U) * sizeof(double);
    BufferCapacityBudget_ budget(bytes - 1);
    BufferCapacityScope_ capacity(&budget);
    ASSERT_THROW(static_cast<void>(solve.Reverse(seed)), Dal::Exception_);
    ASSERT_EQ(budget.CapacityBytes(), 0U);
    {
        const auto rhsOnly = solve.ReverseRhs(seed);
        ASSERT_NO_FATAL_FAILURE(CheckMatrix(rhsOnly, expected));
    }
    ASSERT_EQ(budget.CapacityBytes(), 0U);
}

TEST(LinearSolvePullbackTest, TestCoordinateReverseOverflowLeavesCacheUsableAndRhsOnlyWorks) {
    const auto layout = LinearSolveCoordinates_::Symmetric(1);
    Matrix_<> rhs(1, 1), seed(1, 1);
    rhs(0, 0) = 1e150;
    seed(0, 0) = 1.0;
    const CoordinateLinearSolvePullback_ solve(layout, Vector_<>{1e-150}, rhs);
    ASSERT_THROW(static_cast<void>(solve.Reverse(seed)), Dal::Exception_);
    ASSERT_NEAR(solve.ReverseRhs(seed)(0, 0), 1e150, 1e138);
    seed(0, 0) = 0.0;
    ASSERT_DOUBLE_EQ(solve.Reverse(seed).coordinates_[0], 0.0);
    ASSERT_THROW(static_cast<void>(solve.Reverse(Matrix_<>(2, 1))), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(solve.Reverse(Matrix_<>(1, 0))), Dal::Exception_);
    seed(0, 0) = std::numeric_limits<double>::quiet_NaN();
    ASSERT_THROW(static_cast<void>(solve.Reverse(seed)), Dal::Exception_);
    seed(0, 0) = 0.0;
    ASSERT_DOUBLE_EQ(solve.ReverseRhs(seed)(0, 0), 0.0);
}

TEST(LinearSolvePullbackTest, TestCoordinateNonFiniteParametersRhsAndSeedsReject) {
    const auto layout = LinearSolveCoordinates_::Symmetric(2);
    const auto rhs = Values(2, 2, 1.5), seed = Values(2, 2, -0.4);
    const CoordinateLinearSolvePullback_ solve(layout, Vector_<>{2.0, 1.0, 3.0}, rhs);
    for (double value :
         {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()}) {
        ASSERT_THROW(static_cast<void>(layout.Expand(Vector_<>{2.0, value, 3.0})), Dal::Exception_);
        auto badRhs = rhs, badSeed = seed;
        badRhs(0, 1) = value;
        badSeed(1, 0) = value;
        ASSERT_THROW(static_cast<void>(CoordinateLinearSolvePullback_(layout, Vector_<>{2.0, 1.0, 3.0}, badRhs)), Dal::Exception_);
        ASSERT_THROW(static_cast<void>(solve.Reverse(badSeed)), Dal::Exception_);
    }
}

TEST(LinearSolvePullbackTest, TestCoordinateSingularShapeToleranceAndForwardRangeReject) {
    const auto layout = LinearSolveCoordinates_::Banded(2, 0, 0);
    const auto rhs = Values(2, 1, 1.5);
    ASSERT_THROW(static_cast<void>(CoordinateLinearSolvePullback_(layout, Vector_<>{1.0, 0.0}, rhs)), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(CoordinateLinearSolvePullback_(layout, Vector_<>{1.0, 1e-20}, rhs)), Dal::Exception_);
    const CoordinateLinearSolvePullback_ accepted(layout, Vector_<>{1.0, 1e-20}, rhs, 1e-25);
    ASSERT_DOUBLE_EQ(accepted.ScaledMinimumPivot(), 1e-20);
    ASSERT_THROW(static_cast<void>(CoordinateLinearSolvePullback_(layout, Vector_<>{1.0, 2.0}, Matrix_<>(1, 1))), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(CoordinateLinearSolvePullback_(layout, Vector_<>{1.0, 2.0}, Matrix_<>(2, 0))), Dal::Exception_);
    for (double tolerance : {0.0, -1.0, 1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
        ASSERT_THROW(static_cast<void>(CoordinateLinearSolvePullback_(layout, Vector_<>{1.0, 2.0}, rhs, tolerance)), Dal::Exception_);
    Matrix_<> overflowRhs(1, 1);
    overflowRhs(0, 0) = 1e250;
    ASSERT_THROW(static_cast<void>(CoordinateLinearSolvePullback_(LinearSolveCoordinates_::Symmetric(1), Vector_<>{1e-150}, overflowRhs)),
                 Dal::Exception_);
}

TEST(LinearSolvePullbackTest, TestCoordinateConcurrentConstReverseHasIndependentOwningResults) {
    const auto layout = LinearSolveCoordinates_::Banded(5, 1, 2);
    const auto rhs = Values(5, 2, 1.5), seed = Values(5, 2, -0.4);
    const CoordinateLinearSolvePullback_ solve(layout, Parameters(layout), rhs);
    const auto expected = solve.Reverse(seed);
    std::vector<std::future<CoordinateLinearSolveAdjoints_>> futures;
    for (int request = 1; request <= 4; ++request)
        futures.push_back(std::async(std::launch::async, [&, request] { return solve.Reverse(Scaled(seed, static_cast<double>(request))); }));
    for (int request = 1; request <= 4; ++request)
        ASSERT_NO_FATAL_FAILURE(CheckScaled(futures[request - 1].get(), expected, static_cast<double>(request)));
}

TEST(LinearSolvePullbackTest, TestCoordinateConstructionExactBudgetAndOneByteShortRefund) {
    const auto layout = LinearSolveCoordinates_::Banded(4, 1, 1);
    const auto parameters = Parameters(layout);
    const auto rhs = Values(4, 2, 1.5);
    const size_t retained = 16U * sizeof(double) + 4U * sizeof(int) + 8U * sizeof(double);
    const size_t peak = retained + 16U * sizeof(double);
    {
        BufferCapacityBudget_ budget(peak);
        BufferCapacityScope_ capacity(&budget);
        {
            const CoordinateLinearSolvePullback_ solve(layout, parameters, rhs);
            ASSERT_EQ(budget.CapacityBytes(), retained);
            ASSERT_EQ(budget.PeakCapacityBytes(), peak);
        }
        ASSERT_EQ(budget.CapacityBytes(), 0U);
    }
    {
        BufferCapacityBudget_ budget(peak - 1);
        BufferCapacityScope_ capacity(&budget);
        ASSERT_THROW(static_cast<void>(CoordinateLinearSolvePullback_(layout, parameters, rhs)), Dal::Exception_);
        ASSERT_EQ(budget.CapacityBytes(), 0U);
        {
            const auto expanded = layout.Expand(parameters);
            ASSERT_EQ(expanded.Rows(), 4);
        }
        ASSERT_EQ(budget.CapacityBytes(), 0U);
    }
}

TEST(LinearSolvePullbackTest, TestSymmetricPairedCoordinateOverflowRefundsAndRecovers) {
    const auto layout = LinearSolveCoordinates_::Symmetric(2);
    Matrix_<> rhs(2, 1, 1.0), seed(2, 1, -1e308);
    const CoordinateLinearSolvePullback_ solve(layout, Vector_<>{1.0, 0.0, 1.0}, rhs);
    BufferCapacityBudget_ budget(1024);
    BufferCapacityScope_ capacity(&budget);
    ASSERT_THROW(static_cast<void>(solve.Reverse(seed)), Dal::Exception_);
    ASSERT_EQ(budget.CapacityBytes(), 0U);
    {
        const auto rhsOnly = solve.ReverseRhs(seed);
        ASSERT_DOUBLE_EQ(rhsOnly(0, 0), -1e308);
        ASSERT_DOUBLE_EQ(rhsOnly(1, 0), -1e308);
    }
    ASSERT_EQ(budget.CapacityBytes(), 0U);
    seed(0, 0) = seed(1, 0) = -1e307;
    {
        const auto accepted = solve.Reverse(seed);
        ASSERT_NEAR(accepted.coordinates_[1], 2e307, 1e295);
    }
    ASSERT_EQ(budget.CapacityBytes(), 0U);
}

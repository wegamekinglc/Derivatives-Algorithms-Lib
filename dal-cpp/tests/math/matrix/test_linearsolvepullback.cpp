//
// Created by Codex on 2026/10/7.
//

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <future>
#include <limits>
#include <vector>

#include <dal/math/aad/recording.hpp>
#include <dal/math/matrix/linearsolvepullback.hpp>
#include <dal/utilities/exceptions.hpp>

using namespace Dal;

TEST(LinearSolvePullbackTest, TestRhsOnlyPullbackAvoidsUnusedMatrixOverflow) {
    SquareMatrix_<> matrix(1);
    matrix(0, 0) = 1e-150;
    Matrix_<> rhs(1, 1), seed(1, 1);
    rhs(0, 0) = 1e150;
    seed(0, 0) = 1.0;
    const LinearSolvePullback_ solve(matrix, rhs);
    ASSERT_THROW(static_cast<void>(solve.Reverse(seed)), Exception_);
    const auto contribution = solve.ReverseRhs(seed);
    ASSERT_EQ(contribution.Rows(), 1);
    ASSERT_EQ(contribution.Cols(), 1);
    ASSERT_NEAR(contribution(0, 0), 1e150, 1e138);
    ASSERT_THROW(static_cast<void>(solve.ReverseRhs(Matrix_<>(2, 1))), Exception_);
    seed(0, 0) = std::numeric_limits<double>::infinity();
    ASSERT_THROW(static_cast<void>(solve.ReverseRhs(seed)), Exception_);
}

namespace {
    SquareMatrix_<> PermutedMatrix() {
        SquareMatrix_<> a(3);
        const double entries[3][3] = {{0.0, 2.0, 1.0}, {1.0, 0.0, 3.0}, {4.0, 1.0, 0.0}};
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 3; ++col)
                a(row, col) = entries[row][col];
        return a;
    }

    Matrix_<> KnownSolution() {
        Matrix_<> x(3, 2);
        const double entries[3][2] = {{1.0, -2.0}, {2.0, 0.5}, {-1.0, 3.0}};
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 2; ++col)
                x(row, col) = entries[row][col];
        return x;
    }

    Matrix_<> Multiply(const SquareMatrix_<>& a, const Matrix_<>& x, bool transpose = false) {
        Matrix_<> result(a.Rows(), x.Cols());
        for (int row = 0; row < a.Rows(); ++row)
            for (int rhs = 0; rhs < x.Cols(); ++rhs)
                for (int col = 0; col < a.Rows(); ++col)
                    result(row, rhs) += (transpose ? a(col, row) : a(row, col)) * x(col, rhs);
        return result;
    }

    Matrix_<> Seeds() {
        Matrix_<> seed(3, 2);
        const double entries[3][2] = {{2.0, -1.0}, {-0.5, 0.0}, {3.0, 4.0}};
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 2; ++col)
                seed(row, col) = entries[row][col];
        return seed;
    }

    void AssertMatrixNear(const Matrix_<>& actual, const Matrix_<>& expected, double tolerance = 1e-10) {
        ASSERT_EQ(actual.Rows(), expected.Rows());
        ASSERT_EQ(actual.Cols(), expected.Cols());
        for (int row = 0; row < actual.Rows(); ++row)
            for (int col = 0; col < actual.Cols(); ++col)
                ASSERT_NEAR(actual(row, col), expected(row, col), tolerance) << row << "," << col;
    }

    template <class T_> Matrix_<T_> Augment(const SquareMatrix_<T_>& a, const Matrix_<T_>& b) {
        const int n = a.Rows();
        Matrix_<T_> augmented(n, n + b.Cols());
        for (int row = 0; row < n; ++row) {
            for (int col = 0; col < n; ++col)
                augmented(row, col) = a(row, col);
            for (int rhs = 0; rhs < b.Cols(); ++rhs)
                augmented(row, n + rhs) = b(row, rhs);
        }
        return augmented;
    }

    template <class T_> void ReduceColumn(int pivot, Matrix_<T_>* augmented) {
        int best = pivot;
        for (int row = pivot + 1; row < augmented->Rows(); ++row)
            if (std::abs(static_cast<double>((*augmented)(row, pivot))) > std::abs(static_cast<double>((*augmented)(best, pivot))))
                best = row;
        for (int col = 0; col < augmented->Cols(); ++col)
            std::swap((*augmented)(pivot, col), (*augmented)(best, col));
        const T_ divisor = (*augmented)(pivot, pivot);
        for (int col = pivot; col < augmented->Cols(); ++col)
            (*augmented)(pivot, col) = (*augmented)(pivot, col) / divisor;
        for (int row = 0; row < augmented->Rows(); ++row) {
            if (row == pivot)
                continue;
            const T_ factor = (*augmented)(row, pivot);
            for (int col = pivot; col < augmented->Cols(); ++col)
                (*augmented)(row, col) = (*augmented)(row, col) - factor * (*augmented)(pivot, col);
        }
    }

    template <class T_> Matrix_<T_> GaussJordan(const SquareMatrix_<T_>& a, const Matrix_<T_>& b) {
        const int n = a.Rows();
        auto augmented = Augment(a, b);
        for (int pivot = 0; pivot < n; ++pivot)
            ReduceColumn(pivot, &augmented);
        Matrix_<T_> x(n, b.Cols());
        for (int row = 0; row < n; ++row)
            for (int rhs = 0; rhs < b.Cols(); ++rhs)
                x(row, rhs) = augmented(row, n + rhs);
        return x;
    }

    double Objective(const Matrix_<>& x, const Matrix_<>& seed) {
        double result = 0.0;
        for (int row = 0; row < x.Rows(); ++row)
            for (int col = 0; col < x.Cols(); ++col)
                result += x(row, col) * seed(row, col);
        return result;
    }

    void AssertFiniteDifferences(const SquareMatrix_<>& a, const Matrix_<>& b, const Matrix_<>& seed, const LinearSolveAdjoints_& adjoints) {
        for (double step : {1e-5, 5e-6, 2e-5}) {
            SCOPED_TRACE(step);
            for (int row = 0; row < a.Rows(); ++row) {
                for (int col = 0; col < a.Rows(); ++col) {
                    auto plus = a, minus = a;
                    plus(row, col) += step;
                    minus(row, col) -= step;
                    const double derivative = (Objective(GaussJordan(plus, b), seed) - Objective(GaussJordan(minus, b), seed)) / (2.0 * step);
                    ASSERT_NEAR(derivative, adjoints.matrix_(row, col), 2e-8 + 2e-8 * std::abs(adjoints.matrix_(row, col)));
                }
                for (int col = 0; col < b.Cols(); ++col) {
                    auto plus = b, minus = b;
                    plus(row, col) += step;
                    minus(row, col) -= step;
                    const double derivative = (Objective(GaussJordan(a, plus), seed) - Objective(GaussJordan(a, minus), seed)) / (2.0 * step);
                    ASSERT_NEAR(derivative, adjoints.rhs_(row, col), 2e-8 + 2e-8 * std::abs(adjoints.rhs_(row, col)));
                }
            }
        }
    }

    void
    AssertNativeAdjoints(const SquareMatrix_<AAD::Number_>& activeA, const Matrix_<AAD::Number_>& activeB, const LinearSolveAdjoints_& expected) {
        for (int row = 0; row < activeA.Rows(); ++row) {
            for (int col = 0; col < activeA.Rows(); ++col)
                ASSERT_NEAR(AAD::AdjointValue(activeA(row, col)), expected.matrix_(row, col), 1e-10);
            for (int col = 0; col < activeB.Cols(); ++col)
                ASSERT_NEAR(AAD::AdjointValue(activeB(row, col)), expected.rhs_(row, col), 1e-10);
        }
    }
} // namespace

TEST(LinearSolvePullbackTest, TestAnalyticDenseGradient) {
    SquareMatrix_<> a(2);
    a(0, 0) = 2.0;
    a(0, 1) = 1.0;
    a(1, 0) = 1.0;
    a(1, 1) = 3.0;
    Matrix_<> b(2, 1), seed(2, 1);
    b(0, 0) = 1.0;
    b(1, 0) = 2.0;
    seed(0, 0) = 1.0;
    seed(1, 0) = -2.0;
    const LinearSolvePullback_ solve(a, b);
    ASSERT_NEAR(solve.Solution()(0, 0), 0.2, 1e-10);
    ASSERT_NEAR(solve.Solution()(1, 0), 0.6, 1e-10);
    const auto adjoints = solve.Reverse(seed);
    ASSERT_NEAR(adjoints.rhs_(0, 0), 1.0, 1e-10);
    ASSERT_NEAR(adjoints.rhs_(1, 0), -1.0, 1e-10);
    ASSERT_NEAR(adjoints.matrix_(0, 0), -0.2, 1e-10);
    ASSERT_NEAR(adjoints.matrix_(0, 1), -0.6, 1e-10);
    ASSERT_NEAR(adjoints.matrix_(1, 0), 0.2, 1e-10);
    ASSERT_NEAR(adjoints.matrix_(1, 1), 0.6, 1e-10);
}

TEST(LinearSolvePullbackTest, TestNonsymmetricPermutationAndMultipleRhs) {
    const auto a = PermutedMatrix();
    const auto expected = KnownSolution();
    const auto b = Multiply(a, expected);
    const auto seed = Seeds();
    const LinearSolvePullback_ solve(a, b);
    AssertMatrixNear(solve.Solution(), expected);
    AssertMatrixNear(Multiply(a, solve.Solution()), b);
    const auto adjoints = solve.Reverse(seed);
    AssertMatrixNear(Multiply(a, adjoints.rhs_, true), seed);
    SquareMatrix_<> sum(a.Rows());
    for (int rhs = 0; rhs < b.Cols(); ++rhs) {
        Matrix_<> column(b.Rows(), 1), weight(b.Rows(), 1);
        for (int row = 0; row < b.Rows(); ++row) {
            column(row, 0) = b(row, rhs);
            weight(row, 0) = seed(row, rhs);
        }
        const LinearSolvePullback_ single(a, column);
        const auto individual = single.Reverse(weight);
        for (int row = 0; row < a.Rows(); ++row) {
            ASSERT_NEAR(single.Solution()(row, 0), expected(row, rhs), 1e-10);
            ASSERT_NEAR(individual.rhs_(row, 0), adjoints.rhs_(row, rhs), 1e-10);
            for (int col = 0; col < a.Rows(); ++col)
                sum(row, col) += individual.matrix_(row, col);
        }
    }
    AssertMatrixNear(adjoints.matrix_, sum);
}

TEST(LinearSolvePullbackTest, TestNativeScalarAndFiniteDifferenceOracles) {
    using namespace Dal::AAD;
    const auto a = PermutedMatrix();
    const auto b = Multiply(a, KnownSolution());
    const auto seed = Seeds();
    const LinearSolvePullback_ solve(a, b);
    const auto adjoints = solve.Reverse(seed);
    auto mode = SetNumResultsForAAD(false);
    RecordingScope_ scope;
    SquareMatrix_<Number_> activeA(a.Rows());
    Matrix_<Number_> activeB(b.Rows(), b.Cols());
    for (int row = 0; row < a.Rows(); ++row) {
        for (int col = 0; col < a.Rows(); ++col)
            scope.RegisterInput(activeA(row, col), a(row, col));
        for (int col = 0; col < b.Cols(); ++col)
            scope.RegisterInput(activeB(row, col), b(row, col));
    }
    scope.StartRecording();
    const auto activeX = GaussJordan(activeA, activeB);
    Number_ objective = 0.0;
    for (int row = 0; row < b.Rows(); ++row)
        for (int col = 0; col < b.Cols(); ++col)
            objective = objective + activeX(row, col) * seed(row, col);
    scope.FinishRecording();
    scope.ClearAdjoints();
    Adjoint(objective) = 1.0;
    scope.Reverse();
    AssertNativeAdjoints(activeA, activeB, adjoints);
    scope.Close();
    AssertFiniteDifferences(a, b, seed, adjoints);
}

TEST(LinearSolvePullbackTest, TestDirectionalAdjointIdentity) {
    const auto a = PermutedMatrix();
    const auto b = Multiply(a, KnownSolution());
    const auto seed = Seeds();
    const LinearSolvePullback_ solve(a, b);
    const auto adjoints = solve.Reverse(seed);
    SquareMatrix_<> directionA(a.Rows());
    Matrix_<> directionB(b.Rows(), b.Cols());
    for (int row = 0; row < a.Rows(); ++row) {
        for (int col = 0; col < a.Rows(); ++col)
            directionA(row, col) = 0.25 * (1 + row - 2 * col);
        for (int col = 0; col < b.Cols(); ++col)
            directionB(row, col) = 0.5 * (1 - row + col);
    }
    const auto product = Multiply(directionA, solve.Solution());
    auto linearizedRhs = directionB;
    double reverse = 0.0;
    for (int row = 0; row < a.Rows(); ++row) {
        for (int col = 0; col < a.Rows(); ++col)
            reverse += adjoints.matrix_(row, col) * directionA(row, col);
        for (int col = 0; col < b.Cols(); ++col) {
            reverse += adjoints.rhs_(row, col) * directionB(row, col);
            linearizedRhs(row, col) -= product(row, col);
        }
    }
    ASSERT_NEAR(Objective(GaussJordan(a, linearizedRhs), seed), reverse, 1e-10);
}

TEST(LinearSolvePullbackTest, TestSeedLinearityOwnershipAndRepeatedReverse) {
    auto a = PermutedMatrix();
    auto b = Multiply(a, KnownSolution());
    const auto seed = Seeds();
    const LinearSolvePullback_ solve(a, b);
    const auto first = solve.Reverse(seed);
    a(0, 0) = 123.0;
    b(0, 0) = -321.0;
    const auto again = solve.Reverse(seed);
    AssertMatrixNear(solve.Solution(), KnownSolution());
    AssertMatrixNear(first.matrix_, again.matrix_);
    AssertMatrixNear(first.rhs_, again.rhs_);
    auto scaled = seed;
    for (int row = 0; row < scaled.Rows(); ++row)
        for (int col = 0; col < scaled.Cols(); ++col)
            scaled(row, col) *= -2.5;
    const auto opposite = solve.Reverse(scaled);
    const auto zero = solve.Reverse(Matrix_<>(seed.Rows(), seed.Cols()));
    const auto otherSeed = KnownSolution();
    const auto other = solve.Reverse(otherSeed);
    auto combination = seed;
    for (int row = 0; row < seed.Rows(); ++row)
        for (int col = 0; col < seed.Cols(); ++col)
            combination(row, col) = 2.0 * seed(row, col) - 3.0 * otherSeed(row, col);
    const auto combined = solve.Reverse(combination);
    for (int row = 0; row < seed.Rows(); ++row) {
        for (int col = 0; col < seed.Rows(); ++col) {
            ASSERT_NEAR(opposite.matrix_(row, col), -2.5 * first.matrix_(row, col), 1e-10);
            ASSERT_DOUBLE_EQ(zero.matrix_(row, col), 0.0);
            ASSERT_NEAR(combined.matrix_(row, col), 2.0 * first.matrix_(row, col) - 3.0 * other.matrix_(row, col), 1e-10);
        }
        for (int col = 0; col < seed.Cols(); ++col) {
            ASSERT_NEAR(opposite.rhs_(row, col), -2.5 * first.rhs_(row, col), 1e-10);
            ASSERT_DOUBLE_EQ(zero.rhs_(row, col), 0.0);
            ASSERT_NEAR(combined.rhs_(row, col), 2.0 * first.rhs_(row, col) - 3.0 * other.rhs_(row, col), 1e-10);
            ASSERT_DOUBLE_EQ(seed(row, col), Seeds()(row, col));
        }
    }
}

TEST(LinearSolvePullbackTest, TestScaleAndSingleEntrySystems) {
    const auto original = PermutedMatrix();
    const auto expected = KnownSolution();
    const auto seed = Seeds();
    const auto reference = LinearSolvePullback_(original, Multiply(original, expected)).Reverse(seed);
    for (double scale : {1e-6, 1.0, 1e6}) {
        SCOPED_TRACE(scale);
        auto a = original;
        for (int row = 0; row < a.Rows(); ++row)
            for (int col = 0; col < a.Rows(); ++col)
                a(row, col) *= scale;
        const LinearSolvePullback_ solve(a, Multiply(a, expected));
        AssertMatrixNear(solve.Solution(), expected);
        const auto adjoints = solve.Reverse(seed);
        for (int row = 0; row < a.Rows(); ++row) {
            for (int col = 0; col < a.Rows(); ++col)
                ASSERT_NEAR(scale * adjoints.matrix_(row, col), reference.matrix_(row, col), 1e-10);
            for (int col = 0; col < seed.Cols(); ++col)
                ASSERT_NEAR(scale * adjoints.rhs_(row, col), reference.rhs_(row, col), 1e-10);
        }
        ASSERT_GT(solve.ScaledMinimumPivot(), 0.0);
    }
    SquareMatrix_<> a(1, -2.0);
    Matrix_<> b(1, 2), seedOne(1, 2);
    b(0, 0) = 6.0;
    b(0, 1) = -8.0;
    seedOne(0, 0) = 2.0;
    seedOne(0, 1) = -1.0;
    const LinearSolvePullback_ solve(a, b);
    const auto adjoints = solve.Reverse(seedOne);
    ASSERT_DOUBLE_EQ(solve.Solution()(0, 0), -3.0);
    ASSERT_DOUBLE_EQ(solve.Solution()(0, 1), 4.0);
    ASSERT_DOUBLE_EQ(adjoints.rhs_(0, 0), -1.0);
    ASSERT_DOUBLE_EQ(adjoints.rhs_(0, 1), 0.5);
    ASSERT_DOUBLE_EQ(adjoints.matrix_(0, 0), -5.0);
}

TEST(LinearSolvePullbackTest, TestRejectInvalidShapesPivotsAndFiniteInputs) {
    const auto a = PermutedMatrix();
    const auto b = Multiply(a, KnownSolution());
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();
    ASSERT_THROW((void)LinearSolvePullback_(SquareMatrix_<>(), Matrix_<>()), Exception_);
    ASSERT_THROW((void)LinearSolvePullback_(a, Matrix_<>(2, 2)), Exception_);
    ASSERT_THROW((void)LinearSolvePullback_(a, Matrix_<>(3, 0)), Exception_);
    for (double tolerance : {0.0, -1.0, 1.0, nan, infinity})
        ASSERT_THROW((void)LinearSolvePullback_(a, b, tolerance), Exception_);
    for (double value : {nan, infinity, -infinity}) {
        auto invalidA = a;
        invalidA(1, 2) = value;
        ASSERT_THROW((void)LinearSolvePullback_(invalidA, b), Exception_);
        auto invalidB = b;
        invalidB(2, 1) = value;
        ASSERT_THROW((void)LinearSolvePullback_(a, invalidB), Exception_);
    }
    SquareMatrix_<> singular(2);
    Matrix_<> rhs(2, 1);
    ASSERT_THROW((void)LinearSolvePullback_(singular, rhs), Exception_);
    singular(0, 0) = 1.0;
    singular(0, 1) = 2.0;
    singular(1, 0) = 2.0;
    singular(1, 1) = 4.0;
    ASSERT_THROW((void)LinearSolvePullback_(singular, rhs), Exception_);
    SquareMatrix_<> threshold(2);
    threshold(0, 0) = 1.0;
    threshold(1, 1) = 1e-8;
    ASSERT_THROW((void)LinearSolvePullback_(threshold, rhs, 1e-8), Exception_);
    const LinearSolvePullback_ accepted(threshold, rhs, 5e-9);
    ASSERT_DOUBLE_EQ(accepted.ScaledMinimumPivot(), 1e-8);
}

TEST(LinearSolvePullbackTest, TestReverseFailureAndOverflowRecovery) {
    const auto a = PermutedMatrix();
    const auto seed = Seeds();
    const LinearSolvePullback_ solve(a, Multiply(a, KnownSolution()));
    const auto before = solve.Reverse(seed);
    ASSERT_THROW((void)solve.Reverse(Matrix_<>(3, 1)), Exception_);
    for (double value : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()}) {
        auto invalid = seed;
        invalid(0, 1) = value;
        ASSERT_THROW((void)solve.Reverse(invalid), Exception_);
    }
    const auto after = solve.Reverse(seed);
    AssertMatrixNear(before.matrix_, after.matrix_);
    AssertMatrixNear(before.rhs_, after.rhs_);
    ASSERT_THROW((void)LinearSolvePullback_(SquareMatrix_<>(1, 1e-308), Matrix_<>(1, 1, 1e308)), Exception_);
    const LinearSolvePullback_ tiny(SquareMatrix_<>(1, 1e-308), Matrix_<>(1, 1, 1e-308));
    ASSERT_THROW((void)tiny.Reverse(Matrix_<>(1, 1, 1e308)), Exception_);
    ASSERT_NEAR(tiny.Reverse(Matrix_<>(1, 1, 1e-308)).rhs_(0, 0), 1.0, 1e-10);
    const LinearSolvePullback_ large(SquareMatrix_<>(1, 1.0), Matrix_<>(1, 1, 1e308));
    ASSERT_THROW((void)large.Reverse(Matrix_<>(1, 1, 2.0)), Exception_);
    ASSERT_NEAR(large.Reverse(Matrix_<>(1, 1, 1e-308)).matrix_(0, 0), -1.0, 1e-10);
    const LinearSolvePullback_ unrepresentable(SquareMatrix_<>(1, 1e308), Matrix_<>(1, 1, 1e308));
    ASSERT_THROW((void)unrepresentable.Reverse(Matrix_<>(1, 1, 1e-20)), Exception_);
    ASSERT_DOUBLE_EQ(unrepresentable.Reverse(Matrix_<>(1, 1, 1e308)).rhs_(0, 0), 1.0);
}

TEST(LinearSolvePullbackTest, TestConcurrentConstReverse) {
    const auto a = PermutedMatrix();
    const auto seed = Seeds();
    const LinearSolvePullback_ solve(a, Multiply(a, KnownSolution()));
    const auto expected = solve.Reverse(seed);
    std::vector<std::future<LinearSolveAdjoints_>> tasks;
    for (int worker = 0; worker < 4; ++worker)
        tasks.emplace_back(std::async(std::launch::async, [&] { return solve.Reverse(seed); }));
    for (auto& task : tasks) {
        const auto result = task.get();
        AssertMatrixNear(result.matrix_, expected.matrix_);
        AssertMatrixNear(result.rhs_, expected.rhs_);
    }
}

TEST(LinearSolvePullbackTest, TestRepresentableSubnormalSolveAndTransposeAvoidEarlyScalingLoss) {
    SquareMatrix_<> a(2);
    a(0, 0) = 1e308;
    a(1, 1) = 2e294;
    Matrix_<> b(2, 2), seed(2, 2);
    b(0, 0) = 1e308;
    b(1, 0) = 1e-20;
    b(1, 1) = 1e308;
    seed(1, 0) = 1e-20;
    seed(1, 1) = 1e308;
    const LinearSolvePullback_ solve(a, b);
    ASSERT_DOUBLE_EQ(solve.Solution()(0, 0), 1.0);
    const double expected = b(1, 0) / a(1, 1);
    ASSERT_GT(expected, 0.0);
    ASSERT_DOUBLE_EQ(solve.Solution()(1, 0), expected);
    ASSERT_NEAR(solve.Solution()(1, 1) / (b(1, 1) / a(1, 1)), 1.0, 1e-10);
    const auto adjoints = solve.Reverse(seed);
    ASSERT_DOUBLE_EQ(adjoints.rhs_(1, 0), expected);
    ASSERT_NEAR(adjoints.rhs_(1, 1) / (seed(1, 1) / a(1, 1)), 1.0, 1e-10);
    ASSERT_DOUBLE_EQ(adjoints.matrix_(1, 0), -expected);
    b(1, 0) = 1e-15;
    seed(1, 0) = 1e-15;
    const LinearSolvePullback_ rounded(a, b);
    const double normalReference = b(1, 0) / a(1, 1);
    ASSERT_GT(normalReference, 0.0);
    ASSERT_NEAR(rounded.Solution()(1, 0) / normalReference, 1.0, 1e-10);
    ASSERT_NEAR(rounded.Reverse(seed).rhs_(1, 0) / normalReference, 1.0, 1e-10);
}

//
// Created by Codex on 2026/10/7.
//

#include <gtest/gtest.h>

#include <cmath>
#include <future>
#include <limits>
#include <utility>
#include <vector>

#include <dal/math/buffercapacity.hpp>
#include <dal/math/matrix/linearsolvediagnostics.hpp>
#include <dal/utilities/exceptions.hpp>

using Dal::DiagnosedLinearSolve_;
using Dal::Matrix_;
using Dal::SquareMatrix_;

TEST(LinearSolveDiagnosticsTest, TestAnalyticConditionAndMultipleRhs) {
    SquareMatrix_<> matrix(2);
    matrix(0, 0) = 2.0;
    matrix(0, 1) = 1.0;
    matrix(1, 0) = 1.0;
    matrix(1, 1) = 3.0;
    Matrix_<> rhs(2, 2);
    rhs(0, 0) = 1.0;
    rhs(1, 0) = 2.0;
    const DiagnosedLinearSolve_ result(matrix, rhs);
    const auto& diagnostics = result.Diagnostics();
    ASSERT_NEAR(diagnostics.reciprocalConditionInfinity_, 0.3125, 1e-10);
    ASSERT_EQ(diagnostics.componentwiseBackwardErrors_.size(), 2);
    ASSERT_LE(diagnostics.componentwiseBackwardErrors_[0], 1e-15);
    ASSERT_DOUBLE_EQ(diagnostics.componentwiseBackwardErrors_[1], 0.0);
    ASSERT_NEAR(result.Solve().Solution()(0, 0), 0.2, 1e-10);
    ASSERT_NEAR(result.Solve().Solution()(1, 0), 0.6, 1e-10);
}

TEST(LinearSolveDiagnosticsTest, TestProductRoundingResidualIsRetained) {
    const double step = std::ldexp(1.0, -27);
    SquareMatrix_<> matrix(1, 1.0 + step);
    Matrix_<> rhs(1, 1, 1.0), solution(1, 1, 1.0 - step);
    const auto errors = Dal::LinearSolveBackwardErrors(matrix, rhs, solution);
    ASSERT_GT(errors[0], 0.0);
    ASSERT_NEAR(errors[0] / std::ldexp(1.0, -55), 1.0, 1e-10);
}

TEST(LinearSolveDiagnosticsTest, TestNonsymmetricConditionDiffersFromMinimumPivot) {
    SquareMatrix_<> matrix(2);
    matrix(0, 0) = 1.0;
    matrix(0, 1) = 1e8;
    matrix(1, 1) = 1.0;
    const DiagnosedLinearSolve_ result(matrix, Matrix_<>(2, 1));
    const double expected = 1.0 / (1e8 + 1.0) / (1e8 + 1.0);
    ASSERT_NEAR(result.Diagnostics().reciprocalConditionInfinity_ / expected, 1.0, 1e-10);
    ASSERT_DOUBLE_EQ(result.Solve().ScaledMinimumPivot(), 1e-8);
    ASSERT_LT(result.Diagnostics().reciprocalConditionInfinity_, 1e-15);
}

TEST(LinearSolveDiagnosticsTest, TestPermutationAndUniformScaleInvariance) {
    for (double scale : {1e-300, -1.0, 1e300}) {
        SCOPED_TRACE(scale);
        SquareMatrix_<> matrix(2);
        matrix(0, 1) = 2.0 * scale;
        matrix(1, 0) = 4.0 * scale;
        Matrix_<> rhs(2, 2);
        rhs(0, 0) = 2.0 * scale;
        rhs(1, 0) = -4.0 * scale;
        const DiagnosedLinearSolve_ result(matrix, rhs);
        ASSERT_NEAR(result.Diagnostics().reciprocalConditionInfinity_, 0.5, 1e-10);
        ASSERT_DOUBLE_EQ(result.Solve().Solution()(0, 0), -1.0);
        ASSERT_DOUBLE_EQ(result.Solve().Solution()(1, 0), 1.0);
        ASSERT_DOUBLE_EQ(result.Diagnostics().componentwiseBackwardErrors_[0], 0.0);
        ASSERT_DOUBLE_EQ(result.Diagnostics().componentwiseBackwardErrors_[1], 0.0);
    }
}

TEST(LinearSolveDiagnosticsTest, TestConditionAvoidsPhysicalInverseAndNormOverflow) {
    const double tiny = std::numeric_limits<double>::denorm_min();
    const DiagnosedLinearSolve_ subnormal(SquareMatrix_<>(1, tiny), Matrix_<>(1, 1, tiny));
    ASSERT_DOUBLE_EQ(subnormal.Diagnostics().reciprocalConditionInfinity_, 1.0);
    ASSERT_DOUBLE_EQ(subnormal.Diagnostics().componentwiseBackwardErrors_[0], 0.0);
    SquareMatrix_<> matrix(3);
    matrix(0, 0) = 1.0;
    matrix(0, 1) = 1.0;
    matrix(0, 2) = 1.0;
    matrix(1, 1) = 1e-308;
    matrix(2, 2) = 1e-308;
    const DiagnosedLinearSolve_ largeInverse(matrix, Matrix_<>(3, 1), 1e-309);
    ASSERT_GT(largeInverse.Diagnostics().reciprocalConditionInfinity_, 0.0);
    ASSERT_NEAR(largeInverse.Diagnostics().reciprocalConditionInfinity_ / (1e-308 / 6.0), 1.0, 1e-10);
}

TEST(LinearSolveDiagnosticsTest, TestIndependentNonsymmetricAnalyticInverseNorm) {
    SquareMatrix_<> matrix(3);
    const double entries[3][3] = {{0.0, 2.0, 1.0}, {1.0, 0.0, 3.0}, {4.0, 1.0, 0.0}};
    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 3; ++column)
            matrix(row, column) = entries[row][column];
    const DiagnosedLinearSolve_ result(matrix, Matrix_<>(3, 2));
    ASSERT_NEAR(result.Diagnostics().reciprocalConditionInfinity_, 5.0 / 17.0, 1e-10);
}

TEST(LinearSolveDiagnosticsTest, TestDiagnosticInverseRangeFailureLeavesOrdinarySolveAvailable) {
    SquareMatrix_<> matrix(2);
    matrix(0, 0) = 1.0;
    matrix(1, 1) = 1e-310;
    Matrix_<> rhs(2, 1);
    const Dal::LinearSolvePullback_ ordinary(matrix, rhs, 1e-311);
    ASSERT_DOUBLE_EQ(ordinary.Solution()(1, 0), 0.0);
    ASSERT_THROW(static_cast<void>(DiagnosedLinearSolve_(matrix, rhs, 1e-311)), Dal::Exception_);
    ASSERT_DOUBLE_EQ(ordinary.ReverseRhs(rhs)(1, 0), 0.0);
}

TEST(LinearSolveDiagnosticsTest, TestBackwardErrorForPerturbedSolutionAndZeroRows) {
    SquareMatrix_<> matrix(2);
    matrix(0, 0) = 2.0;
    Matrix_<> rhs(2, 3), solution(2, 3);
    rhs(0, 0) = 4.0;
    solution(0, 0) = 1.0;
    rhs(1, 1) = 1.0;
    solution(0, 2) = 1.0;
    const auto errors = Dal::LinearSolveBackwardErrors(matrix, rhs, solution);
    ASSERT_NEAR(errors[0], 1.0 / 3.0, 1e-10);
    ASSERT_DOUBLE_EQ(errors[1], 1.0);
    ASSERT_DOUBLE_EQ(errors[2], 1.0);
}

TEST(LinearSolveDiagnosticsTest, TestBackwardErrorPreservesCancellationAcrossExtremeProducts) {
    for (double value : {std::numeric_limits<double>::denorm_min(), 1e308}) {
        SCOPED_TRACE(value);
        SquareMatrix_<> matrix(3);
        matrix(0, 0) = value;
        matrix(0, 1) = value;
        matrix(0, 2) = -value;
        Matrix_<> rhs(3, 1), solution(3, 1);
        solution(0, 0) = 2.0;
        solution(1, 0) = 1.0;
        solution(2, 0) = 2.0;
        const auto errors = Dal::LinearSolveBackwardErrors(matrix, rhs, solution);
        ASSERT_NEAR(errors[0], 0.2, 1e-10);
        rhs(0, 0) = value;
        ASSERT_DOUBLE_EQ(Dal::LinearSolveBackwardErrors(matrix, rhs, solution)[0], 0.0);
    }
}

TEST(LinearSolveDiagnosticsTest, TestBackwardErrorRetainsSmallTermsAfterCancellation) {
    SquareMatrix_<> matrix(3);
    matrix(0, 0) = 1e16;
    matrix(0, 1) = 1.0;
    matrix(0, 2) = -1e16;
    Matrix_<> rhs(3, 1), solution(3, 1, 1.0);
    const auto errors = Dal::LinearSolveBackwardErrors(matrix, rhs, solution);
    ASSERT_NEAR(errors[0] / 5e-17, 1.0, 1e-10);
    rhs(0, 0) = 1.0;
    ASSERT_DOUBLE_EQ(Dal::LinearSolveBackwardErrors(matrix, rhs, solution)[0], 0.0);
}

TEST(LinearSolveDiagnosticsTest, TestRepresentableMinimumBackwardErrorSurvivesScaling) {
    SquareMatrix_<> matrix(3);
    matrix(0, 0) = std::ldexp(1.0, 1023);
    matrix(0, 1) = std::ldexp(1.0, -49);
    matrix(0, 2) = -matrix(0, 0);
    Matrix_<> rhs(3, 1), solution(3, 1, 1.0);
    solution(0, 0) = solution(2, 0) = 2.0;
    ASSERT_EQ(Dal::LinearSolveBackwardErrors(matrix, rhs, solution)[0], std::numeric_limits<double>::denorm_min());
}

TEST(LinearSolveDiagnosticsTest, TestIndividuallyUnderflowingTermsAccumulateIntoRepresentableError) {
    SquareMatrix_<> matrix(6);
    matrix(0, 0) = std::ldexp(1.0, 1023);
    matrix(0, 5) = -matrix(0, 0);
    for (int column = 1; column < 5; ++column)
        matrix(0, column) = std::ldexp(1.0, -51);
    Matrix_<> rhs(6, 1), solution(6, 1, 1.0);
    solution(0, 0) = solution(5, 0) = 2.0;
    ASSERT_EQ(Dal::LinearSolveBackwardErrors(matrix, rhs, solution)[0], std::numeric_limits<double>::denorm_min());
}

TEST(LinearSolveDiagnosticsTest, TestRejectInvalidShapesAndNonfiniteWitnesses) {
    SquareMatrix_<> matrix(2, 1.0);
    Matrix_<> rhs(2, 1), solution(2, 1);
    ASSERT_THROW(static_cast<void>(Dal::LinearSolveBackwardErrors(SquareMatrix_<>(), Matrix_<>(), Matrix_<>())), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(Dal::LinearSolveBackwardErrors(matrix, Matrix_<>(1, 1), solution)), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(Dal::LinearSolveBackwardErrors(matrix, Matrix_<>(2, 0), Matrix_<>(2, 0))), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(Dal::LinearSolveBackwardErrors(matrix, rhs, Matrix_<>(2, 2))), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(Dal::LinearSolveBackwardErrors(matrix, rhs, Matrix_<>(1, 1))), Dal::Exception_);
    for (double value : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()}) {
        matrix(0, 0) = value;
        ASSERT_THROW(static_cast<void>(Dal::LinearSolveBackwardErrors(matrix, rhs, solution)), Dal::Exception_);
        matrix(0, 0) = 1.0;
        rhs(1, 0) = value;
        ASSERT_THROW(static_cast<void>(Dal::LinearSolveBackwardErrors(matrix, rhs, solution)), Dal::Exception_);
        rhs(1, 0) = 0.0;
        solution(1, 0) = value;
        ASSERT_THROW(static_cast<void>(Dal::LinearSolveBackwardErrors(matrix, rhs, solution)), Dal::Exception_);
        solution(1, 0) = 0.0;
    }
}

TEST(LinearSolveDiagnosticsTest, TestSingularAndPivotPolicyMatchOrdinarySolve) {
    Matrix_<> rhs(2, 1);
    ASSERT_THROW(static_cast<void>(DiagnosedLinearSolve_(SquareMatrix_<>(2), rhs)), Dal::Exception_);
    SquareMatrix_<> matrix(2);
    matrix(0, 0) = 1.0;
    matrix(1, 1) = 1e-8;
    ASSERT_THROW(static_cast<void>(DiagnosedLinearSolve_(matrix, rhs, 1e-8)), Dal::Exception_);
    const DiagnosedLinearSolve_ accepted(matrix, rhs, 5e-9);
    ASSERT_DOUBLE_EQ(accepted.Diagnostics().reciprocalConditionInfinity_, 1e-8);
    for (double tolerance : {0.0, -1.0, 1.0, std::numeric_limits<double>::quiet_NaN()})
        ASSERT_THROW(static_cast<void>(DiagnosedLinearSolve_(matrix, rhs, tolerance)), Dal::Exception_);
}

TEST(LinearSolveDiagnosticsTest, TestOwningDiagnosticsAndPullbackSurviveInputMutation) {
    SquareMatrix_<> matrix(1, 2.0);
    Matrix_<> rhs(1, 1, 6.0), seed(1, 1, 1.0);
    const DiagnosedLinearSolve_ result(matrix, rhs);
    matrix(0, 0) = 0.0;
    rhs(0, 0) = 0.0;
    ASSERT_DOUBLE_EQ(result.Diagnostics().reciprocalConditionInfinity_, 1.0);
    ASSERT_DOUBLE_EQ(result.Diagnostics().componentwiseBackwardErrors_[0], 0.0);
    for (int repeat = 0; repeat < 2; ++repeat) {
        const auto adjoints = result.Solve().Reverse(seed);
        ASSERT_DOUBLE_EQ(adjoints.matrix_(0, 0), -1.5);
        ASSERT_DOUBLE_EQ(adjoints.rhs_(0, 0), 0.5);
    }
}

TEST(LinearSolveDiagnosticsTest, TestTrackedScratchRefundsAfterCapacityFailure) {
    SquareMatrix_<> matrix(2);
    matrix(0, 0) = matrix(1, 1) = 1.0;
    Matrix_<> rhs(2, 1);
    Dal::BufferCapacityBudget_ measured(4096);
    size_t peak;
    {
        Dal::BufferCapacityScope_ scope(&measured);
        {
            const DiagnosedLinearSolve_ result(matrix, rhs);
        }
        ASSERT_EQ(measured.CapacityBytes(), 0);
        peak = measured.PeakCapacityBytes();
    }
    ASSERT_GT(peak, 0);
    for (size_t limit : {peak, peak - 1}) {
        Dal::BufferCapacityBudget_ budget(limit);
        Dal::BufferCapacityScope_ scope(&budget);
        if (limit == peak) {
            const DiagnosedLinearSolve_ result(matrix, rhs);
            ASSERT_DOUBLE_EQ(result.Diagnostics().reciprocalConditionInfinity_, 1.0);
        } else
            ASSERT_THROW(static_cast<void>(DiagnosedLinearSolve_(matrix, rhs)), Dal::Exception_);
        ASSERT_EQ(budget.CapacityBytes(), 0);
    }
}

TEST(LinearSolveDiagnosticsTest, TestConcurrentConstDiagnosticsAndReverse) {
    SquareMatrix_<> matrix(2);
    matrix(0, 0) = 2.0;
    matrix(0, 1) = matrix(1, 0) = 1.0;
    matrix(1, 1) = 3.0;
    Matrix_<> rhs(2, 1), seed(2, 1);
    rhs(0, 0) = seed(0, 0) = 1.0;
    rhs(1, 0) = 2.0;
    const DiagnosedLinearSolve_ result(matrix, rhs);
    std::vector<std::future<std::pair<double, Dal::LinearSolveAdjoints_>>> tasks;
    for (int worker = 0; worker < 4; ++worker)
        tasks.emplace_back(std::async(
            std::launch::async, [&] { return std::make_pair(result.Diagnostics().reciprocalConditionInfinity_, result.Solve().Reverse(seed)); }));
    for (auto& task : tasks) {
        const auto values = task.get();
        ASSERT_NEAR(values.first, 0.3125, 1e-10);
        ASSERT_NEAR(values.second.rhs_(0, 0), 0.6, 1e-10);
        ASSERT_NEAR(values.second.rhs_(1, 0), -0.2, 1e-10);
        ASSERT_NEAR(values.second.matrix_(0, 0), -0.12, 1e-10);
    }
}

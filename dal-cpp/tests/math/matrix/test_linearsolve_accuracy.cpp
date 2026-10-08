//
// Created by Codex on 2026/10/8.
//

#include <gtest/gtest.h>

#include <cmath>
#include <future>
#include <limits>
#include <string>
#include <vector>

#include <dal/math/buffercapacity.hpp>
#include <dal/math/matrix/linearsolveaccuracy.hpp>
#include <dal/utilities/exceptions.hpp>

using Dal::CheckedLinearSolve_;
using Dal::LinearSolveAccuracyPolicy_;
using Dal::Matrix_;
using Dal::SquareMatrix_;

namespace {
    struct ResourceFixture_ {
        SquareMatrix_<> matrix_{4};
        Matrix_<> rhs_{4, 2}, seeds_{4, 2};

        ResourceFixture_() {
            for (int row = 0; row < 4; ++row) {
                matrix_(row, row) = std::ldexp(1.0, row);
                rhs_(row, 0) = matrix_(row, row);
                rhs_(row, 1) = -2.0 * matrix_(row, row);
                seeds_(row, 0) = 1.0;
                seeds_(row, 1) = -1.0;
            }
        }
    };

    void CheckResourceRisk(const Dal::CheckedLinearSolveAdjoints_& risk, bool full) {
        ASSERT_EQ(risk.adjoints_.rhs_.Rows(), 4);
        ASSERT_EQ(risk.adjoints_.rhs_.Cols(), 2);
        ASSERT_EQ(risk.adjoints_.rhs_(3, 0), 0.125);
        ASSERT_EQ(risk.adjoints_.rhs_(3, 1), -0.125);
        ASSERT_EQ(risk.transposeBackwardErrors_.size(), 2);
        ASSERT_EQ(risk.transposeBackwardErrors_[0], 0.0);
        ASSERT_EQ(risk.transposeBackwardErrors_[1], 0.0);
        ASSERT_EQ(risk.adjoints_.matrix_.Rows(), full ? 4 : 0);
        if (full) {
            ASSERT_EQ(risk.adjoints_.matrix_(0, 3), -3.0);
            ASSERT_EQ(risk.adjoints_.matrix_(3, 3), -0.375);
        }
    }

    void CheckInvalidPolicy(double value, bool forward) {
        SquareMatrix_<> matrix(1, 2.0);
        Matrix_<> rhs(1, 1, 3.0);
        LinearSolveAccuracyPolicy_ policy{1e-14, 1e-14};
        if (forward)
            policy.forwardBackwardErrorLimit_ = value;
        else
            policy.transposeBackwardErrorLimit_ = value;
        Dal::BufferCapacityBudget_ budget(0);
        Dal::BufferCapacityScope_ capacity(&budget);
        try {
            static_cast<void>(CheckedLinearSolve_(matrix, rhs, policy));
            FAIL() << "Invalid accuracy policy was accepted";
        } catch (const Dal::Exception_& error) {
            const std::string message(error.what());
            ASSERT_NE(message.find(forward ? "forward backward-error limit" : "transpose backward-error limit"), std::string::npos);
        }
        ASSERT_EQ(budget.CapacityBytes(), 0);
        ASSERT_EQ(budget.PeakCapacityBytes(), 0);
    }
} // namespace

TEST(LinearSolveDiagnosticsTest, TestCheckedAnalyticNonsymmetricMultipleRhs) {
    SquareMatrix_<> matrix(2);
    matrix(0, 0) = 2.0;
    matrix(0, 1) = 1.0;
    matrix(1, 0) = 3.0;
    matrix(1, 1) = 4.0;
    Matrix_<> rhs(2, 2), seeds(2, 2);
    rhs(0, 0) = 1.0;
    rhs(1, 0) = 2.0;
    rhs(0, 1) = -3.0;
    rhs(1, 1) = 1.0;
    seeds(0, 0) = 2.0;
    seeds(1, 0) = -1.0;
    const CheckedLinearSolve_ solve(matrix, rhs, LinearSolveAccuracyPolicy_{1e-14, 1e-14});
    ASSERT_NEAR(solve.Solution()(0, 0), 0.4, 1e-10);
    ASSERT_NEAR(solve.Solution()(1, 0), 0.2, 1e-10);
    ASSERT_NEAR(solve.Solution()(0, 1), -2.6, 1e-10);
    ASSERT_NEAR(solve.Solution()(1, 1), 2.2, 1e-10);
    ASSERT_NEAR(solve.Diagnostics().reciprocalConditionInfinity_, 1.0 / 7.0, 1e-10);
    ASSERT_EQ(solve.Diagnostics().componentwiseBackwardErrors_.size(), 2);
    const auto risk = solve.Reverse(seeds);
    ASSERT_NEAR(risk.adjoints_.rhs_(0, 0), 2.2, 1e-10);
    ASSERT_NEAR(risk.adjoints_.rhs_(1, 0), -0.8, 1e-10);
    ASSERT_NEAR(risk.adjoints_.matrix_(0, 0), -0.88, 1e-10);
    ASSERT_NEAR(risk.adjoints_.matrix_(0, 1), -0.44, 1e-10);
    ASSERT_NEAR(risk.adjoints_.matrix_(1, 0), 0.32, 1e-10);
    ASSERT_NEAR(risk.adjoints_.matrix_(1, 1), 0.16, 1e-10);
    ASSERT_EQ(risk.transposeBackwardErrors_.size(), 2);
    ASSERT_LE(risk.transposeBackwardErrors_[0], solve.Policy().transposeBackwardErrorLimit_);
    ASSERT_DOUBLE_EQ(risk.transposeBackwardErrors_[1], 0.0);
    const auto rhsRisk = solve.ReverseRhs(seeds);
    ASSERT_EQ(rhsRisk.adjoints_.matrix_.Rows(), 0);
    ASSERT_NEAR(rhsRisk.adjoints_.rhs_(0, 0), 2.2, 1e-10);
    ASSERT_NEAR(rhsRisk.adjoints_.rhs_(1, 0), -0.8, 1e-10);
    ASSERT_DOUBLE_EQ(rhsRisk.adjoints_.rhs_(0, 1), 0.0);
    ASSERT_DOUBLE_EQ(rhsRisk.adjoints_.rhs_(1, 1), 0.0);
    ASSERT_EQ(rhsRisk.transposeBackwardErrors_, risk.transposeBackwardErrors_);
}

TEST(LinearSolveDiagnosticsTest, TestCheckedForwardRoundingPolicyInclusiveBoundary) {
    const double step = std::ldexp(1.0, -27), expectedError = std::ldexp(1.0, -55);
    SquareMatrix_<> matrix(1, 1.0 + step);
    Matrix_<> rhs(1, 1, 1.0);
    LinearSolveAccuracyPolicy_ policy{expectedError, expectedError};
    const CheckedLinearSolve_ solve(matrix, rhs, policy);
    ASSERT_EQ(solve.Solution()(0, 0), 1.0 - step);
    ASSERT_EQ(solve.Diagnostics().componentwiseBackwardErrors_[0], expectedError);
    policy.forwardBackwardErrorLimit_ = 0.0;
    policy.transposeBackwardErrorLimit_ = 0.0;
    ASSERT_EQ(solve.Policy().forwardBackwardErrorLimit_, expectedError);
    ASSERT_EQ(solve.Policy().transposeBackwardErrorLimit_, expectedError);
    const auto risk = solve.ReverseRhs(rhs);
    ASSERT_EQ(risk.adjoints_.rhs_(0, 0), 1.0 - step);
    ASSERT_EQ(risk.transposeBackwardErrors_[0], expectedError);
    const LinearSolveAccuracyPolicy_ below{std::nextafter(expectedError, 0.0), expectedError};
    ASSERT_THROW(static_cast<void>(CheckedLinearSolve_(matrix, rhs, below)), Dal::Exception_);
}

TEST(LinearSolveDiagnosticsTest, TestCheckedTransposeRoundingPolicyAndFailureRecovery) {
    const double step = std::ldexp(1.0, -27), expectedError = std::ldexp(1.0, -55);
    SquareMatrix_<> matrix(1, 1.0 + step);
    Matrix_<> rhs(1, 1, 0.0), seed(1, 1, 1.0);
    const CheckedLinearSolve_ rejected(matrix, rhs, {0.0, std::nextafter(expectedError, 0.0)});
    ASSERT_EQ(rejected.Diagnostics().componentwiseBackwardErrors_[0], 0.0);
    ASSERT_THROW(static_cast<void>(rejected.ReverseRhs(seed)), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(rejected.Reverse(seed)), Dal::Exception_);
    seed(0, 0) = 0.0;
    const auto recovered = rejected.Reverse(seed);
    ASSERT_EQ(recovered.adjoints_.rhs_(0, 0), 0.0);
    ASSERT_EQ(recovered.adjoints_.matrix_(0, 0), 0.0);
    ASSERT_EQ(recovered.transposeBackwardErrors_[0], 0.0);
    const CheckedLinearSolve_ accepted(matrix, rhs, {0.0, expectedError});
    for (double sign : {1.0, -1.0}) {
        seed(0, 0) = sign;
        const auto risk = accepted.ReverseRhs(seed);
        ASSERT_EQ(risk.adjoints_.rhs_(0, 0), sign * (1.0 - step));
        ASSERT_EQ(risk.transposeBackwardErrors_[0], expectedError);
    }
}

TEST(LinearSolveDiagnosticsTest, TestCheckedInvalidPoliciesRejectBeforeAllocation) {
    const double nan = std::numeric_limits<double>::quiet_NaN(), infinity = std::numeric_limits<double>::infinity();
    for (double value : {-1.0, std::nextafter(1.0, infinity), nan, infinity, -infinity})
        for (bool forward : {false, true})
            ASSERT_NO_FATAL_FAILURE(CheckInvalidPolicy(value, forward));
    const CheckedLinearSolve_ permissive(SquareMatrix_<>(1, 2.0), Matrix_<>(1, 1, 3.0), {1.0, 1.0});
    ASSERT_EQ(permissive.Solution()(0, 0), 1.5);
}

TEST(LinearSolveDiagnosticsTest, TestCheckedInvalidInputsAndSeedsPreserveNumericRecovery) {
    const LinearSolveAccuracyPolicy_ policy{0.0, 0.0};
    SquareMatrix_<> matrix(1, 2.0);
    Matrix_<> rhs(1, 1, 3.0), seed(1, 1, 1.0);
    ASSERT_THROW(static_cast<void>(CheckedLinearSolve_(SquareMatrix_<>(), rhs, policy)), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(CheckedLinearSolve_(SquareMatrix_<>(1, 0.0), rhs, policy)), Dal::Exception_);
    for (const auto& bad : {Matrix_<>(0, 1), Matrix_<>(2, 1), Matrix_<>(1, 0)})
        ASSERT_THROW(static_cast<void>(CheckedLinearSolve_(matrix, bad, policy)), Dal::Exception_);
    for (double tolerance : {0.0, -1.0, 1.0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()})
        ASSERT_THROW(static_cast<void>(CheckedLinearSolve_(matrix, rhs, policy, tolerance)), Dal::Exception_);
    const CheckedLinearSolve_ solve(matrix, rhs, policy);
    for (const auto& bad : {Matrix_<>(0, 1), Matrix_<>(2, 1), Matrix_<>(1, 2), Matrix_<>(1, 0)}) {
        ASSERT_THROW(static_cast<void>(solve.Reverse(bad)), Dal::Exception_);
        ASSERT_THROW(static_cast<void>(solve.ReverseRhs(bad)), Dal::Exception_);
    }
    for (double value : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()}) {
        seed(0, 0) = value;
        ASSERT_THROW(static_cast<void>(solve.Reverse(seed)), Dal::Exception_);
        ASSERT_THROW(static_cast<void>(solve.ReverseRhs(seed)), Dal::Exception_);
    }
    seed(0, 0) = -2.0;
    const auto recovered = solve.Reverse(seed);
    ASSERT_EQ(recovered.adjoints_.rhs_(0, 0), -1.0);
    ASSERT_EQ(recovered.adjoints_.matrix_(0, 0), 1.5);
    ASSERT_EQ(recovered.transposeBackwardErrors_[0], 0.0);
}

TEST(LinearSolveDiagnosticsTest, TestCheckedIllConditionedSystemPreservesLargeFiniteRisk) {
    const double epsilon = std::ldexp(1.0, -54);
    SquareMatrix_<> matrix(2);
    matrix(0, 0) = 1.0;
    matrix(1, 1) = epsilon;
    Matrix_<> rhs(2, 1), seed(2, 1);
    rhs(0, 0) = 1.0;
    rhs(1, 0) = 3.0;
    seed(1, 0) = 1.0;
    ASSERT_THROW(static_cast<void>(CheckedLinearSolve_(matrix, rhs, {0.0, 0.0})), Dal::Exception_);
    const CheckedLinearSolve_ solve(matrix, rhs, {0.0, 0.0}, std::ldexp(1.0, -60));
    const auto risk = solve.Reverse(seed);
    ASSERT_EQ(solve.Solution()(1, 0), 3.0 / epsilon);
    ASSERT_EQ(solve.Diagnostics().reciprocalConditionInfinity_, epsilon);
    ASSERT_EQ(solve.Diagnostics().componentwiseBackwardErrors_[0], 0.0);
    ASSERT_EQ(risk.adjoints_.rhs_(1, 0), 1.0 / epsilon);
    ASSERT_EQ(risk.adjoints_.matrix_(1, 1), -3.0 / epsilon / epsilon);
    ASSERT_EQ(risk.transposeBackwardErrors_[0], 0.0);
}

TEST(LinearSolveDiagnosticsTest, TestCheckedRhsOnlyAvoidsUnusedMatrixRiskOverflow) {
    const CheckedLinearSolve_ solve(SquareMatrix_<>(1, 1e-150), Matrix_<>(1, 1, 1e150), {1e-14, 1e-14});
    Matrix_<> seed(1, 1, 1.0);
    ASSERT_THROW(static_cast<void>(solve.Reverse(seed)), Dal::Exception_);
    const auto risk = solve.ReverseRhs(seed);
    ASSERT_EQ(risk.adjoints_.matrix_.Rows(), 0);
    ASSERT_NEAR(risk.adjoints_.rhs_(0, 0) / 1e150, 1.0, 1e-10);
    ASSERT_LE(risk.transposeBackwardErrors_[0], 1e-14);
}

TEST(LinearSolveDiagnosticsTest, TestCheckedOwningSnapshotsReportsAndConcurrentReverse) {
    Dal::CheckedLinearSolveAdjoints_ detached;
    {
        SquareMatrix_<> matrix(2);
        matrix(0, 0) = 2.0;
        matrix(0, 1) = 1.0;
        matrix(1, 1) = 4.0;
        Matrix_<> rhs(2, 1), seed(2, 1);
        rhs(0, 0) = 5.0;
        rhs(1, 0) = 8.0;
        seed(0, 0) = 3.0;
        seed(1, 0) = -1.0;
        const CheckedLinearSolve_ solve(matrix, rhs, {0.0, 0.0});
        matrix(0, 0) = 99.0;
        matrix(1, 0) = 7.0;
        rhs.Resize(0, 0);
        std::vector<std::future<Dal::CheckedLinearSolveAdjoints_>> futures;
        for (double weight : {1.0, -2.0, 0.0, 3.0})
            futures.push_back(std::async(std::launch::async, [&solve, seed, weight] {
                auto local = seed;
                for (int row = 0; row < local.Rows(); ++row)
                    local(row, 0) *= weight;
                return solve.Reverse(local);
            }));
        const double weights[4] = {1.0, -2.0, 0.0, 3.0};
        for (size_t index = 0; index < futures.size(); ++index) {
            const auto risk = futures[index].get();
            ASSERT_EQ(risk.adjoints_.rhs_(0, 0), 1.5 * weights[index]);
            ASSERT_EQ(risk.adjoints_.rhs_(1, 0), -0.625 * weights[index]);
            ASSERT_EQ(risk.adjoints_.matrix_(0, 0), -2.25 * weights[index]);
            ASSERT_EQ(risk.adjoints_.matrix_(0, 1), -3.0 * weights[index]);
            ASSERT_EQ(risk.adjoints_.matrix_(1, 0), 0.9375 * weights[index]);
            ASSERT_EQ(risk.adjoints_.matrix_(1, 1), 1.25 * weights[index]);
            ASSERT_EQ(risk.transposeBackwardErrors_[0], 0.0);
        }
        detached = solve.ReverseRhs(seed);
        seed(0, 0) = 999.0;
        ASSERT_EQ(solve.Solution()(0, 0), 1.5);
        ASSERT_EQ(solve.Solution()(1, 0), 2.0);
    }
    ASSERT_EQ(detached.adjoints_.rhs_(0, 0), 1.5);
    ASSERT_EQ(detached.adjoints_.rhs_(1, 0), -0.625);
    ASSERT_EQ(detached.transposeBackwardErrors_[0], 0.0);
}

TEST(LinearSolveDiagnosticsTest, TestCheckedExactConstructionPeakAndTransposeRetention) {
    const ResourceFixture_ fixture;
    const size_t retained = (2 * 16 + 8 + 2) * sizeof(double) + 4 * sizeof(int);
    const size_t peak = retained + 2 * 16 * sizeof(double);
    size_t legacyRetained = 0;
    {
        Dal::BufferCapacityBudget_ budget(4096);
        Dal::BufferCapacityScope_ capacity(&budget);
        {
            const Dal::DiagnosedLinearSolve_ legacy(fixture.matrix_, fixture.rhs_);
            legacyRetained = budget.CapacityBytes();
        }
        ASSERT_EQ(budget.CapacityBytes(), 0);
    }
    for (size_t limit : {peak, peak - 1}) {
        Dal::BufferCapacityBudget_ budget(limit);
        Dal::BufferCapacityScope_ capacity(&budget);
        if (limit == peak) {
            const CheckedLinearSolve_ solve(fixture.matrix_, fixture.rhs_, {0.0, 0.0});
            ASSERT_EQ(budget.CapacityBytes(), retained);
            ASSERT_EQ(budget.CapacityBytes() - legacyRetained, 16 * sizeof(double));
            ASSERT_EQ(budget.PeakCapacityBytes(), peak);
            ASSERT_EQ(solve.Solution()(3, 1), -2.0);
        } else
            ASSERT_THROW(static_cast<void>(CheckedLinearSolve_(fixture.matrix_, fixture.rhs_, {0.0, 0.0})), Dal::Exception_);
        ASSERT_EQ(budget.CapacityBytes(), 0);
    }
}

TEST(LinearSolveDiagnosticsTest, TestCheckedExactReverseBudgetsAndOneByteShortRefund) {
    const ResourceFixture_ fixture;
    const CheckedLinearSolve_ solve(fixture.matrix_, fixture.rhs_, {0.0, 0.0});
    for (bool full : {false, true}) {
        const size_t exact = (8 + 2 + (full ? 16 : 0)) * sizeof(double);
        for (size_t limit : {exact, exact - 1}) {
            Dal::BufferCapacityBudget_ budget(limit);
            Dal::BufferCapacityScope_ capacity(&budget);
            if (limit == exact) {
                const auto risk = full ? solve.Reverse(fixture.seeds_) : solve.ReverseRhs(fixture.seeds_);
                ASSERT_NO_FATAL_FAILURE(CheckResourceRisk(risk, full));
                ASSERT_EQ(budget.CapacityBytes(), exact);
                ASSERT_EQ(budget.PeakCapacityBytes(), exact);
            } else
                ASSERT_THROW(static_cast<void>(full ? solve.Reverse(fixture.seeds_) : solve.ReverseRhs(fixture.seeds_)), Dal::Exception_);
            ASSERT_EQ(budget.CapacityBytes(), 0);
        }
        ASSERT_NO_FATAL_FAILURE(CheckResourceRisk(full ? solve.Reverse(fixture.seeds_) : solve.ReverseRhs(fixture.seeds_), full));
    }
}

TEST(LinearSolveDiagnosticsTest, TestCheckedPolicyFailureRefundsUnpublishedReverseOutputs) {
    const double error = std::ldexp(1.0, -55);
    const CheckedLinearSolve_ solve(SquareMatrix_<>(1, 1.0 + std::ldexp(1.0, -27)), Matrix_<>(1, 1), {0.0, std::nextafter(error, 0.0)});
    Matrix_<> seed(1, 1, 1.0);
    for (bool full : {false, true}) {
        Dal::BufferCapacityBudget_ budget(1024);
        Dal::BufferCapacityScope_ capacity(&budget);
        seed(0, 0) = 1.0;
        ASSERT_THROW(static_cast<void>(full ? solve.Reverse(seed) : solve.ReverseRhs(seed)), Dal::Exception_);
        ASSERT_EQ(budget.CapacityBytes(), 0);
        seed(0, 0) = 0.0;
        {
            const auto recovered = full ? solve.Reverse(seed) : solve.ReverseRhs(seed);
            ASSERT_EQ(recovered.adjoints_.rhs_(0, 0), 0.0);
            ASSERT_EQ(recovered.transposeBackwardErrors_[0], 0.0);
        }
        ASSERT_EQ(budget.CapacityBytes(), 0);
    }
}

TEST(LinearSolveDiagnosticsTest, TestCheckedNonfiniteConstructionRefundsAndRecovers) {
    for (double bad : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()}) {
        SquareMatrix_<> matrix(1, 2.0);
        Matrix_<> rhs(1, 1, 3.0);
        Dal::BufferCapacityBudget_ budget(4096);
        Dal::BufferCapacityScope_ capacity(&budget);
        matrix(0, 0) = bad;
        ASSERT_THROW(static_cast<void>(CheckedLinearSolve_(matrix, rhs, {1e-14, 1e-14})), Dal::Exception_);
        ASSERT_EQ(budget.CapacityBytes(), 0);
        matrix(0, 0) = 2.0;
        rhs(0, 0) = bad;
        ASSERT_THROW(static_cast<void>(CheckedLinearSolve_(matrix, rhs, {1e-14, 1e-14})), Dal::Exception_);
        ASSERT_EQ(budget.CapacityBytes(), 0);
        rhs(0, 0) = 3.0;
        {
            const CheckedLinearSolve_ recovered(matrix, rhs, {0.0, 0.0});
            ASSERT_EQ(recovered.Solution()(0, 0), 1.5);
        }
        ASSERT_EQ(budget.CapacityBytes(), 0);
    }
}

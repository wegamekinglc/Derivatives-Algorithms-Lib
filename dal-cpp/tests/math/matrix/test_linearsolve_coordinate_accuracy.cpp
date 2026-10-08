//
// Created by Codex on 2026/10/8.
//

#include <gtest/gtest.h>

#include <cmath>
#include <future>
#include <limits>
#include <vector>

#include <dal/math/buffercapacity.hpp>
#include <dal/math/matrix/linearsolvecoordinateaccuracy.hpp>
#include <dal/utilities/exceptions.hpp>

using Dal::CheckedCoordinateLinearSolve_;
using Dal::LinearSolveCoordinates_;
using Dal::Matrix_;
using Dal::Vector_;

namespace {
    double SymmetricObjective(const Vector_<>& parameters, const Matrix_<>& rhs, const Matrix_<>& seed) {
        const double determinant = parameters[0] * parameters[2] - parameters[1] * parameters[1];
        double result = 0.0;
        for (int column = 0; column < rhs.Cols(); ++column) {
            const double x = (parameters[2] * rhs(0, column) - parameters[1] * rhs(1, column)) / determinant;
            const double y = (parameters[0] * rhs(1, column) - parameters[1] * rhs(0, column)) / determinant;
            result += seed(0, column) * x + seed(1, column) * y;
        }
        return result;
    }

    void CheckCoordinateDifferences(
        const Vector_<>& parameters, const Matrix_<>& rhs, const Matrix_<>& seed, const Vector_<>& risk, double step, double tolerance) {
        for (size_t index = 0; index < parameters.size(); ++index) {
            auto plus = parameters, minus = parameters;
            plus[index] += step;
            minus[index] -= step;
            const double difference = (SymmetricObjective(plus, rhs, seed) - SymmetricObjective(minus, rhs, seed)) / (2.0 * step);
            ASSERT_NEAR(risk[index], difference, tolerance);
        }
    }

    void CheckRhsDifferences(const Vector_<>& parameters, const Matrix_<>& rhs, const Matrix_<>& seed, const Matrix_<>& risk) {
        const double step = 1e-5;
        for (int row = 0; row < rhs.Rows(); ++row)
            for (int column = 0; column < rhs.Cols(); ++column) {
                auto plus = rhs, minus = rhs;
                plus(row, column) += step;
                minus(row, column) -= step;
                const double difference = (SymmetricObjective(parameters, plus, seed) - SymmetricObjective(parameters, minus, seed)) / (2.0 * step);
                ASSERT_NEAR(risk(row, column), difference, 1e-9);
            }
    }

    struct ResourceFixture_ {
        LinearSolveCoordinates_ coordinates_ = LinearSolveCoordinates_::Banded(4, 0, 0);
        Vector_<> parameters_{1.0, 2.0, 4.0, 8.0};
        Matrix_<> rhs_{4, 2}, seeds_{4, 2};

        ResourceFixture_() {
            for (int row = 0; row < 4; ++row) {
                rhs_(row, 0) = parameters_[row];
                rhs_(row, 1) = -2.0 * parameters_[row];
                seeds_(row, 0) = 1.0;
                seeds_(row, 1) = -1.0;
            }
        }
    };

    void CheckResourceRisk(const Dal::CheckedCoordinateLinearSolveAdjoints_& risk, bool full) {
        ASSERT_EQ(risk.adjoints_.rhs_.Rows(), 4);
        ASSERT_EQ(risk.adjoints_.rhs_.Cols(), 2);
        ASSERT_EQ(risk.adjoints_.rhs_(3, 0), 0.125);
        ASSERT_EQ(risk.adjoints_.rhs_(3, 1), -0.125);
        ASSERT_EQ(risk.transposeBackwardErrors_.size(), 2);
        ASSERT_EQ(risk.transposeBackwardErrors_[0], 0.0);
        ASSERT_EQ(risk.transposeBackwardErrors_[1], 0.0);
        ASSERT_EQ(risk.adjoints_.coordinates_.size(), full ? 4 : 0);
        if (full) {
            ASSERT_EQ(risk.adjoints_.coordinates_[0], -3.0);
            ASSERT_EQ(risk.adjoints_.coordinates_[3], -0.375);
        }
    }
} // namespace

TEST(LinearSolvePullbackTest, TestCheckedSymmetricPhysicalParameterRisk) {
    const auto coordinates = LinearSolveCoordinates_::Symmetric(2);
    const Vector_<> parameters{3.0, 1.0, 2.0};
    Matrix_<> rhs(2, 1), seeds(2, 1);
    rhs(0, 0) = 1.0;
    rhs(1, 0) = 2.0;
    seeds(0, 0) = 2.0;
    seeds(1, 0) = -1.0;
    const CheckedCoordinateLinearSolve_ solve(coordinates, parameters, rhs, {1e-14, 1e-14});
    ASSERT_EQ(solve.Coordinates().Count(), 3);
    ASSERT_NEAR(solve.Solution()(0, 0), 0.0, 1e-10);
    ASSERT_NEAR(solve.Solution()(1, 0), 1.0, 1e-10);
    const auto risk = solve.Reverse(seeds);
    ASSERT_EQ(risk.adjoints_.coordinates_.size(), 3);
    ASSERT_NEAR(risk.adjoints_.coordinates_[0], 0.0, 1e-10);
    ASSERT_NEAR(risk.adjoints_.coordinates_[1], -1.0, 1e-10);
    ASSERT_NEAR(risk.adjoints_.coordinates_[2], 1.0, 1e-10);
    ASSERT_NEAR(risk.adjoints_.rhs_(0, 0), 1.0, 1e-10);
    ASSERT_NEAR(risk.adjoints_.rhs_(1, 0), -1.0, 1e-10);
    ASSERT_EQ(risk.transposeBackwardErrors_.size(), 1);
    ASSERT_LE(risk.transposeBackwardErrors_[0], solve.Policy().transposeBackwardErrorLimit_);
    const auto rhsRisk = solve.ReverseRhs(seeds);
    ASSERT_TRUE(rhsRisk.adjoints_.coordinates_.empty());
    ASSERT_NEAR(rhsRisk.adjoints_.rhs_(0, 0), 1.0, 1e-10);
    ASSERT_NEAR(rhsRisk.adjoints_.rhs_(1, 0), -1.0, 1e-10);
    ASSERT_EQ(rhsRisk.transposeBackwardErrors_, risk.transposeBackwardErrors_);
}

TEST(LinearSolvePullbackTest, TestCheckedZeroSymmetricParameterRemainsActive) {
    Matrix_<> rhs(2, 1), seed(2, 1);
    rhs(0, 0) = seed(0, 0) = 3.0;
    rhs(1, 0) = seed(1, 0) = 2.0;
    const CheckedCoordinateLinearSolve_ solve(LinearSolveCoordinates_::Symmetric(2), {3.0, 0.0, 2.0}, rhs, {0.0, 0.0});
    const auto risk = solve.Reverse(seed);
    ASSERT_EQ(risk.adjoints_.coordinates_[0], -1.0);
    ASSERT_EQ(risk.adjoints_.coordinates_[1], -2.0);
    ASSERT_EQ(risk.adjoints_.coordinates_[2], -1.0);
    ASSERT_EQ(risk.transposeBackwardErrors_[0], 0.0);
}

TEST(LinearSolvePullbackTest, TestCheckedBandPhysicalAndRhsChainContributions) {
    Matrix_<> rhs(2, 1), seed(2, 1);
    rhs(0, 0) = 1.0;
    rhs(1, 0) = 8.0;
    seed(0, 0) = 3.0;
    seed(1, 0) = -1.0;
    const auto layout = LinearSolveCoordinates_::Banded(2, 0, 1);
    const CheckedCoordinateLinearSolve_ solve(layout, {2.0, 1.0, 4.0}, rhs, {0.0, 0.0});
    ASSERT_EQ(solve.Solution()(0, 0), -0.5);
    ASSERT_EQ(solve.Solution()(1, 0), 2.0);
    const auto risk = solve.Reverse(seed);
    ASSERT_EQ(risk.adjoints_.coordinates_.size(), 3);
    ASSERT_EQ(risk.adjoints_.coordinates_[1], -3.0);
    ASSERT_EQ(risk.adjoints_.rhs_(0, 0), 1.5);
    ASSERT_EQ(risk.adjoints_.coordinates_[1] + risk.adjoints_.rhs_(0, 0), -1.5);
    ASSERT_EQ(risk.transposeBackwardErrors_[0], 0.0);
}

TEST(LinearSolvePullbackTest, TestCheckedCoordinateIndependentCramerDifferencesAndZeroColumn) {
    const Vector_<> parameters{3.0, 0.3, 2.0};
    Matrix_<> rhs(2, 3), seed(2, 3);
    rhs(0, 0) = 0.7;
    rhs(1, 0) = 1.2;
    rhs(0, 1) = -1.1;
    rhs(1, 1) = 0.4;
    rhs(0, 2) = 2.0;
    rhs(1, 2) = -3.0;
    seed(0, 0) = 2.0;
    seed(1, 0) = -1.0;
    seed(0, 1) = -0.5;
    seed(1, 1) = 3.0;
    const CheckedCoordinateLinearSolve_ solve(LinearSolveCoordinates_::Symmetric(2), parameters, rhs, {1e-14, 1e-14});
    const auto risk = solve.Reverse(seed);
    ASSERT_NO_FATAL_FAILURE(CheckCoordinateDifferences(parameters, rhs, seed, risk.adjoints_.coordinates_, 1e-3, 2e-6));
    ASSERT_NO_FATAL_FAILURE(CheckCoordinateDifferences(parameters, rhs, seed, risk.adjoints_.coordinates_, 1e-4, 2e-8));
    ASSERT_NO_FATAL_FAILURE(CheckCoordinateDifferences(parameters, rhs, seed, risk.adjoints_.coordinates_, 1e-5, 2e-9));
    ASSERT_NO_FATAL_FAILURE(CheckRhsDifferences(parameters, rhs, seed, risk.adjoints_.rhs_));
    ASSERT_EQ(risk.transposeBackwardErrors_.size(), 3);
    ASSERT_EQ(risk.transposeBackwardErrors_[2], 0.0);
    ASSERT_EQ(risk.adjoints_.rhs_(0, 2), 0.0);
    ASSERT_EQ(risk.adjoints_.rhs_(1, 2), 0.0);
}

TEST(LinearSolvePullbackTest, TestCheckedCoordinateRoundingLimitsAndRecovery) {
    const double delta = std::ldexp(1.0, -27), error = std::ldexp(1.0, -55);
    const auto layout = LinearSolveCoordinates_::Banded(1, 0, 0);
    Matrix_<> rhs(1, 1, 1.0), seed(1, 1, 1.0);
    const CheckedCoordinateLinearSolve_ accepted(layout, {1.0 + delta}, rhs, {error, error});
    ASSERT_EQ(accepted.Solution()(0, 0), 1.0 - delta);
    ASSERT_EQ(accepted.Diagnostics().componentwiseBackwardErrors_[0], error);
    const auto risk = accepted.ReverseRhs(seed);
    ASSERT_EQ(risk.adjoints_.rhs_(0, 0), 1.0 - delta);
    ASSERT_EQ(risk.transposeBackwardErrors_[0], error);
    ASSERT_THROW(static_cast<void>(CheckedCoordinateLinearSolve_(layout, {1.0 + delta}, rhs, {std::nextafter(error, 0.0), error})), Dal::Exception_);
    rhs(0, 0) = 0.0;
    const CheckedCoordinateLinearSolve_ rejected(layout, {1.0 + delta}, rhs, {0.0, std::nextafter(error, 0.0)});
    Dal::BufferCapacityBudget_ budget(1024);
    Dal::BufferCapacityScope_ capacity(&budget);
    ASSERT_THROW(static_cast<void>(rejected.Reverse(seed)), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(rejected.ReverseRhs(seed)), Dal::Exception_);
    ASSERT_EQ(budget.CapacityBytes(), 0);
    seed(0, 0) = 0.0;
    {
        const auto recovered = rejected.Reverse(seed);
        ASSERT_EQ(recovered.adjoints_.coordinates_[0], 0.0);
        ASSERT_EQ(recovered.transposeBackwardErrors_[0], 0.0);
    }
    ASSERT_EQ(budget.CapacityBytes(), 0);
}

TEST(LinearSolvePullbackTest, TestCheckedPairedCoordinateOverflowAndRhsOnlyRecovery) {
    Matrix_<> rhs(2, 1, 1.0), seed(2, 1, 0.75 * std::numeric_limits<double>::max());
    const CheckedCoordinateLinearSolve_ solve(LinearSolveCoordinates_::Symmetric(2), {1.0, 0.0, 1.0}, rhs, {0.0, 0.0});
    Dal::BufferCapacityBudget_ budget(1024);
    Dal::BufferCapacityScope_ capacity(&budget);
    ASSERT_THROW(static_cast<void>(solve.Reverse(seed)), Dal::Exception_);
    ASSERT_EQ(budget.CapacityBytes(), 0);
    {
        const auto risk = solve.ReverseRhs(seed);
        ASSERT_TRUE(risk.adjoints_.coordinates_.empty());
        ASSERT_EQ(risk.adjoints_.rhs_(0, 0), seed(0, 0));
        ASSERT_EQ(risk.adjoints_.rhs_(1, 0), seed(1, 0));
        ASSERT_EQ(risk.transposeBackwardErrors_[0], 0.0);
    }
    ASSERT_EQ(budget.CapacityBytes(), 0);
}

TEST(LinearSolvePullbackTest, TestCheckedCoordinateRhsOnlyOmitsUnusedOverflow) {
    const CheckedCoordinateLinearSolve_ solve(LinearSolveCoordinates_::Banded(1, 0, 0), {1e-150}, Matrix_<>(1, 1, 1e150), {1e-14, 1e-14});
    const Matrix_<> seed(1, 1, 1.0);
    ASSERT_THROW(static_cast<void>(solve.Reverse(seed)), Dal::Exception_);
    const auto risk = solve.ReverseRhs(seed);
    ASSERT_TRUE(risk.adjoints_.coordinates_.empty());
    ASSERT_NEAR(risk.adjoints_.rhs_(0, 0) / 1e150, 1.0, 1e-10);
    ASSERT_LE(risk.transposeBackwardErrors_[0], 1e-14);
}

TEST(LinearSolvePullbackTest, TestCheckedCoordinateConstructionHasOneCacheAndExpansion) {
    const ResourceFixture_ fixture;
    const size_t retained = (2 * 16 + 8 + 2) * sizeof(double) + 4 * sizeof(int);
    const size_t peak = retained + 3 * 16 * sizeof(double);
    for (size_t limit : {peak, peak - 1}) {
        Dal::BufferCapacityBudget_ budget(limit);
        Dal::BufferCapacityScope_ capacity(&budget);
        if (limit == peak) {
            const CheckedCoordinateLinearSolve_ solve(fixture.coordinates_, fixture.parameters_, fixture.rhs_, {0.0, 0.0});
            ASSERT_EQ(budget.CapacityBytes(), retained);
            ASSERT_EQ(budget.PeakCapacityBytes(), peak);
            ASSERT_EQ(solve.Solution()(3, 1), -2.0);
        } else
            ASSERT_THROW(static_cast<void>(CheckedCoordinateLinearSolve_(fixture.coordinates_, fixture.parameters_, fixture.rhs_, {0.0, 0.0})),
                         Dal::Exception_);
        ASSERT_EQ(budget.CapacityBytes(), 0);
    }
}

TEST(LinearSolvePullbackTest, TestCheckedCoordinateReverseAllocatesOnlyPackedRisks) {
    const ResourceFixture_ fixture;
    const CheckedCoordinateLinearSolve_ solve(fixture.coordinates_, fixture.parameters_, fixture.rhs_, {0.0, 0.0});
    for (bool full : {false, true}) {
        const size_t exact = (8 + 2 + (full ? 4 : 0)) * sizeof(double);
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
    }
}

TEST(LinearSolvePullbackTest, TestCheckedCoordinateOwnsInputsPolicyAndConcurrentResults) {
    Dal::CheckedCoordinateLinearSolveAdjoints_ detached;
    {
        Vector_<> parameters{2.0, 1.0, 4.0};
        Matrix_<> rhs(2, 1), seed(2, 1);
        rhs(0, 0) = 5.0;
        rhs(1, 0) = 8.0;
        seed(0, 0) = 3.0;
        seed(1, 0) = -1.0;
        Dal::LinearSolveAccuracyPolicy_ policy{0.0, 0.0};
        const CheckedCoordinateLinearSolve_ solve(LinearSolveCoordinates_::Banded(2, 0, 1), parameters, rhs, policy);
        parameters.clear();
        rhs.Resize(0, 0);
        policy.transposeBackwardErrorLimit_ = -1.0;
        ASSERT_EQ(solve.Policy().transposeBackwardErrorLimit_, 0.0);
        std::vector<std::future<Dal::CheckedCoordinateLinearSolveAdjoints_>> futures;
        const double weights[] = {1.0, -2.0, 0.0, 3.0};
        for (double weight : weights)
            futures.push_back(std::async(std::launch::async, [&solve, seed, weight] {
                auto local = seed;
                for (int row = 0; row < local.Rows(); ++row)
                    local(row, 0) *= weight;
                return solve.Reverse(local);
            }));
        for (size_t index = 0; index < futures.size(); ++index) {
            const auto risk = futures[index].get();
            ASSERT_EQ(risk.adjoints_.coordinates_[0], -2.25 * weights[index]);
            ASSERT_EQ(risk.adjoints_.coordinates_[1], -3.0 * weights[index]);
            ASSERT_EQ(risk.adjoints_.coordinates_[2], 1.25 * weights[index]);
            ASSERT_EQ(risk.adjoints_.rhs_(0, 0), 1.5 * weights[index]);
            ASSERT_EQ(risk.adjoints_.rhs_(1, 0), -0.625 * weights[index]);
            ASSERT_EQ(risk.transposeBackwardErrors_[0], 0.0);
        }
        detached = solve.Reverse(seed);
    }
    ASSERT_EQ(detached.adjoints_.coordinates_[1], -3.0);
    ASSERT_EQ(detached.adjoints_.rhs_(1, 0), -0.625);
    ASSERT_EQ(detached.transposeBackwardErrors_[0], 0.0);
}

TEST(LinearSolvePullbackTest, TestCheckedCoordinateInvalidCaptureRefundsAndRecovers) {
    const auto layout = LinearSolveCoordinates_::Banded(1, 0, 0);
    Matrix_<> rhs(1, 1, 3.0);
    Vector_<> parameters{2.0};
    Dal::BufferCapacityBudget_ budget(4096);
    Dal::BufferCapacityScope_ capacity(&budget);
    for (double bad : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()}) {
        parameters[0] = bad;
        ASSERT_THROW(static_cast<void>(CheckedCoordinateLinearSolve_(layout, parameters, rhs, {1e-14, 1e-14})), Dal::Exception_);
        ASSERT_EQ(budget.CapacityBytes(), 0);
        parameters[0] = 2.0;
        rhs(0, 0) = bad;
        ASSERT_THROW(static_cast<void>(CheckedCoordinateLinearSolve_(layout, parameters, rhs, {1e-14, 1e-14})), Dal::Exception_);
        ASSERT_EQ(budget.CapacityBytes(), 0);
        rhs(0, 0) = 3.0;
    }
    {
        const CheckedCoordinateLinearSolve_ recovered(layout, parameters, rhs, {0.0, 0.0});
        ASSERT_EQ(recovered.Solution()(0, 0), 1.5);
    }
    ASSERT_EQ(budget.CapacityBytes(), 0);
}

TEST(LinearSolvePullbackTest, TestCheckedCoordinateInvalidShapesPoliciesPivotsAndSeeds) {
    const auto layout = LinearSolveCoordinates_::Banded(1, 0, 0);
    const Vector_<> parameters{2.0};
    Matrix_<> rhs(1, 1, 3.0);
    ASSERT_THROW(static_cast<void>(CheckedCoordinateLinearSolve_(layout, Vector_<>(), rhs, {0.0, 0.0})), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(CheckedCoordinateLinearSolve_(layout, {0.0}, rhs, {0.0, 0.0})), Dal::Exception_);
    for (const auto& bad : {Matrix_<>(0, 1), Matrix_<>(2, 1), Matrix_<>(1, 0)})
        ASSERT_THROW(static_cast<void>(CheckedCoordinateLinearSolve_(layout, parameters, bad, {0.0, 0.0})), Dal::Exception_);
    for (double bad : {-1.0, std::nextafter(1.0, 2.0), std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()}) {
        ASSERT_THROW(static_cast<void>(CheckedCoordinateLinearSolve_(layout, parameters, rhs, {bad, 0.0})), Dal::Exception_);
        ASSERT_THROW(static_cast<void>(CheckedCoordinateLinearSolve_(layout, parameters, rhs, {0.0, bad})), Dal::Exception_);
    }
    for (double bad : {0.0, -1.0, 1.0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()})
        ASSERT_THROW(static_cast<void>(CheckedCoordinateLinearSolve_(layout, parameters, rhs, {0.0, 0.0}, bad)), Dal::Exception_);
    const CheckedCoordinateLinearSolve_ solve(layout, parameters, rhs, {0.0, 0.0});
    for (const auto& bad : {Matrix_<>(0, 1), Matrix_<>(2, 1), Matrix_<>(1, 2), Matrix_<>(1, 0)}) {
        ASSERT_THROW(static_cast<void>(solve.Reverse(bad)), Dal::Exception_);
        ASSERT_THROW(static_cast<void>(solve.ReverseRhs(bad)), Dal::Exception_);
    }
    Matrix_<> seed(1, 1);
    for (double bad : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()}) {
        seed(0, 0) = bad;
        ASSERT_THROW(static_cast<void>(solve.Reverse(seed)), Dal::Exception_);
        ASSERT_THROW(static_cast<void>(solve.ReverseRhs(seed)), Dal::Exception_);
    }
    seed(0, 0) = -2.0;
    const auto recovered = solve.Reverse(seed);
    ASSERT_EQ(recovered.adjoints_.coordinates_[0], 1.5);
    ASSERT_EQ(recovered.adjoints_.rhs_(0, 0), -1.0);
}

TEST(LinearSolvePullbackTest, TestCheckedCoordinateIllConditionedLegalPivotPreservesLargeRisk) {
    const auto layout = LinearSolveCoordinates_::Banded(2, 0, 0);
    const double epsilon = std::ldexp(1.0, -54);
    const Vector_<> parameters{1.0, epsilon};
    Matrix_<> rhs(2, 1), seed(2, 1);
    rhs(0, 0) = 1.0;
    rhs(1, 0) = 3.0;
    seed(1, 0) = 1.0;
    ASSERT_THROW(static_cast<void>(CheckedCoordinateLinearSolve_(layout, parameters, rhs, {0.0, 0.0})), Dal::Exception_);
    const CheckedCoordinateLinearSolve_ solve(layout, parameters, rhs, {0.0, 0.0}, std::ldexp(1.0, -60));
    ASSERT_EQ(solve.Diagnostics().reciprocalConditionInfinity_, epsilon);
    const auto risk = solve.Reverse(seed);
    ASSERT_EQ(risk.adjoints_.coordinates_[1], -3.0 / epsilon / epsilon);
    ASSERT_EQ(risk.adjoints_.rhs_(1, 0), 1.0 / epsilon);
    ASSERT_EQ(risk.transposeBackwardErrors_[0], 0.0);
}

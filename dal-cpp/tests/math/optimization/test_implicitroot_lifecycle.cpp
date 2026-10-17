//
// Created by Codex on 2026/10/8.
//

#include <gtest/gtest.h>

#include <cmath>
#include <future>
#include <limits>
#include <memory>

#include <dal/math/buffercapacity.hpp>
#include <dal/math/optimization/implicitroot.hpp>
#include <dal/platform/platform.hpp>

using Dal::ImplicitRootAccuracyPolicy_;
using Dal::ImplicitRootEquation_;
using Dal::ImplicitRootEvaluation_;
using Dal::ImplicitRootLinearization_;
using Dal::Matrix_;
using Dal::SquareMatrix_;
using Dal::Vector_;

namespace {
    class AffineEquation_ final : public ImplicitRootEquation_ {
    public:
        double slope_, inputSlope_;
        AffineEquation_(double slope, double inputSlope) : slope_(slope), inputSlope_(inputSlope) {}
        [[nodiscard]] ImplicitRootEvaluation_ Evaluate(const Vector_<>& parameters, const Vector_<>& inputs) const override {
            double residual = slope_ * parameters[0];
            for (double input : inputs)
                residual += inputSlope_ * input;
            return {Vector_<>{residual}, SquareMatrix_<>(1, slope_), Matrix_<>(1, static_cast<int>(inputs.size()), inputSlope_)};
        }
    };

    class BranchEquation_ final : public ImplicitRootEquation_ {
    public:
        [[nodiscard]] ImplicitRootEvaluation_ Evaluate(const Vector_<>& parameters, const Vector_<>& inputs) const override {
            return {Vector_<>{parameters[0] * parameters[0] - inputs[0]}, SquareMatrix_<>(1, 2.0 * parameters[0]), Matrix_<>(1, 1, -1.0)};
        }
    };

    class DiagonalEquation_ final : public ImplicitRootEquation_ {
        double epsilon_;

    public:
        explicit DiagonalEquation_(double epsilon = std::ldexp(1.0, -54)) : epsilon_(epsilon) {}
        [[nodiscard]] ImplicitRootEvaluation_ Evaluate(const Vector_<>& parameters, const Vector_<>& inputs) const override {
            ImplicitRootEvaluation_ result{Vector_<>{parameters[0], epsilon_ * parameters[1] - inputs[0]}, SquareMatrix_<>(2, 0.0),
                                           Matrix_<>(2, 1, 0.0)};
            result.parameterJacobian_(0, 0) = 1.0;
            result.parameterJacobian_(1, 1) = epsilon_;
            result.inputJacobian_(1, 0) = -1.0;
            return result;
        }
    };

    class BadEvaluation_ final : public ImplicitRootEquation_ {
        int problem_;

    public:
        explicit BadEvaluation_(int problem) : problem_(problem) {}
        [[nodiscard]] ImplicitRootEvaluation_ Evaluate(const Vector_<>&, const Vector_<>&) const override {
            ImplicitRootEvaluation_ result{Vector_<>{0.0}, SquareMatrix_<>(1, 1.0), Matrix_<>(1, 1, -1.0)};
            if (problem_ == 0)
                result.residuals_.clear();
            if (problem_ == 1)
                result.parameterJacobian_ = SquareMatrix_<>(2, 1.0);
            if (problem_ == 2)
                result.inputJacobian_.Resize(2, 1);
            if (problem_ == 3)
                result.inputJacobian_.Resize(1, 0);
            if (problem_ == 4)
                result.residuals_[0] = std::numeric_limits<double>::infinity();
            if (problem_ == 5)
                result.parameterJacobian_(0, 0) = std::numeric_limits<double>::quiet_NaN();
            if (problem_ == 6)
                result.inputJacobian_(0, 0) = std::numeric_limits<double>::infinity();
            return result;
        }
    };
} // namespace

TEST(ImplicitRootTest, TestNegativeBranchAndApproximateCandidateIdentity) {
    const BranchEquation_ equation;
    for (double candidate : {2.0, -2.0}) {
        const ImplicitRootLinearization_ root(equation, Vector_<>{candidate}, Vector_<>{4.0}, ImplicitRootAccuracyPolicy_{Vector_<>{0.0}, 0.0});
        ASSERT_DOUBLE_EQ(root.Reverse(Matrix_<>(1, 1, 1.0)).inputs_(0, 0), 1.0 / (2.0 * candidate));
    }
    ASSERT_THROW(
        static_cast<void>(ImplicitRootLinearization_(equation, Vector_<>{1.9}, Vector_<>{4.0}, ImplicitRootAccuracyPolicy_{Vector_<>{1e-12}, 1e-12})),
        Dal::Exception_);
    const ImplicitRootLinearization_ approximate(equation, Vector_<>{1.9}, Vector_<>{4.0}, ImplicitRootAccuracyPolicy_{Vector_<>{0.4}, 1e-12});
    ASSERT_NEAR(approximate.Residuals()[0], -0.39, 1e-10);
    ASSERT_NEAR(approximate.Reverse(Matrix_<>(1, 1, 1.0)).inputs_(0, 0), 1.0 / 3.8, 1e-10);
}

TEST(ImplicitRootTest, TestPerEquationResidualLimitIsInclusive) {
    const AffineEquation_ equation(1.0, -1.0);
    const double error = std::ldexp(1.0, -30);
    const ImplicitRootLinearization_ accepted(equation, Vector_<>{error}, Vector_<>{0.0}, ImplicitRootAccuracyPolicy_{Vector_<>{error}, 0.0});
    ASSERT_DOUBLE_EQ(accepted.Residuals()[0], error);
    ASSERT_DOUBLE_EQ(accepted.Reverse(Matrix_<>(1, 1, 1.0)).inputs_(0, 0), 1.0);
    ASSERT_THROW(static_cast<void>(ImplicitRootLinearization_(equation, Vector_<>{error}, Vector_<>{0.0},
                                                              ImplicitRootAccuracyPolicy_{Vector_<>{std::nextafter(error, 0.0)}, 0.0})),
                 Dal::Exception_);
}

TEST(ImplicitRootTest, TestActualTransposeErrorAndFailedReverseRecovery) {
    const double delta = std::ldexp(1.0, -27), error = std::ldexp(1.0, -55);
    const AffineEquation_ equation(1.0 + delta, -1.0);
    const ImplicitRootLinearization_ accepted(equation, Vector_<>{0.0}, Vector_<>{0.0}, ImplicitRootAccuracyPolicy_{Vector_<>{0.0}, error});
    ASSERT_DOUBLE_EQ(accepted.Reverse(Matrix_<>(1, 1, 1.0)).transposeBackwardErrors_[0], error);
    const ImplicitRootLinearization_ limited(equation, Vector_<>{0.0}, Vector_<>{0.0},
                                             ImplicitRootAccuracyPolicy_{Vector_<>{0.0}, std::nextafter(error, 0.0)});
    ASSERT_THROW(static_cast<void>(limited.Reverse(Matrix_<>(1, 1, 1.0))), Dal::Exception_);
    ASSERT_DOUBLE_EQ(limited.Reverse(Matrix_<>(1, 1, 0.0)).transposeBackwardErrors_[0], 0.0);
}

TEST(ImplicitRootTest, TestSourceAndEquationDestructionPreservesRepeatedReverse) {
    std::unique_ptr<ImplicitRootLinearization_> root;
    {
        AffineEquation_ equation(2.0, -1.0);
        Vector_<> parameters{2.0}, inputs{4.0};
        root = std::make_unique<ImplicitRootLinearization_>(equation, parameters, inputs, ImplicitRootAccuracyPolicy_{Vector_<>{0.0}, 0.0});
        equation.slope_ = 0.0;
        equation.inputSlope_ = std::numeric_limits<double>::infinity();
        parameters.clear();
        inputs.clear();
        ASSERT_DOUBLE_EQ(root->Reverse(Matrix_<>(1, 1, 1.0)).inputs_(0, 0), 0.5);
    }
    for (double weight : {1.0, -2.0, 0.0})
        ASSERT_DOUBLE_EQ(root->Reverse(Matrix_<>(1, 1, weight)).inputs_(0, 0), 0.5 * weight);
}

TEST(ImplicitRootTest, TestConcurrentConstReadersUseIndependentResults) {
    const AffineEquation_ equation(2.0, -1.0);
    const ImplicitRootLinearization_ root(equation, Vector_<>{2.0}, Vector_<>{4.0}, ImplicitRootAccuracyPolicy_{Vector_<>{0.0}, 0.0});
    auto first = std::async(std::launch::async, [&root] { return root.Reverse(Matrix_<>(1, 2, 1.0)); });
    auto second = std::async(std::launch::async, [&root] { return root.Reverse(Matrix_<>(1, 2, -2.0)); });
    const auto one = first.get(), two = second.get();
    for (int column = 0; column < 2; ++column) {
        ASSERT_DOUBLE_EQ(one.inputs_(0, column), 0.5);
        ASSERT_DOUBLE_EQ(two.inputs_(0, column), -1.0);
        ASSERT_DOUBLE_EQ(one.transposeBackwardErrors_[column], 0.0);
        ASSERT_DOUBLE_EQ(two.transposeBackwardErrors_[column], 0.0);
    }
}

TEST(ImplicitRootTest, TestZeroInputsKeepsRequestedReportColumns) {
    const AffineEquation_ equation(1.0, -1.0);
    const ImplicitRootLinearization_ root(equation, Vector_<>{0.0}, Vector_<>{}, ImplicitRootAccuracyPolicy_{Vector_<>{0.0}, 0.0});
    Matrix_<> seeds(1, 3);
    seeds(0, 0) = 1.0;
    seeds(0, 1) = -2.0;
    seeds(0, 2) = 0.0;
    const auto reverse = root.Reverse(seeds);
    ASSERT_EQ(reverse.inputs_.Rows(), 0);
    ASSERT_EQ(reverse.inputs_.Cols(), 3);
    ASSERT_EQ(reverse.transposeBackwardErrors_.size(), 3);
    for (double error : reverse.transposeBackwardErrors_)
        ASSERT_DOUBLE_EQ(error, 0.0);
}

TEST(ImplicitRootTest, TestInvalidEvaluationShapesAndValuesReject) {
    for (int problem = 0; problem < 7; ++problem) {
        SCOPED_TRACE(problem);
        const BadEvaluation_ equation(problem);
        ASSERT_THROW(
            static_cast<void>(ImplicitRootLinearization_(equation, Vector_<>{0.0}, Vector_<>{0.0}, ImplicitRootAccuracyPolicy_{Vector_<>{0.0}, 0.0})),
            Dal::Exception_);
    }
}

TEST(ImplicitRootTest, TestInvalidCandidatesPoliciesAndPivotsReject) {
    const AffineEquation_ equation(1.0, -1.0);
    const Vector_<> candidate{0.0}, inputs{0.0};
    ASSERT_THROW(static_cast<void>(ImplicitRootLinearization_(equation, Vector_<>{}, inputs, ImplicitRootAccuracyPolicy_{Vector_<>{}, 0.0})),
                 Dal::Exception_);
    ASSERT_THROW(static_cast<void>(ImplicitRootLinearization_(equation, candidate, inputs, ImplicitRootAccuracyPolicy_{Vector_<>{}, 0.0})),
                 Dal::Exception_);
    const double nan = std::numeric_limits<double>::quiet_NaN(), infinity = std::numeric_limits<double>::infinity();
    for (double invalid : {nan, infinity, -infinity}) {
        ASSERT_THROW(
            static_cast<void>(ImplicitRootLinearization_(equation, Vector_<>{invalid}, inputs, ImplicitRootAccuracyPolicy_{Vector_<>{0.0}, 0.0})),
            Dal::Exception_);
        ASSERT_THROW(
            static_cast<void>(ImplicitRootLinearization_(equation, candidate, Vector_<>{invalid}, ImplicitRootAccuracyPolicy_{Vector_<>{0.0}, 0.0})),
            Dal::Exception_);
    }
    for (double invalid : {-1.0, nan, infinity})
        ASSERT_THROW(static_cast<void>(ImplicitRootLinearization_(equation, candidate, inputs, ImplicitRootAccuracyPolicy_{Vector_<>{invalid}, 0.0})),
                     Dal::Exception_);
    for (double invalid : {-1.0, std::nextafter(1.0, 2.0), nan, infinity})
        ASSERT_THROW(static_cast<void>(ImplicitRootLinearization_(equation, candidate, inputs, ImplicitRootAccuracyPolicy_{Vector_<>{0.0}, invalid})),
                     Dal::Exception_);
    for (double invalid : {0.0, 1.0, -1.0, nan, infinity})
        ASSERT_THROW(
            static_cast<void>(ImplicitRootLinearization_(equation, candidate, inputs, ImplicitRootAccuracyPolicy_{Vector_<>{0.0}, 0.0}, invalid)),
            Dal::Exception_);
}

TEST(ImplicitRootTest, TestConvergedSingularAndUnsupportedInverseRangeReject) {
    const AffineEquation_ singular(0.0, -1.0);
    ASSERT_THROW(
        static_cast<void>(ImplicitRootLinearization_(singular, Vector_<>{0.0}, Vector_<>{0.0}, ImplicitRootAccuracyPolicy_{Vector_<>{0.0}, 1.0})),
        Dal::Exception_);
    const DiagonalEquation_ inverseOverflow(1e-310);
    ASSERT_THROW(static_cast<void>(ImplicitRootLinearization_(inverseOverflow, Vector_<>{0.0, 0.0}, Vector_<>{0.0},
                                                              ImplicitRootAccuracyPolicy_{Vector_<>{0.0, 0.0}, 1.0}, 1e-312)),
                 Dal::Exception_);
    const AffineEquation_ scaled(1e-310, -1.0);
    const ImplicitRootLinearization_ root(scaled, Vector_<>{0.0}, Vector_<>{0.0}, ImplicitRootAccuracyPolicy_{Vector_<>{0.0}, 0.0});
    ASSERT_DOUBLE_EQ(root.ReciprocalJacobianConditionInfinity(), 1.0);
    ASSERT_THROW(static_cast<void>(root.Reverse(Matrix_<>(1, 1, 1.0))), Dal::Exception_);
    ASSERT_DOUBLE_EQ(root.Reverse(Matrix_<>(1, 1, 0.0)).inputs_(0, 0), 0.0);
}

TEST(ImplicitRootTest, TestInvalidSeedsRejectWithoutPoisoningCache) {
    const AffineEquation_ equation(1.0, -1.0);
    const ImplicitRootLinearization_ root(equation, Vector_<>{0.0}, Vector_<>{0.0}, ImplicitRootAccuracyPolicy_{Vector_<>{0.0}, 0.0});
    ASSERT_THROW(static_cast<void>(root.Reverse(Matrix_<>(2, 1, 1.0))), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(root.Reverse(Matrix_<>(1, 0))), Dal::Exception_);
    for (double value : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()})
        ASSERT_THROW(static_cast<void>(root.Reverse(Matrix_<>(1, 1, value))), Dal::Exception_);
    ASSERT_DOUBLE_EQ(root.Reverse(Matrix_<>(1, 1, 1.0)).inputs_(0, 0), 1.0);
}

TEST(ImplicitRootTest, TestLargeFiniteRiskAndRequestedOverflowRecovery) {
    const double epsilon = std::ldexp(1.0, -54);
    const DiagonalEquation_ tiny;
    ASSERT_THROW(static_cast<void>(
                     ImplicitRootLinearization_(tiny, Vector_<>{0.0, 0.0}, Vector_<>{0.0}, ImplicitRootAccuracyPolicy_{Vector_<>{0.0, 0.0}, 0.0})),
                 Dal::Exception_);
    const ImplicitRootLinearization_ large(tiny, Vector_<>{0.0, 0.0}, Vector_<>{0.0}, ImplicitRootAccuracyPolicy_{Vector_<>{0.0, 0.0}, 0.0},
                                           std::ldexp(1.0, -60));
    Matrix_<> seed(2, 1, 0.0);
    seed(1, 0) = 1.0;
    ASSERT_DOUBLE_EQ(large.Reverse(seed).inputs_(0, 0), 1.0 / epsilon);
    ASSERT_DOUBLE_EQ(large.ReciprocalJacobianConditionInfinity(), epsilon);
    const AffineEquation_ overflow(0.5, std::numeric_limits<double>::max());
    const ImplicitRootLinearization_ limited(overflow, Vector_<>{0.0}, Vector_<>{0.0}, ImplicitRootAccuracyPolicy_{Vector_<>{0.0}, 0.0});
    ASSERT_THROW(static_cast<void>(limited.Reverse(Matrix_<>(1, 1, 1.0))), Dal::Exception_);
    ASSERT_DOUBLE_EQ(limited.Reverse(Matrix_<>(1, 1, 0.0)).inputs_(0, 0), 0.0);
}

TEST(ImplicitRootTest, TestExactCaptureCapacityAndOneByteShortRefund) {
    const AffineEquation_ equation(1.0, -1.0);
    const Vector_<> parameters{2.0}, inputs{2.0};
    const ImplicitRootAccuracyPolicy_ policy{Vector_<>{0.0}, 0.0};
    Dal::BufferCapacityBudget_ exact(108);
    {
        Dal::BufferCapacityScope_ caller(&exact);
        const ImplicitRootLinearization_ root(equation, parameters, inputs, policy);
        ASSERT_EQ(exact.CapacityBytes(), 76);
        ASSERT_EQ(exact.PeakCapacityBytes(), 108);
    }
    ASSERT_EQ(exact.CapacityBytes(), 0);
    Dal::BufferCapacityBudget_ shortBudget(107);
    {
        Dal::BufferCapacityScope_ caller(&shortBudget);
        ASSERT_THROW(static_cast<void>(ImplicitRootLinearization_(equation, parameters, inputs, policy)), Dal::Exception_);
        ASSERT_EQ(shortBudget.CapacityBytes(), 0);
    }
}

TEST(ImplicitRootTest, TestExactReverseCapacityAndOneByteShortRefund) {
    const AffineEquation_ equation(1.0, -1.0);
    const ImplicitRootLinearization_ root(equation, Vector_<>{2.0}, Vector_<>{2.0}, ImplicitRootAccuracyPolicy_{Vector_<>{0.0}, 0.0});
    const Matrix_<> seeds(1, 3, 1.0);
    Dal::BufferCapacityBudget_ exact(72);
    {
        Dal::BufferCapacityScope_ caller(&exact);
        const auto reverse = root.Reverse(seeds);
        ASSERT_EQ(exact.CapacityBytes(), 48);
        ASSERT_EQ(exact.PeakCapacityBytes(), 72);
    }
    ASSERT_EQ(exact.CapacityBytes(), 0);
    for (size_t limit : {47U, 71U}) {
        Dal::BufferCapacityBudget_ shortBudget(limit);
        Dal::BufferCapacityScope_ caller(&shortBudget);
        ASSERT_THROW(static_cast<void>(root.Reverse(seeds)), Dal::Exception_);
        ASSERT_EQ(shortBudget.CapacityBytes(), 0);
    }
    ASSERT_DOUBLE_EQ(root.Reverse(Matrix_<>(1, 1, 1.0)).inputs_(0, 0), 1.0);
}

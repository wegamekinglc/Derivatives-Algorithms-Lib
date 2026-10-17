//
// Created by Codex on 2026/10/8.
//

#include <gtest/gtest.h>

#include <cmath>

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
    class CoupledEquation_ final : public ImplicitRootEquation_ {
        double firstCross_, secondCross_;

    public:
        CoupledEquation_(double firstCross, double secondCross) : firstCross_(firstCross), secondCross_(secondCross) {}
        [[nodiscard]] ImplicitRootEvaluation_ Evaluate(const Vector_<>& parameters, const Vector_<>& inputs) const override {
            const double x = parameters[0], y = parameters[1];
            ImplicitRootEvaluation_ result{Vector_<>{x * x + firstCross_ * y - inputs[0], secondCross_ * x + y * y - inputs[1]}, SquareMatrix_<>(2),
                                           Matrix_<>(2, 2, 0.0)};
            result.parameterJacobian_(0, 0) = 2.0 * x;
            result.parameterJacobian_(0, 1) = firstCross_;
            result.parameterJacobian_(1, 0) = secondCross_;
            result.parameterJacobian_(1, 1) = 2.0 * y;
            result.inputJacobian_(0, 0) = -1.0;
            result.inputJacobian_(1, 1) = -1.0;
            return result;
        }
    };

    class StationaryEquation_ final : public ImplicitRootEquation_ {
    public:
        [[nodiscard]] ImplicitRootEvaluation_ Evaluate(const Vector_<>& parameters, const Vector_<>& inputs) const override {
            const double theta = parameters[0];
            ImplicitRootEvaluation_ result{Vector_<>{2.0 * theta * (theta * theta - inputs[0]) + theta - inputs[1]},
                                           SquareMatrix_<>(1, 6.0 * theta * theta - 2.0 * inputs[0] + 1.0), Matrix_<>(1, 2)};
            result.inputJacobian_(0, 0) = -2.0 * theta;
            result.inputJacobian_(0, 1) = -1.0;
            return result;
        }
    };

    double CompleteCoupledRoot(double firstCross, double secondCross, const Vector_<>& candidate, const Vector_<>& inputs) {
        double x = candidate[0], y = candidate[1];
        for (int iteration = 0; iteration < 20; ++iteration) {
            const double first = x * x + firstCross * y - inputs[0], second = secondCross * x + y * y - inputs[1];
            const double determinant = 4.0 * x * y - firstCross * secondCross;
            const double dx = (2.0 * y * first - firstCross * second) / determinant;
            const double dy = (-secondCross * first + 2.0 * x * second) / determinant;
            x -= dx;
            y -= dy;
        }
        REQUIRE(std::abs(x * x + firstCross * y - inputs[0]) < 1e-13 && std::abs(secondCross * x + y * y - inputs[1]) < 1e-13,
                "Independent complete root must converge on the selected branch");
        return x - 2.0 * y;
    }

    double CompleteStationaryRoot(const Vector_<>& inputs) {
        double theta = 1.0;
        for (int iteration = 0; iteration < 20; ++iteration) {
            const double residual = 2.0 * theta * (theta * theta - inputs[0]) + theta - inputs[1];
            theta -= residual / (6.0 * theta * theta - 2.0 * inputs[0] + 1.0);
        }
        REQUIRE(std::abs(2.0 * theta * (theta * theta - inputs[0]) + theta - inputs[1]) < 1e-13,
                "Independent stationary reference must converge with the complete Jacobian");
        return theta;
    }

    void CoupledDifferences(double firstCross, double secondCross, const Vector_<>& candidate, const Vector_<>& inputs) {
        const CoupledEquation_ equation(firstCross, secondCross);
        const ImplicitRootLinearization_ root(equation, candidate, inputs, ImplicitRootAccuracyPolicy_{Vector_<>{0.0, 0.0}, 1e-12});
        Matrix_<> seeds(2, 1);
        seeds(0, 0) = 1.0;
        seeds(1, 0) = -2.0;
        const auto reverse = root.Reverse(seeds);
        const double steps[] = {1e-3, 1e-4, 1e-5}, tolerances[] = {8e-6, 8e-8, 1e-9};
        for (int input = 0; input < 2; ++input)
            for (int step = 0; step < 3; ++step) {
                auto plus = inputs, minus = inputs;
                plus[input] += steps[step];
                minus[input] -= steps[step];
                const double reference =
                    (CompleteCoupledRoot(firstCross, secondCross, candidate, plus) - CompleteCoupledRoot(firstCross, secondCross, candidate, minus)) /
                    (2.0 * steps[step]);
                ASSERT_NEAR(reverse.inputs_(input, 0), reference, tolerances[step]);
            }
    }
} // namespace

TEST(ImplicitRootTest, TestIndependentCoupledAnalyticMultipleSeeds) {
    const CoupledEquation_ equation(1.0, 1.0);
    const ImplicitRootLinearization_ root(equation, Vector_<>{2.0, 3.0}, Vector_<>{7.0, 11.0},
                                          ImplicitRootAccuracyPolicy_{Vector_<>{0.0, 0.0}, 1e-12});
    Matrix_<> seeds(2, 3, 0.0);
    seeds(0, 0) = 1.0;
    seeds(1, 0) = -2.0;
    seeds(0, 1) = -2.0;
    seeds(1, 1) = 3.0;
    const auto reverse = root.Reverse(seeds);
    ASSERT_NEAR(reverse.inputs_(0, 0), 8.0 / 23.0, 1e-10);
    ASSERT_NEAR(reverse.inputs_(1, 0), -9.0 / 23.0, 1e-10);
    ASSERT_NEAR(reverse.inputs_(0, 1), -15.0 / 23.0, 1e-10);
    ASSERT_NEAR(reverse.inputs_(1, 1), 14.0 / 23.0, 1e-10);
    ASSERT_DOUBLE_EQ(reverse.inputs_(0, 2), 0.0);
    ASSERT_DOUBLE_EQ(reverse.inputs_(1, 2), 0.0);
    ASSERT_EQ(reverse.transposeBackwardErrors_.size(), 3);
    ASSERT_DOUBLE_EQ(reverse.transposeBackwardErrors_[2], 0.0);
}

TEST(ImplicitRootTest, TestNonSymmetricTransposeAndPivoting) {
    const CoupledEquation_ equation(2.0, 3.0);
    Matrix_<> seeds(2, 1);
    seeds(0, 0) = 1.0;
    seeds(1, 0) = -2.0;
    for (bool pivot : {false, true}) {
        const ImplicitRootLinearization_ root(equation, pivot ? Vector_<>{0.0, 3.0} : Vector_<>{2.0, 3.0},
                                              pivot ? Vector_<>{6.0, 9.0} : Vector_<>{10.0, 15.0},
                                              ImplicitRootAccuracyPolicy_{Vector_<>{0.0, 0.0}, 1e-12});
        const auto reverse = root.Reverse(seeds);
        ASSERT_NEAR(reverse.inputs_(0, 0), pivot ? -2.0 : 2.0 / 3.0, 1e-10);
        ASSERT_NEAR(reverse.inputs_(1, 0), pivot ? 1.0 / 3.0 : -5.0 / 9.0, 1e-10);
        ASSERT_LE(reverse.transposeBackwardErrors_[0], 1e-12);
    }
}

TEST(ImplicitRootTest, TestIndependentCompleteCoupledThreeStepDifferences) {
    ASSERT_NO_FATAL_FAILURE(CoupledDifferences(1.0, 1.0, Vector_<>{2.0, 3.0}, Vector_<>{7.0, 11.0}));
    ASSERT_NO_FATAL_FAILURE(CoupledDifferences(2.0, 3.0, Vector_<>{2.0, 3.0}, Vector_<>{10.0, 15.0}));
    ASSERT_NO_FATAL_FAILURE(CoupledDifferences(2.0, 3.0, Vector_<>{0.0, 3.0}, Vector_<>{6.0, 9.0}));
}

TEST(ImplicitRootTest, TestNonzeroResidualStationarityUsesCompleteJacobian) {
    const StationaryEquation_ equation;
    const ImplicitRootLinearization_ root(equation, Vector_<>{1.0}, Vector_<>{0.0, 3.0}, ImplicitRootAccuracyPolicy_{Vector_<>{0.0}, 1e-12});
    const auto reverse = root.Reverse(Matrix_<>(1, 1, 1.0));
    ASSERT_DOUBLE_EQ(root.Residuals()[0], 0.0);
    ASSERT_NEAR(reverse.inputs_(0, 0), 2.0 / 7.0, 1e-10);
    ASSERT_NEAR(reverse.inputs_(1, 0), 1.0 / 7.0, 1e-10);
    ASSERT_GT(std::abs(reverse.inputs_(0, 0) - 2.0 / 5.0), 0.1);
}

TEST(ImplicitRootTest, TestIndependentCompleteStationaryThreeStepDifferences) {
    const StationaryEquation_ equation;
    const Vector_<> inputs{0.0, 3.0};
    const ImplicitRootLinearization_ root(equation, Vector_<>{1.0}, inputs, ImplicitRootAccuracyPolicy_{Vector_<>{0.0}, 1e-12});
    const auto reverse = root.Reverse(Matrix_<>(1, 1, 1.0));
    const double steps[] = {1e-3, 1e-4, 1e-5}, tolerances[] = {3e-8, 3e-10, 1e-9};
    for (int input = 0; input < 2; ++input)
        for (int step = 0; step < 3; ++step) {
            auto plus = inputs, minus = inputs;
            plus[input] += steps[step];
            minus[input] -= steps[step];
            const double reference = (CompleteStationaryRoot(plus) - CompleteStationaryRoot(minus)) / (2.0 * steps[step]);
            ASSERT_NEAR(reverse.inputs_(input, 0), reference, tolerances[step]);
        }
}

//
// Created by Codex on 2026/10/8.
//

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <memory>

#include <dal/math/buffercapacity.hpp>
#include <dal/math/optimization/implicitroot.hpp>
#include <dal/platform/platform.hpp>

namespace {
    class TinyInputEquation_ final : public Dal::ImplicitRootEquation_ {
    public:
        [[nodiscard]] Dal::ImplicitRootEvaluation_ Evaluate(const Dal::Vector_<>& parameters, const Dal::Vector_<>& inputs) const override {
            const double coefficient = std::numeric_limits<double>::min();
            Dal::ImplicitRootEvaluation_ result{Dal::Vector_<>{parameters[0] + coefficient * inputs[0], parameters[1] + coefficient * inputs[0]},
                                                Dal::SquareMatrix_<>(2), Dal::Matrix_<>(2, 1, coefficient)};
            result.parameterJacobian_(0, 0) = 1.0;
            result.parameterJacobian_(1, 1) = 1.0;
            return result;
        }
    };

    class IdentityEquation_ final : public Dal::ImplicitRootEquation_ {
        std::shared_ptr<int> calls_;

    public:
        explicit IdentityEquation_(std::shared_ptr<int> calls) : calls_(std::move(calls)) {}
        [[nodiscard]] Dal::ImplicitRootEvaluation_ Evaluate(const Dal::Vector_<>& parameters, const Dal::Vector_<>& inputs) const override {
            ++*calls_;
            const int n = static_cast<int>(parameters.size());
            Dal::ImplicitRootEvaluation_ result{Dal::Vector_<>(n), Dal::SquareMatrix_<>(n), Dal::Matrix_<>(n, n)};
            for (int row = 0; row < n; ++row) {
                result.residuals_[row] = parameters[row] - inputs[row];
                result.parameterJacobian_(row, row) = 1.0;
                result.inputJacobian_(row, row) = -1.0;
            }
            return result;
        }
    };
} // namespace

TEST(ImplicitRootTest, TestNonzeroContractionUnderflowRejectsAndCacheRecovers) {
    const TinyInputEquation_ equation;
    const Dal::ImplicitRootLinearization_ root(equation, Dal::Vector_<>{0.0, 0.0}, Dal::Vector_<>{0.0},
                                               Dal::ImplicitRootAccuracyPolicy_{Dal::Vector_<>{0.0, 0.0}, 0.0});
    const Dal::Matrix_<> underflowingSeeds(2, 1, std::ldexp(1.0, -53));
    ASSERT_THROW(static_cast<void>(root.Reverse(underflowingSeeds)), Dal::Exception_);
    const auto supported = root.Reverse(Dal::Matrix_<>(2, 1, 1.0));
    ASSERT_DOUBLE_EQ(supported.inputs_(0, 0), -2.0 * std::numeric_limits<double>::min());
    ASSERT_DOUBLE_EQ(supported.transposeBackwardErrors_[0], 0.0);
    Dal::Matrix_<> representableSeeds(2, 1, 0.0);
    representableSeeds(0, 0) = std::ldexp(1.0, -52);
    ASSERT_EQ(root.Reverse(representableSeeds).inputs_(0, 0), -std::numeric_limits<double>::denorm_min());
    ASSERT_EQ(root.Reverse(Dal::Matrix_<>(2, 1, 0.0)).inputs_(0, 0), 0.0);
}

TEST(ImplicitRootTest, TestValidateBeforeOneEvaluationAndNoReverseCallbacks) {
    const auto calls = std::make_shared<int>(0);
    const IdentityEquation_ equation(calls);
    const Dal::Vector_<> parameters{2.0}, inputs{2.0};
    const Dal::ImplicitRootAccuracyPolicy_ policy{Dal::Vector_<>{0.0}, 0.0};
    ASSERT_THROW(
        static_cast<void>(Dal::ImplicitRootLinearization_(equation, parameters, inputs, Dal::ImplicitRootAccuracyPolicy_{Dal::Vector_<>{-1.0}, 0.0})),
        Dal::Exception_);
    ASSERT_THROW(static_cast<void>(Dal::ImplicitRootLinearization_(equation, parameters, inputs, policy, 0.0)), Dal::Exception_);
    ASSERT_EQ(*calls, 0);
    const Dal::ImplicitRootLinearization_ root(equation, parameters, inputs, policy);
    ASSERT_EQ(*calls, 1);
    for (double weight : {1.0, -2.0, 0.0})
        ASSERT_DOUBLE_EQ(root.Reverse(Dal::Matrix_<>(1, 1, weight)).inputs_(0, 0), weight);
    ASSERT_EQ(*calls, 1);
}

TEST(ImplicitRootTest, TestCoupledCapacityExcludesDenseMatrixAdjoints) {
    const IdentityEquation_ equation(std::make_shared<int>(0));
    const Dal::Vector_<> parameters{2.0, 3.0}, inputs{2.0, 3.0};
    const Dal::ImplicitRootAccuracyPolicy_ policy{Dal::Vector_<>{0.0, 0.0}, 0.0};
    Dal::BufferCapacityBudget_ captureBudget(304);
    {
        Dal::BufferCapacityScope_ caller(&captureBudget);
        const Dal::ImplicitRootLinearization_ root(equation, parameters, inputs, policy);
        ASSERT_EQ(captureBudget.CapacityBytes(), 192);
        ASSERT_EQ(captureBudget.PeakCapacityBytes(), 304);
    }
    ASSERT_EQ(captureBudget.CapacityBytes(), 0);
    const Dal::ImplicitRootLinearization_ root(equation, parameters, inputs, policy);
    const Dal::Matrix_<> seeds(2, 3, 1.0);
    Dal::BufferCapacityBudget_ reverseBudget(112);
    {
        Dal::BufferCapacityScope_ caller(&reverseBudget);
        const auto reverse = root.Reverse(seeds);
        ASSERT_EQ(reverseBudget.CapacityBytes(), 72);
        ASSERT_EQ(reverseBudget.PeakCapacityBytes(), 112);
        for (double value : reverse.inputs_)
            ASSERT_DOUBLE_EQ(value, 1.0);
    }
    ASSERT_EQ(reverseBudget.CapacityBytes(), 0);
    Dal::BufferCapacityBudget_ shortBudget(111);
    {
        Dal::BufferCapacityScope_ caller(&shortBudget);
        ASSERT_THROW(static_cast<void>(root.Reverse(seeds)), Dal::Exception_);
        ASSERT_EQ(shortBudget.CapacityBytes(), 0);
    }
}

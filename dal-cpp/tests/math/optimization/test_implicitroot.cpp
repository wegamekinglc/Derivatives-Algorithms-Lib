//
// Created by Codex on 2026/10/8.
//

#include <gtest/gtest.h>

#include <dal/math/optimization/implicitroot.hpp>
#include <dal/platform/platform.hpp>

namespace {
    class QuadraticEquation_ final : public Dal::ImplicitRootEquation_ {
    public:
        [[nodiscard]] Dal::ImplicitRootEvaluation_ Evaluate(const Dal::Vector_<>& parameters, const Dal::Vector_<>& inputs) const override {
            Dal::ImplicitRootEvaluation_ result{Dal::Vector_<>{parameters[0] * parameters[0] - inputs[0]},
                                                Dal::SquareMatrix_<>(1, 2.0 * parameters[0]), Dal::Matrix_<>(1, 1, -1.0)};
            return result;
        }
    };
} // namespace

TEST(ImplicitRootTest, TestQuadraticOwningCandidateAndReverse) {
    const QuadraticEquation_ equation;
    Dal::Vector_<> parameters{2.0}, inputs{4.0};
    Dal::ImplicitRootAccuracyPolicy_ policy{Dal::Vector_<>{0.0}, 0.0};
    const Dal::ImplicitRootLinearization_ root(equation, parameters, inputs, policy);
    parameters[0] = 1.9;
    inputs[0] = 9.0;
    policy.residualAbsoluteLimits_[0] = 1.0;
    const auto reverse = root.Reverse(Dal::Matrix_<>(1, 1, 1.0));
    ASSERT_DOUBLE_EQ(root.Parameters()[0], 2.0);
    ASSERT_DOUBLE_EQ(root.Inputs()[0], 4.0);
    ASSERT_DOUBLE_EQ(root.Residuals()[0], 0.0);
    ASSERT_DOUBLE_EQ(root.Policy().residualAbsoluteLimits_[0], 0.0);
    ASSERT_DOUBLE_EQ(root.ReciprocalJacobianConditionInfinity(), 1.0);
    ASSERT_EQ(reverse.inputs_.Rows(), 1);
    ASSERT_EQ(reverse.inputs_.Cols(), 1);
    ASSERT_DOUBLE_EQ(reverse.inputs_(0, 0), 0.25);
    ASSERT_EQ(reverse.transposeBackwardErrors_.size(), 1);
    ASSERT_DOUBLE_EQ(reverse.transposeBackwardErrors_[0], 0.0);
}

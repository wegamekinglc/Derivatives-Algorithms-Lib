//
// Created by Codex on 2026/10/8.
//

#pragma once

#include <dal/math/matrix/linearsolveaccuracy.hpp>

namespace Dal {
    struct ImplicitRootEvaluation_ {
        Vector_<> residuals_;
        SquareMatrix_<> parameterJacobian_;
        Matrix_<> inputJacobian_;
    };

    class ImplicitRootEquation_ {
    public:
        virtual ~ImplicitRootEquation_() = default;
        [[nodiscard]] virtual ImplicitRootEvaluation_ Evaluate(const Vector_<>& parameters, const Vector_<>& inputs) const = 0;
    };

    struct ImplicitRootAccuracyPolicy_ {
        Vector_<> residualAbsoluteLimits_;
        double transposeBackwardErrorLimit_ = 0.0;
    };

    struct ImplicitRootAdjoints_ {
        Matrix_<> inputs_;
        Vector_<> transposeBackwardErrors_;
    };

    class ImplicitRootLinearization_ {
        struct Capture_;
        Vector_<> parameters_;
        Vector_<> inputs_;
        Vector_<> residuals_;
        ImplicitRootAccuracyPolicy_ policy_;
        Matrix_<> inputJacobian_;
        CheckedLinearSolve_ jacobian_;

        explicit ImplicitRootLinearization_(Capture_&& capture);

    public:
        ImplicitRootLinearization_(const ImplicitRootEquation_& equation,
                                   const Vector_<>& parameters,
                                   const Vector_<>& inputs,
                                   const ImplicitRootAccuracyPolicy_& policy,
                                   double relativePivotTolerance = 64.0 * std::numeric_limits<double>::epsilon());
        [[nodiscard]] const Vector_<>& Parameters() const { return parameters_; }
        [[nodiscard]] const Vector_<>& Inputs() const { return inputs_; }
        [[nodiscard]] const Vector_<>& Residuals() const { return residuals_; }
        [[nodiscard]] const ImplicitRootAccuracyPolicy_& Policy() const { return policy_; }
        [[nodiscard]] double ReciprocalJacobianConditionInfinity() const { return jacobian_.Diagnostics().reciprocalConditionInfinity_; }
        [[nodiscard]] ImplicitRootAdjoints_ Reverse(const Matrix_<>& parameterAdjoints) const;
    };
} // namespace Dal

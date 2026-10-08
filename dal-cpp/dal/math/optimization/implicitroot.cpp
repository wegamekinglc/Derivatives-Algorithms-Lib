//
// Created by Codex on 2026/10/8.
//

#include <dal/platform/platform.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include <dal/math/optimization/implicitroot.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal {
    namespace {
        template <class C_> void ValidateFinite(const C_& values, const char* message) {
            for (double value : values)
                REQUIRE(std::isfinite(value), message);
        }

        void ValidateExtent(size_t rows, size_t columns) {
            const size_t maximum =
                std::min(static_cast<size_t>(std::numeric_limits<int>::max()), std::numeric_limits<size_t>::max() / sizeof(double));
            REQUIRE(rows <= maximum && columns <= maximum && (rows == 0 || columns <= maximum / rows),
                    "ImplicitRoot: matrix extent exceeds the supported range");
        }

        void ValidateCapture(const Vector_<>& parameters,
                             const Vector_<>& inputs,
                             const ImplicitRootAccuracyPolicy_& policy,
                             double relativePivotTolerance) {
            REQUIRE(!parameters.empty(), "ImplicitRoot: candidate must contain at least one parameter");
            ValidateExtent(parameters.size(), parameters.size());
            ValidateExtent(parameters.size(), inputs.size());
            ValidateFinite(parameters, "ImplicitRoot: candidate entries must be finite");
            ValidateFinite(inputs, "ImplicitRoot: input entries must be finite");
            REQUIRE(policy.residualAbsoluteLimits_.size() == parameters.size(), "ImplicitRoot: residual limits must match the equation count");
            for (double limit : policy.residualAbsoluteLimits_)
                REQUIRE(std::isfinite(limit) && limit >= 0.0, "ImplicitRoot: residual limits must be finite and nonnegative");
            const double transposeLimit = policy.transposeBackwardErrorLimit_;
            REQUIRE(std::isfinite(transposeLimit) && transposeLimit >= 0.0 && transposeLimit <= 1.0,
                    "ImplicitRoot: transpose backward-error limit must be finite in [0,1]");
            REQUIRE(std::isfinite(relativePivotTolerance) && relativePivotTolerance > 0.0 && relativePivotTolerance < 1.0,
                    "ImplicitRoot: relative pivot tolerance must be finite and strictly between zero and one");
        }

        CheckedLinearSolve_ CaptureJacobian(const ImplicitRootEvaluation_& evaluation,
                                            int parameters,
                                            int inputs,
                                            const ImplicitRootAccuracyPolicy_& policy,
                                            double relativePivotTolerance) {
            REQUIRE(evaluation.residuals_.size() == static_cast<size_t>(parameters), "ImplicitRoot: residual shape must match the candidate");
            REQUIRE(evaluation.parameterJacobian_.Rows() == parameters, "ImplicitRoot: parameter Jacobian shape must match the candidate");
            REQUIRE(evaluation.inputJacobian_.Rows() == parameters && evaluation.inputJacobian_.Cols() == inputs,
                    "ImplicitRoot: input Jacobian shape must match the candidate and inputs");
            ValidateFinite(evaluation.residuals_, "ImplicitRoot: residual entries must be finite");
            ValidateFinite(static_cast<const Matrix_<>&>(evaluation.parameterJacobian_), "ImplicitRoot: parameter Jacobian entries must be finite");
            ValidateFinite(evaluation.inputJacobian_, "ImplicitRoot: input Jacobian entries must be finite");
            for (int row = 0; row < parameters; ++row)
                REQUIRE(std::abs(evaluation.residuals_[row]) <= policy.residualAbsoluteLimits_[row],
                        "ImplicitRoot: equation residual exceeds its declared absolute limit");
            return CheckedLinearSolve_(evaluation.parameterJacobian_, Matrix_<>(parameters, 1, 0.0),
                                       LinearSolveAccuracyPolicy_{0.0, policy.transposeBackwardErrorLimit_}, relativePivotTolerance);
        }

        void ValidateSeeds(const Matrix_<>& seeds, int parameters, int inputs) {
            REQUIRE(seeds.Rows() == parameters && seeds.Cols() > 0, "ImplicitRoot.Reverse: seeds require candidate-size rows and positive columns");
            ValidateExtent(parameters, seeds.Cols());
            ValidateExtent(inputs, seeds.Cols());
            ValidateFinite(seeds, "ImplicitRoot.Reverse: seed entries must be finite");
        }

        double InputAdjoint(const Matrix_<>& jacobian, const Matrix_<>& lambda, int input) {
            double value = 0.0;
            for (int row = 0; row < jacobian.Rows(); ++row) {
                value -= jacobian(row, input) * lambda(row, 0);
                REQUIRE(std::isfinite(value), "ImplicitRoot.Reverse: input adjoint accumulation overflow");
            }
            return value;
        }
    } // namespace

    struct ImplicitRootLinearization_::Capture_ {
        Vector_<> parameters_, inputs_;
        ImplicitRootAccuracyPolicy_ policy_;
        ImplicitRootEvaluation_ evaluation_;
        CheckedLinearSolve_ jacobian_;

        [[nodiscard]] static Capture_ Make(const ImplicitRootEquation_& equation,
                                           const Vector_<>& parameters,
                                           const Vector_<>& inputs,
                                           const ImplicitRootAccuracyPolicy_& policy,
                                           double relativePivotTolerance) {
            ValidateCapture(parameters, inputs, policy, relativePivotTolerance);
            return Capture_(equation, parameters, inputs, policy, relativePivotTolerance);
        }

    private:
        Capture_(const ImplicitRootEquation_& equation,
                 const Vector_<>& parameters,
                 const Vector_<>& inputs,
                 const ImplicitRootAccuracyPolicy_& policy,
                 double relativePivotTolerance)
            : parameters_(parameters), inputs_(inputs), policy_(policy), evaluation_(equation.Evaluate(parameters_, inputs_)),
              jacobian_(CaptureJacobian(
                  evaluation_, static_cast<int>(parameters_.size()), static_cast<int>(inputs_.size()), policy_, relativePivotTolerance)) {}
    };

    ImplicitRootLinearization_::ImplicitRootLinearization_(Capture_&& capture)
        : parameters_(std::move(capture.parameters_)), inputs_(std::move(capture.inputs_)), residuals_(std::move(capture.evaluation_.residuals_)),
          policy_(std::move(capture.policy_)), inputJacobian_(std::move(capture.evaluation_.inputJacobian_)),
          jacobian_(std::move(capture.jacobian_)) {}

    ImplicitRootLinearization_::ImplicitRootLinearization_(const ImplicitRootEquation_& equation,
                                                           const Vector_<>& parameters,
                                                           const Vector_<>& inputs,
                                                           const ImplicitRootAccuracyPolicy_& policy,
                                                           double relativePivotTolerance)
        : ImplicitRootLinearization_(Capture_::Make(equation, parameters, inputs, policy, relativePivotTolerance)) {}

    ImplicitRootAdjoints_ ImplicitRootLinearization_::Reverse(const Matrix_<>& parameterAdjoints) const {
        const int n = static_cast<int>(parameters_.size()), k = static_cast<int>(inputs_.size());
        ValidateSeeds(parameterAdjoints, n, k);
        ImplicitRootAdjoints_ result{Matrix_<>(k, parameterAdjoints.Cols()), Vector_<>(parameterAdjoints.Cols())};
        for (int column = 0; column < parameterAdjoints.Cols(); ++column) {
            Matrix_<> seeds(n, 1);
            for (int row = 0; row < n; ++row)
                seeds(row, 0) = parameterAdjoints(row, column);
            const auto reverse = jacobian_.ReverseRhs(seeds);
            result.transposeBackwardErrors_[column] = reverse.transposeBackwardErrors_[0];
            for (int input = 0; input < k; ++input)
                result.inputs_(input, column) = InputAdjoint(inputJacobian_, reverse.adjoints_.rhs_, input);
        }
        return result;
    }
} // namespace Dal

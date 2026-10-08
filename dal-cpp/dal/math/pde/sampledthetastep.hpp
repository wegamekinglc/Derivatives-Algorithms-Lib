//
// Created by Codex on 2026/10/8.
//

#pragma once

#include <array>
#include <limits>

#include <dal/math/matrix/linearsolveaccuracy.hpp>
#include <dal/math/pde/sampledthetastepinternal.hpp>

namespace Dal::PDE {
    struct SampledThetaStepInputs_ {
        Vector_<> x_, rates_, drifts_, variances_;
        double dt_ = 0.0;
        double theta_ = 0.5;
        Matrix_<> oldValues_;
        std::array<bool, 2> externalBoundaries_ = {false, false};
        Matrix_<> externalValues_;
    };

    struct SampledThetaStepAdjoints_ {
        Matrix_<> oldValues_, externalValues_;
        Vector_<> rates_, drifts_, variances_;
        double dt_ = 0.0;
        double theta_ = 0.0;
        Vector_<> transposeBackwardErrors_;
    };

    class SampledThetaStepPullback_ {
        Matrix_<> dx_, dxx_, generator_, oldValues_, solution_;
        Vector_<> forwardBackwardErrors_;
        SampledThetaDetail::TridiagonalFactors_ factors_;
        LinearSolveAccuracyPolicy_ policy_;
        double dt_, theta_, implicitMultiplier_, explicitMultiplier_;
        std::array<bool, 2> externalBoundaries_;

        void BuildGenerator(const SampledThetaStepInputs_& inputs);
        void BuildFactors(double tolerance);
        [[nodiscard]] double ImplicitEntry(int row, int column) const;
        [[nodiscard]] double ExplicitEntry(int row, int local) const;
        [[nodiscard]] Matrix_<> BuildRhs(const SampledThetaStepInputs_& inputs) const;
        [[nodiscard]] Vector_<> BackwardErrors(bool transpose, const Matrix_<>& rhs, const Matrix_<>& solution) const;
        void AccumulateInterior(int row, int layer, double lambda, SampledThetaStepAdjoints_* risk) const;
        void AccumulateAdjoints(const Matrix_<>& lambda, SampledThetaStepAdjoints_* risk) const;

    public:
        SampledThetaStepPullback_(const SampledThetaStepInputs_& inputs,
                                  const LinearSolveAccuracyPolicy_& policy,
                                  double relativePivotTolerance = 64.0 * std::numeric_limits<double>::epsilon());
        [[nodiscard]] const Matrix_<>& Solution() const { return solution_; }
        [[nodiscard]] const Vector_<>& ForwardBackwardErrors() const { return forwardBackwardErrors_; }
        [[nodiscard]] const LinearSolveAccuracyPolicy_& Policy() const { return policy_; }
        [[nodiscard]] SampledThetaStepAdjoints_ Reverse(const Matrix_<>& solutionSeeds) const;
    };
} // namespace Dal::PDE

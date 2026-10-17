//
// Created by Codex on 2026/10/10.
//

#pragma once

#include <utility>

#include <dal-public/src/dupireriskrequest.hpp>
#include <dal/math/aad/bumpoveraad.hpp>

namespace Dal {
    struct DupireScriptCurvatureRequest_ {
        DupireScriptRiskRequest_ risk_;
        AAD::BumpOverAADRequest_ bumps_;
    };

    struct DupireScriptCurvatureExecution_ {
        String_ method_ = "BumpOverRecalibratedNativeDupireMonteCarloAAD";
        size_t quoteGradientEvaluations_ = 0;
        size_t pathsPerEvaluation_ = 0;
        size_t numericPayloadBytes_ = 0;
    };

    class DupireScriptCurvatureResult_;

    class DupireScriptCurvaturePlan_ {
        DupireScriptRiskPlan_ base_;
        Vector_<> point_;
        AAD::BumpOverAADRequest_ bumps_;
        size_t numericPayloadBytes_;

        DupireScriptCurvaturePlan_(DupireScriptRiskPlan_ base, Vector_<> point, AAD::BumpOverAADRequest_ bumps, size_t bytes)
            : base_(std::move(base)), point_(std::move(point)), bumps_(std::move(bumps)), numericPayloadBytes_(bytes) {}
        friend DupireScriptCurvaturePlan_ PlanDupireScriptCurvature(const Handle_<ScriptProductData_>&,
                                                                    const Handle_<ModelData_>&,
                                                                    const DupireCalibrationSnapshot_&,
                                                                    const String_&,
                                                                    const DupireScriptCurvatureRequest_&);
        friend DupireScriptCurvatureResult_ ValueByMonteCarloWithDupireCurvature(const DupireScriptCurvaturePlan_&);

    public:
        [[nodiscard]] const DupireScriptRiskPlan_& BasePlan() const { return base_; }
        [[nodiscard]] const Vector_<>& Point() const { return point_; }
        [[nodiscard]] const Matrix_<>& Directions() const { return bumps_.directions_; }
        [[nodiscard]] const Vector_<>& Steps() const { return bumps_.steps_; }
        [[nodiscard]] size_t NumericPayloadBytes() const { return numericPayloadBytes_; }
    };

    [[nodiscard]] DupireScriptCurvaturePlan_ PlanDupireScriptCurvature(const Handle_<ScriptProductData_>& product,
                                                                       const Handle_<ModelData_>& model,
                                                                       const DupireCalibrationSnapshot_& calibration,
                                                                       const String_& component,
                                                                       const DupireScriptCurvatureRequest_& request);

    class DupireScriptCurvatureResult_ {
        DupireScriptRiskResult_ base_;
        Vector_<> point_;
        Vector_<> gradient_;
        AAD::BumpOverAADRequest_ bumps_;
        Matrix_<> products_;
        DupireScriptCurvatureExecution_ execution_;

        DupireScriptCurvatureResult_(DupireScriptRiskResult_ base,
                                     Vector_<> point,
                                     Vector_<> gradient,
                                     AAD::BumpOverAADRequest_ bumps,
                                     Matrix_<> products,
                                     DupireScriptCurvatureExecution_ execution)
            : base_(std::move(base)), point_(std::move(point)), gradient_(std::move(gradient)), bumps_(std::move(bumps)),
              products_(std::move(products)), execution_(std::move(execution)) {}
        friend DupireScriptCurvatureResult_ ValueByMonteCarloWithDupireCurvature(const DupireScriptCurvaturePlan_&);

    public:
        [[nodiscard]] const DupireScriptRiskResult_& Base() const { return base_; }
        [[nodiscard]] const Vector_<>& Point() const { return point_; }
        [[nodiscard]] const Vector_<>& Gradient() const { return gradient_; }
        [[nodiscard]] const Vector_<CalibrationQuoteCoordinate_>& InputAxis() const { return base_.QuoteRisk().Plan().CompleteInputAxis(); }
        [[nodiscard]] const Matrix_<>& Directions() const { return bumps_.directions_; }
        [[nodiscard]] const Vector_<>& Steps() const { return bumps_.steps_; }
        [[nodiscard]] const Matrix_<>& HessianProducts() const { return products_; }
        [[nodiscard]] const DupireScriptCurvatureExecution_& Execution() const { return execution_; }
    };

    [[nodiscard]] DupireScriptCurvatureResult_ ValueByMonteCarloWithDupireCurvature(const DupireScriptCurvaturePlan_& plan);
} // namespace Dal

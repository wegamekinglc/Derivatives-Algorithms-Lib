//
// Created by Codex on 2026/10/10.
//

#include <algorithm>

#include <dal/math/aad/detail/gradientbumps.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/storage/globals.hpp>

#include <dal-public/src/dupirecurvature.hpp>

namespace Dal {
    namespace {
        namespace Bumps = AAD::GradientBumpsDetail;

        const DupireCalibrationSnapshot_& Calibration(const DupireScriptRiskPlan_& plan) {
            return std::get<DupireCalibrationSnapshot_>(plan.QuotePlan().Calibration().Source());
        }

        Matrix_<> QuoteMatrix(const DupireCalibrationSnapshot_& calibration, const Vector_<>& point) {
            const auto& original = calibration.Inputs().quoteSpreads_;
            Matrix_<> result(original.Rows(), original.Cols());
            std::copy(point.begin(), point.end(), result.Data());
            return result;
        }

        void ValidateCalibrationPoint(const DupireCalibrationSnapshot_& calibration, const Vector_<>& point, const String_& context) {
            try {
                ValidateDupireQuoteRecalibration(calibration, QuoteMatrix(calibration, point));
            } catch (const Exception_& error) {
                THROW("DupireScriptCurvature: admission; " + context + "; " + error.what());
            }
        }

        DupireScriptRiskPlan_ RebuildPoint(const DupireScriptRiskPlan_& plan, const Vector_<>& point, const String_& context) {
            try {
                return RecalibrateDupireScriptRisk(plan, QuoteMatrix(Calibration(plan), point));
            } catch (const Exception_& error) {
                THROW("DupireScriptCurvature: rebuild; " + context + "; " + error.what());
            }
        }

        struct Gradient_ {
            DupireScriptRiskResult_ result_;
            Vector_<> gradient_;
            [[nodiscard]] const Vector_<>& Gradient() const { return gradient_; }
        };

        Gradient_ EvaluateGradient(const DupireScriptRiskPlan_& plan, const Vector_<>& point, const String_& context) {
            auto rebuilt = RebuildPoint(plan, point, context);
            try {
                auto result = ValueByMonteCarloWithDupireRisk(rebuilt);
                const auto& total = result.QuoteRisk().QuoteRisk().TotalAdjoints();
                Vector_<> gradient(total.begin(), total.end());
                return {std::move(result), std::move(gradient)};
            } catch (const Exception_& error) {
                THROW("DupireScriptCurvature: execution; " + context + "; " + error.what());
            }
        }
    } // namespace

    DupireScriptCurvaturePlan_ PlanDupireScriptCurvature(const Handle_<ScriptProductData_>& product,
                                                         const Handle_<ModelData_>& model,
                                                         const DupireCalibrationSnapshot_& calibration,
                                                         const String_& component,
                                                         const DupireScriptCurvatureRequest_& request) {
        AAD::RequireRecordingModeChangeAllowed();
        XGLOBAL::ValuationMutationGuard_ guard;
        auto fixed = request;
        const auto fixedCalibration = calibration;
        REQUIRE(!fixed.risk_.direct_, "DupireScriptCurvature: external first-order direct seeds do not define curvature");
        REQUIRE(!fixed.bumps_.recordingCapacityBudgetBytes_, "DupireScriptCurvature: worker recording capacity limits are unsupported");
        const auto& quotes = fixedCalibration.Inputs().quoteSpreads_;
        Vector_<> point(quotes.begin(), quotes.end());
        const size_t bumpBytes = Bumps::Validate(point, fixed.bumps_);
        ValidateCalibrationPoint(fixedCalibration, point, "base");
        for (int row = 0; row < fixed.bumps_.directions_.Rows(); ++row) {
            const auto context = Bumps::DirectionContext(row);
            ValidateCalibrationPoint(fixedCalibration, Bumps::BumpedPoint(point, fixed.bumps_, row, 1.0), context + "; plus");
            ValidateCalibrationPoint(fixedCalibration, Bumps::BumpedPoint(point, fixed.bumps_, row, -1.0), context + "; minus");
        }
        auto base = PlanDupireScriptRisk(product, model, fixedCalibration, component, fixed.risk_);
        const size_t bytes = Bumps::Sum(bumpBytes, base.NumericPayloadBytes());
        REQUIRE(!fixed.bumps_.numericPayloadBudgetBytes_ || bytes <= *fixed.bumps_.numericPayloadBudgetBytes_,
                "DupireScriptCurvature: combined numeric payload budget exceeded; requiredBytes=" + String_(std::to_string(bytes)));
        base = RebuildPoint(base, point, "base");
        for (int row = 0; row < fixed.bumps_.directions_.Rows(); ++row) {
            const auto context = Bumps::DirectionContext(row);
            static_cast<void>(RebuildPoint(base, Bumps::BumpedPoint(point, fixed.bumps_, row, 1.0), context + "; plus"));
            static_cast<void>(RebuildPoint(base, Bumps::BumpedPoint(point, fixed.bumps_, row, -1.0), context + "; minus"));
        }
        return {std::move(base), std::move(point), std::move(fixed.bumps_), bytes};
    }

    DupireScriptCurvatureResult_ ValueByMonteCarloWithDupireCurvature(const DupireScriptCurvaturePlan_& plan) {
        AAD::RequireRecordingModeChangeAllowed();
        XGLOBAL::ValuationMutationGuard_ guard;
        const auto fixed = plan;
        auto* tape = AAD::Tape();
        const AAD::NumResultsResetterForAAD_ mode(tape, tape->multi_, tape->numAdj_);
        tape->multi_ = false;
        tape->numAdj_ = 1;
        auto evaluated =
            Bumps::Evaluate([&](const Vector_<>& point, const String_& context) { return EvaluateGradient(fixed.base_, point, context); },
                            fixed.point_, fixed.bumps_);
        DupireScriptCurvatureExecution_ execution;
        execution.quoteGradientEvaluations_ = Bumps::Sum(1, Bumps::Product(2, fixed.bumps_.steps_.size()));
        execution.pathsPerEvaluation_ = static_cast<size_t>(fixed.base_.NumPaths());
        execution.numericPayloadBytes_ = fixed.numericPayloadBytes_;
        return {std::move(evaluated.base_.result_), fixed.point_,        std::move(evaluated.base_.gradient_), fixed.bumps_,
                std::move(evaluated.products_),     std::move(execution)};
    }
} // namespace Dal

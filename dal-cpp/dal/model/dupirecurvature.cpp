//
// Created by Codex on 2026/10/10.
//

#include <algorithm>
#include <limits>

#include <dal/math/aad/detail/gradientbumps.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/math/aad/tapecapacity.hpp>
#include <dal/model/dupirecurvature.hpp>

namespace Dal {
    namespace {
        namespace Bumps = AAD::GradientBumpsDetail;

        Vector_<> Flatten(const Matrix_<>& values) { return Vector_<>(values.begin(), values.end()); }

        Matrix_<> QuoteMatrix(const DupireCalibrationSnapshot_& calibration, const Vector_<>& point) {
            const auto& original = calibration.Inputs().quoteSpreads_;
            Matrix_<> result(original.Rows(), original.Cols());
            std::copy(point.begin(), point.end(), result.Data());
            return result;
        }

        Vector_<> ObjectivePoint(const DupireCalibrationSnapshot_& calibration) {
            auto point = Flatten(calibration.Surface()->vols_);
            const auto& quotes = calibration.Inputs().quoteSpreads_;
            point.Append(quotes);
            return point;
        }

        void ValidatePoint(const DupireCalibrationSnapshot_& calibration, const Vector_<>& point, const String_& context) {
            try {
                ValidateDupireQuoteRecalibration(calibration, QuoteMatrix(calibration, point));
            } catch (const Exception_& error) {
                THROW("DupireQuoteCurvature: admission; " + context + "; " + error.what());
            }
        }

        struct Gradient_ {
            double value_;
            Vector_<> gradient_;
            DupireCalibrationSnapshot_ calibration_;
            [[nodiscard]] const Vector_<>& Gradient() const { return gradient_; }
        };

        Gradient_ EvaluateGradient(const AAD::NativeScalarFunction_& objective,
                                   const DupireCalibrationSnapshot_& original,
                                   const Vector_<>& point,
                                   const std::optional<size_t>& recordingCap,
                                   const String_& context,
                                   DupireQuoteCurvatureExecution_* execution) {
            String_ stage = "calibration";
            try {
                auto calibration = RecalibrateDupireWithRisk(original, QuoteMatrix(original, point));
                auto objectivePoint = ObjectivePoint(calibration);
                AAD::BumpOverAADRequest_ firstOrder;
                firstOrder.directions_ = Matrix_<>(0, static_cast<int>(objectivePoint.size()));
                firstOrder.recordingCapacityBudgetBytes_ = recordingCap;
                stage = "objective";
                const auto differentiated = AAD::EvaluateBumpOverAAD(objective, objectivePoint, firstOrder);
                execution->peakTapeBytes_ = std::max(execution->peakTapeBytes_, differentiated.Execution().peakTapeBytes_);
                execution->cleanupReserveBytes_ = std::max(execution->cleanupReserveBytes_, differentiated.Execution().cleanupReserveBytes_);
                const auto& surface = calibration.Surface()->vols_;
                Matrix_<> parameters(surface.Rows(), surface.Cols());
                const size_t nodes = static_cast<size_t>(surface.Rows()) * surface.Cols();
                std::copy_n(differentiated.Gradient().begin(), nodes, parameters.Data());
                auto direct = QuoteMatrix(calibration, point);
                std::copy(differentiated.Gradient().begin() + static_cast<std::ptrdiff_t>(nodes), differentiated.Gradient().end(), direct.Data());
                stage = "calibration pullback";
                AAD::TapeCapacityBudget_ budget(recordingCap.value_or(std::numeric_limits<size_t>::max()));
                AAD::TapeCapacityScope_ capacity(&budget, true);
                const auto risk = PullbackDupireCalibration(calibration, {calibration, std::move(parameters)},
                                                            DupireDirectQuoteAdjoints_{calibration, std::move(direct)});
                execution->peakTapeBytes_ = std::max(execution->peakTapeBytes_, budget.PeakCapacityBytes());
                execution->cleanupReserveBytes_ = std::max(execution->cleanupReserveBytes_, AAD::TapeCleanupCapacityBytes());
                capacity.Close();
                return {differentiated.Value(), Flatten(risk.TotalAdjoints()), std::move(calibration)};
            } catch (const Exception_& error) {
                THROW("DupireQuoteCurvature: " + context + "; stage=" + stage + "; " + error.what());
            }
        }
    } // namespace

    DupireQuoteCurvatureResult_ EvaluateDupireQuoteCurvature(const AAD::NativeScalarFunction_& objective,
                                                             const DupireCalibrationSnapshot_& calibration,
                                                             const AAD::BumpOverAADRequest_& request) {
        AAD::RequireRecordingModeChangeAllowed();
        const AAD::NativeScalarFunction_ fixedObjective = objective;
        const DupireCalibrationSnapshot_ fixedCalibration = calibration;
        AAD::BumpOverAADRequest_ fixedRequest = request;
        Vector_<> point = Flatten(fixedCalibration.Inputs().quoteSpreads_);
        REQUIRE(static_cast<bool>(fixedObjective), "DupireQuoteCurvature: objective must be available");
        const auto& surface = fixedCalibration.Surface()->vols_;
        const size_t nodes = Bumps::Product(static_cast<size_t>(surface.Rows()), static_cast<size_t>(surface.Cols()));
        REQUIRE(Bumps::Sum(nodes, point.size()) <= static_cast<size_t>(std::numeric_limits<int>::max()),
                "DupireQuoteCurvature: objective input count exceeds the matrix integer range");
        DupireQuoteCurvatureExecution_ execution;
        execution.numericPayloadBytes_ = Bumps::Validate(point, fixedRequest);
        execution.quoteGradientEvaluations_ = Bumps::Sum(1, Bumps::Product(2, fixedRequest.steps_.size()));
        execution.calibrations_ = execution.quoteGradientEvaluations_;
        execution.objectiveReverseSweeps_ = execution.quoteGradientEvaluations_;
        execution.calibrationReverseSweeps_ = execution.quoteGradientEvaluations_;
        ValidatePoint(fixedCalibration, point, "base");
        for (int row = 0; row < fixedRequest.directions_.Rows(); ++row) {
            const String_ context = Bumps::DirectionContext(row);
            ValidatePoint(fixedCalibration, Bumps::BumpedPoint(point, fixedRequest, row, 1.0), context + "; plus");
            ValidatePoint(fixedCalibration, Bumps::BumpedPoint(point, fixedRequest, row, -1.0), context + "; minus");
        }
        auto* tape = AAD::Tape();
        const AAD::NumResultsResetterForAAD_ mode(tape, tape->multi_, tape->numAdj_);
        tape->multi_ = false;
        tape->numAdj_ = 1;
        auto result = Bumps::Evaluate(
            [&](const Vector_<>& inputs, const String_& context) {
                return EvaluateGradient(fixedObjective, fixedCalibration, inputs, fixedRequest.recordingCapacityBudgetBytes_, context, &execution);
            },
            point, fixedRequest);
        return {result.base_.value_,         std::move(result.base_.gradient_),    std::move(point),    std::move(fixedRequest),
                std::move(result.products_), std::move(result.base_.calibration_), std::move(execution)};
    }
} // namespace Dal

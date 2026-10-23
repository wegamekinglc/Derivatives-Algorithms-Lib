#include <dal/platform/platform.hpp>

#include <dal/calibration/validation_internal.hpp>
#include <dal/script/xccymc.hpp>

namespace Dal {
    XccyMcValidation_ ValidateXccyMc(const XccyMcResult_& value, const CalibrationMcReference_& reference, const CalibrationMcPrecision_& precision) {
        using namespace CalibrationInternal;
        Check(reference.numericalError_ <= .01 * precision.errorScale_, "InvalidCalibrationPlan", "/reference/numerical_error",
              "independently measured value indicator within one percent of frozen scale");
        auto comparison = value.estimate_;
        comparison.validation_ = true;
        XccyMcValidation_ result{ValidateCalibrationMc(comparison, reference, precision), true, {}};
        for (size_t j = 0; j < comparison.gradient_.size(); ++j) {
            const double scale = precision.errorScale_ / precision.parameterScales_[j];
            Check(reference.finiteDifferenceError_[j] <= .01 * scale, "InvalidCalibrationPlan", "/reference/finite_difference_error",
                  "independently measured gradient indicator within one percent of native gradient scale");
            const double combined = std::abs(comparison.gradient_[j] - reference.gradient_[j]) + result.validation_.gradientPrecisionIndicators_[j];
            result.gradientErrorIndicators_.push_back(combined);
            result.gradientCombinedPassed_ &= combined <= precision.gradientErrorFraction_ * scale;
        }
        auto& validation = result.validation_;
        validation.numericalValidationPassed_ &= result.gradientCombinedPassed_;
        validation.budgetExhausted_ = !validation.numericalValidationPassed_;
        validation.terminationReason_ = validation.numericalValidationPassed_ ? "NumericalValidationPassed" : "NumericalValidationFailed";
        auto errors = validation.report_->Errors();
        if (!result.gradientCombinedPassed_) {
            CalibrationDiagnostic_ diagnostic;
            diagnostic.stage_ = CalibrationStage_::Value_::Validate;
            diagnostic.code_ = "NumericalValidationFailed";
            diagnostic.field_ = "/validation/gradient_combined";
            diagnostic.measureId_ = comparison.measureId_;
            diagnostic.message_ = "Frozen combined native gradient error budget not met";
            errors.push_back(diagnostic);
        }
        auto snapshot = value.report_ ? value.report_->Data() : CalibrationReportSnapshot_{};
        snapshot.summary_.numericalValidationPassed_ = validation.numericalValidationPassed_;
        snapshot.summary_.terminationReason_ = validation.terminationReason_;
        snapshot.summary_.status_ = validation.numericalValidationPassed_ ? "Validated" : "Candidate";
        for (size_t j = 0; j < result.gradientErrorIndicators_.size(); ++j) {
            CalibrationReportValidation_ row;
            row.edge_ = value.reservation_.phase_.String() ? value.reservation_.phase_.String() : "Reference";
            row.quantity_ = comparison.parameterLabels_[j];
            row.reason_ = "CombinedGradientError";
            row.value_ = result.gradientErrorIndicators_[j];
            row.absoluteTolerance_ = precision.gradientErrorFraction_ * precision.errorScale_ / precision.parameterScales_[j];
            row.accepted_ = *row.value_ <= *row.absoluteTolerance_;
            snapshot.validation_.push_back(row);
        }
        validation.report_.reset(new CalibrationReport_(CalibrationStage_::Value_::Validate, {}, std::move(errors), snapshot));
        return result;
    }
} // namespace Dal

#include <dal/platform/platform.hpp>

#include <dal/calibration/validation_internal.hpp>
#include <dal/script/xccymc_internal.hpp>

namespace Dal::XccyMcInternal {
    String_ MappingIdentity(const CalibrationMcMapping_& mapping) {
        using namespace CalibrationInternal;
        return Fingerprint(Obj({{"layout", Str(AAD::XccyGaussian_<double>::LayoutVersion())},
                                {"numeraire", Str(mapping.numeraire_)},
                                {"clock", Str("ACT365F")},
                                {"seed", Num(mapping.seed_)},
                                {"generator", Str(mapping.generator_)},
                                {"normal", Str(mapping.normalPrecision_)},
                                {"bridge", Bool(mapping.brownianBridge_)},
                                {"stream", Str(mapping.streamVersion_)},
                                {"dimension", Str(OrdinalText(mapping.dimension_))},
                                {"timeline", Array(mapping.timeline_, [](double t) { return Num(t); })},
                                {"coordinates", Array(mapping.normalCoordinates_, [](const auto& c) { return Str(c); })}}));
    }
} // namespace Dal::XccyMcInternal

namespace Dal {
    namespace {
        using namespace CalibrationInternal;
        bool SourceShape(const CalibrationReportSnapshot_& data) {
            return data.observations_.size() == 1 && data.observations_[0].instrument_ && data.plans_.size() == 1 &&
                   data.execution_.sampling_.has_value();
        }
        bool SourceAxesMatch(const XccyMcResult_& value, const CalibrationReportSnapshot_& data) {
            const auto& row = data.observations_[0];
            return Bytes(value.estimate_.instrumentIdentity_) == Bytes(row.instrument_->InstrumentIdentity()) &&
                   Bytes(value.estimate_.planIdentity_) == Bytes(row.planIdentity_);
        }
        bool SourceContextMatches(const XccyMcResult_& value, const CalibrationReportSnapshot_& data) {
            return Bytes(data.summary_.contextIdentity_.value_or("")) == Bytes(value.estimate_.contextIdentity_) &&
                   Bytes(data.execution_.modelIdentity_.value_or("")) == Bytes(value.modelIdentity_) &&
                   Bytes(data.plans_[0].contextIdentity_.value_or("")) == Bytes(value.estimate_.contextIdentity_) &&
                   Bytes(data.plans_[0].ruleId_) == Bytes(value.ruleId_);
        }
        void RequireReport(const XccyMcResult_& value) {
            Check(static_cast<bool>(value.report_), "InvalidCalibrationPlan", "/validation/identity", "owning evaluated XCCY source report");
            const auto& data = value.report_->Data();
            Check(SourceShape(data), "InvalidCalibrationPlan", "/validation/identity", "one source, plan and sampling ledger");
            Check(SourceAxesMatch(value, data), "CalibrationContextMismatch", "/validation/identity", "unchanged source and plan");
            Check(SourceContextMatches(value, data), "CalibrationContextMismatch", "/validation/identity", "unchanged evaluated model context");
        }
        void RequireReportedRanges(const XccyMcResult_& value) {
            const auto& sampling = *value.report_->Data().execution_.sampling_;
            const auto& ranges = value.reservation_.ranges_;
            Check(sampling.firstPaths_.size() == ranges.size() && sampling.pathCounts_.size() == ranges.size() &&
                      sampling.batches_ == static_cast<int>(ranges.size()),
                  "CalibrationContextMismatch", "/validation/ranges", "same owned report batch and range counts");
            for (size_t i = 0; i < ranges.size(); ++i) {
                const size_t count = ranges[i].pathEnd_ - ranges[i].firstPath_;
                Check(sampling.firstPaths_[i] == static_cast<double>(ranges[i].firstPath_) && sampling.pathCounts_[i] == static_cast<double>(count),
                      "CalibrationContextMismatch", "/validation/ranges", "same owned report phase prefix");
            }
        }
        void RequireMapping(const XccyMcResult_& value) {
            const auto& mapping = value.mapping_;
            Check(Bytes(mapping.identity_) == Bytes(XccyMcInternal::MappingIdentity(mapping)) &&
                      Bytes(value.reservation_.mappingIdentity_) == Bytes(mapping.identity_) &&
                      Bytes(value.estimate_.mappingIdentity_) == Bytes(mapping.identity_),
                  "CalibrationContextMismatch", "/sampling/mapping", "same complete mapping and estimator identity");
            Check(value.reservation_.dimension_ == mapping.dimension_ && value.reservation_.seed_ == mapping.seed_, "CalibrationContextMismatch",
                  "/sampling/mapping", "same reservation dimension and seed");
            const auto& sampling = *value.report_->Data().execution_.sampling_;
            Check(Bytes(sampling.mappingIdentity_) == Bytes(mapping.identity_) && sampling.seed_ == mapping.seed_, "CalibrationContextMismatch",
                  "/sampling/mapping", "same owned report mapping and seed");
        }
        void RequireRanges(const XccyMcResult_& value) {
            const auto& ranges = value.reservation_.ranges_;
            const auto& estimate = value.estimate_;
            Check(estimate.ranges_.size() == ranges.size() && estimate.batches_.size() == ranges.size(), "CalibrationContextMismatch",
                  "/validation/ranges", "one estimator batch per reserved range");
            size_t paths = 0, coordinates = 0;
            for (size_t i = 0; i < ranges.size(); ++i) {
                const size_t count = ranges[i].pathEnd_ - ranges[i].firstPath_;
                Check(estimate.ranges_[i].firstPath_ == ranges[i].firstPath_ && estimate.ranges_[i].pathCount_ == count, "CalibrationContextMismatch",
                      "/validation/ranges", "actual estimator ranges equal checked phase ledger");
                paths += count;
                coordinates += ranges[i].coordinateEnd_ - ranges[i].firstCoordinate_;
            }
            Check(estimate.pathsPerView_ == paths && value.consumedCoordinates_ == coordinates && estimate.engineRuns_ == 2 * ranges.size(),
                  "CalibrationContextMismatch", "/validation/ranges", "actual path, coordinate and engine counts equal checked ledger");
        }
        void RequireValidationSource(const XccyMcResult_& value) {
            Check(value.reservation_.phase_ == XccyMonteCarloPhase_::Value_::Validation && value.estimate_.validation_ && !value.estimate_.heldOut_,
                  "InvalidCalibrationPlan", "/validation", "actual Validation phase and matching estimator flags");
            Check(Bytes(value.version_) == "dal.xccy-gaussian-mc-result/1" && Bytes(value.ruleId_) == "dal.xccy-gaussian-script-mc/1",
                  "InvalidCalibrationPlan", "/validation/identity", "XCCY result and execution rule version 1");
            RequireXccyMcIndependent(value.reservation_, value.reservation_, true);
            RequireReport(value);
            RequireMapping(value);
            RequireRanges(value);
            RequireReportedRanges(value);
        }
        [[noreturn]] void OwnValidationFailure(const XccyMcResult_& value, const CalibrationError_& error) {
            auto diagnostic = error.Failure().diagnostic_;
            diagnostic.stage_ = CalibrationStage_::Value_::Validate;
            diagnostic.measureId_ = value.estimate_.measureId_;
            const auto data = value.report_ ? value.report_->Data() : CalibrationReportSnapshot_{};
            if (!data.observations_.empty() && data.observations_[0].instrument_) {
                diagnostic.instrumentId_ = data.observations_[0].instrument_->Id();
            }
            const Handle_<CalibrationReport_> report(new CalibrationReport_(CalibrationStage_::Value_::Validate, {}, {diagnostic}, data));
            throw CalibrationError_(__FILE__, __LINE__, __func__,
                                    std::make_shared<const CalibrationFailure_>(CalibrationFailure_{diagnostic, report, error.Failure().candidate_}));
        }
    } // namespace
    XccyMcValidation_
    ValidateXccyMc(const XccyMcResult_& value, const CalibrationMcReference_& reference, const CalibrationMcPrecision_& precision) try {
        using namespace CalibrationInternal;
        RequireValidationSource(value);
        Check(reference.numericalError_ <= .01 * precision.errorScale_, "InvalidCalibrationPlan", "/reference/numerical_error",
              "independently measured value indicator within one percent of frozen scale");
        const auto& comparison = value.estimate_;
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
    } catch (const CalibrationError_& error) {
        OwnValidationFailure(value, error);
    }
} // namespace Dal

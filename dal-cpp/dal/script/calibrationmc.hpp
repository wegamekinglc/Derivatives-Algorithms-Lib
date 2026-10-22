#pragma once

#include <dal/script/calibrationpreparation.hpp>
#include <dal/script/settings.hpp>

namespace Dal {
    struct GSRCalibrationParameter_;
    Vector_<String_> CalibrationMcGLabels(const Handle_<ModelData_>& model, const Vector_<GSRCalibrationParameter_>& coordinates);
    Vector_<size_t> CalibrationMcParameterColumns(const Vector_<String_>& runtimeLabels,
                                                  const Vector_<String_>& requestedLabels,
                                                  const CalibrationDiagnostic_& diagnostic = {});

    struct CalibrationMcSettings_ {
        bool allowMonteCarloCalibration_ = false;
        bool aad_ = false, compiled_ = Script::DEFAULT_COMPILED, retainPaths_ = false;
        int seed_ = 1024;
        int fitBatches_ = 0, validationBatches_ = 0;
        int fitPathsPerBatch_ = 0, validationPathsPerBatch_ = 0;
        size_t maxPathsPerView_ = 0;
        double annuitySigma_ = 3.;
        Vector_<String_> parameterLabels_;
        int heldOutBatches_ = 0, heldOutPathsPerBatch_ = 0;
    };
    struct CalibrationMcBatch_ {
        size_t firstPath_ = 0, pathCount_ = 0;
    };
    struct CalibrationMcMapping_ {
        Vector_<> timeline_;
        Vector_<String_> normalCoordinates_;
        size_t dimension_ = 0;
        String_ identity_;
        String_ streamVersion_ = "dal.mrg32-normal-paths/1";
        int seed_ = 0;
        String_ generator_ = "mrg32", normalPrecision_ = "Precise", numeraire_ = "dal.native-gsr/1";
        bool brownianBridge_ = false;
    };
    struct CalibrationMcBatchValue_ {
        double value_ = 0.;
        Vector_<> derivatives_;
        std::optional<CalibrationAffineCoefficients_> coefficients_;
    };
    struct CalibrationMcAffineEstimate_ {
        int annuitySign_ = 0;
        double minAbsAnnuity_ = 0.;
        CalibrationAffineCoefficients_ meanCoefficients_;
        Matrix_<> coefficientCovariance_;
        CalibrationMeasureValue_ value_;
        double standardError_ = 0., annuityStandardError_ = 0.;
        Vector_<> gradientStandardError_;
    };
    struct CalibrationMcResult_ {
        String_ instrumentIdentity_, contextIdentity_, mappingIdentity_, measureId_, planIdentity_;
        bool validation_ = false;
        Vector_<String_> parameterLabels_;
        Vector_<CalibrationMcBatch_> ranges_;
        Vector_<CalibrationMcBatchValue_> batches_;
        double mean_ = 0., standardError_ = 0.;
        Vector_<> gradient_, gradientStandardError_;
        Matrix_<> quoteJacobian_;
        Vector_<> pathValues_;
        Matrix_<> pathDerivatives_;
        std::optional<CalibrationMcAffineEstimate_> affine_;
        size_t pathsPerView_ = 0, engineRuns_ = 0;
        size_t peakTapeNodes_ = 0, peakTapeBytes_ = 0;
        bool heldOut_ = false;
    };
    struct CalibrationMcReference_ {
        double value_ = 0., standardError_ = 0., numericalError_ = 0.;
        Vector_<> gradient_, gradientStandardError_, finiteDifferenceError_;
    };
    struct CalibrationMcPrecision_ {
        double errorScale_ = 0., sigma_ = 3., numericalErrorFraction_ = 0.25, epsilonValue_ = 1e-10;
        double gradientErrorFraction_ = 0.25;
        Vector_<> parameterScales_, epsilonGradient_;
    };
    struct CalibrationMcValidation_ {
        bool parityPassed_ = false, precisionPassed_ = false, valueErrorPassed_ = false;
        bool gradientValidationRequested_ = false, gradientParityPassed_ = false, gradientPrecisionPassed_ = false;
        bool numericalValidationPassed_ = false, budgetExhausted_ = false;
        double deltaStandardError_ = 0., precisionIndicator_ = 0., errorIndicator_ = 0.;
        Vector_<> gradientDeltaStandardError_, gradientPrecisionIndicators_;
        String_ terminationReason_;
        Handle_<CalibrationReport_> report_;
    };
    CalibrationMcValidation_
    ValidateCalibrationMc(const CalibrationMcResult_& validation, const CalibrationMcReference_& reference, const CalibrationMcPrecision_& precision);
    CalibrationMcValidation_
    ValidateCalibrationMc(const CalibrationMcResult_& fit, const CalibrationMcResult_& validation, const CalibrationMcPrecision_& precision);
    class PreparedCalibrationMc_ {
        struct Impl_;
        std::shared_ptr<const Impl_> impl_;
        explicit PreparedCalibrationMc_(std::shared_ptr<const Impl_> impl) : impl_(std::move(impl)) {}
        [[nodiscard]] CalibrationMcResult_ EvaluateRanges(const Handle_<CalibrationInstrument_>& instrument,
                                                          const CalibrationPreparationContext_& context,
                                                          bool validation,
                                                          bool heldOut) const;
        friend PreparedCalibrationMc_ PrepareCalibrationMc(const Handle_<CalibrationInstrument_>&,
                                                           const String_&,
                                                           const CalibrationPreparationContext_&,
                                                           const CalibrationMcSettings_&);

    public:
        [[nodiscard]] const CalibrationPreparationCapability_& Capability() const;
        [[nodiscard]] const CalibrationMcMapping_& Mapping() const;
        [[nodiscard]] const String_& PlanIdentity() const;
        [[nodiscard]] const Vector_<CalibrationMcBatch_>& Batches(bool validation) const;
        [[nodiscard]] PreparedCalibrationMc_ RefreshG(const CalibrationPreparationContext_& context) const;
        [[nodiscard]] CalibrationMcResult_
        Evaluate(const Handle_<CalibrationInstrument_>& instrument, const CalibrationPreparationContext_& context, bool validation) const;
        [[nodiscard]] CalibrationMcResult_ EvaluateHeldOut(const Handle_<CalibrationInstrument_>& instrument,
                                                           const CalibrationPreparationContext_& context) const;
    };
    PreparedCalibrationMc_ PrepareCalibrationMc(const Handle_<CalibrationInstrument_>& instrument,
                                                const String_& measureId,
                                                const CalibrationPreparationContext_& context,
                                                const CalibrationMcSettings_& settings);
} // namespace Dal

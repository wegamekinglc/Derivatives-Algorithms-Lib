#pragma once

#include <dal/calibration/types.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/storage/storable.hpp>

namespace Dal {
    class CalibrationInstrument_;
    class CalibrationReport_;

    class CalibrationMarketQuote_ final {
        String_ id_, fingerprint_;
        double value_;
        CalibrationValueUnit_ unit_;
        CalibrationQuoteConvention_ convention_;
        CalibrationMarketQuote_(String_ id, double value, CalibrationValueUnit_ unit, CalibrationQuoteConvention_ convention);
        friend Handle_<CalibrationMarketQuote_>
        NewCalibrationMarketQuote(const String_&, double, const CalibrationValueUnit_&, const std::optional<CalibrationQuoteConvention_>&);

    public:
        [[nodiscard]] const String_& Id() const { return id_; }
        [[nodiscard]] double Value() const { return value_; }
        [[nodiscard]] const CalibrationValueUnit_& Unit() const { return unit_; }
        [[nodiscard]] const CalibrationQuoteConvention_& Convention() const { return convention_; }
        [[nodiscard]] const String_& Fingerprint() const { return fingerprint_; }
    };
    class CalibrationObservation_ final {
        String_ id_, measureId_;
        Handle_<CalibrationInstrument_> instrument_;
        Handle_<CalibrationMarketQuote_> quote_;
        double errorScale_;
        CalibrationObservation_(
            String_ id, Handle_<CalibrationInstrument_> instrument, String_ measureId, Handle_<CalibrationMarketQuote_> quote, double errorScale);
        friend Handle_<CalibrationObservation_> NewCalibrationObservation(
            const String_&, const Handle_<CalibrationInstrument_>&, const String_&, const Handle_<CalibrationMarketQuote_>&, double);

    public:
        [[nodiscard]] const String_& Id() const { return id_; }
        [[nodiscard]] const String_& MeasureId() const { return measureId_; }
        [[nodiscard]] const Handle_<CalibrationInstrument_>& Instrument() const { return instrument_; }
        [[nodiscard]] const Handle_<CalibrationMarketQuote_>& Quote() const { return quote_; }
        [[nodiscard]] double ErrorScale() const { return errorScale_; }
    };
    struct CalibrationObservationAxes_ {
        Vector_<Handle_<CalibrationObservation_>> observations_;
        Vector_<Handle_<CalibrationMarketQuote_>> quotes_;
        Vector_<int> quoteIndex_;
        Matrix_<> quoteMap_;
    };
    struct CalibrationReportSummary_ {
        String_ domain_, status_;
        std::optional<String_> mode_, terminationReason_, contextIdentity_, compilerIdentity_;
        std::optional<bool> converged_, fitWithinTolerance_, numericalValidationPassed_;
        std::optional<double> rawL2_, scaledL2_, scaledInfinity_, fitWeight_, fitNormTolerance_;
        std::optional<double> objective_, dataObjective_, penaltyObjective_, jacobianConditionEstimate_;
        std::optional<int> iterations_, evaluations_, jacobianEvaluations_, jacobianRank_;
    };
    struct CalibrationReportObservation_ {
        Handle_<CalibrationInstrument_> instrument_;
        std::optional<String_> observationId_, quoteId_;
        std::optional<int> quoteIndex_;
        String_ measureId_, representation_, unit_, backend_, derivativeMethod_, planIdentity_;
        std::optional<double> target_, errorScale_, modelValue_, rawResidual_, scaledResidual_;
        std::optional<bool> fitRowPassed_;
        std::optional<double> validationValue_, validationError_, standardError_;
    };
    struct CalibrationReportParameter_ {
        String_ id_, unit_;
        std::optional<double> initial_, value_, scale_, lower_, upper_;
        std::optional<String_> activeBound_;
    };
    struct CalibrationReportPlan_ {
        std::optional<String_> observationId_;
        String_ measureId_, ruleId_, backend_, derivativeMethod_;
        bool admitted_ = true;
        std::optional<String_> reasonCode_, reason_, sourceIdentity_, definitionIdentity_, bindingIdentity_;
        std::optional<String_> compilerIdentity_, contextIdentity_, planIdentity_;
    };
    struct CalibrationReportObjective_ {
        Vector_<> theta0_, parameterScales_;
        Vector_<String_> nativeLayout_;
        std::optional<Matrix_<>> weightMatrix_;
        String_ weightProvenance_;
        std::optional<double> lambda_, eta_;
    };
    struct CalibrationReportSampling_ {
        String_ mode_, streamVersion_, mappingIdentity_, generator_, normalPrecision_;
        int seed_ = 0, batches_ = 0, pathsPerBatch_ = 0;
        Vector_<double> firstPaths_, pathCounts_;
    };
    struct CalibrationReportSimulation_ {
        bool useBb_ = false, compiled_ = true;
        double smooth_ = 0.;
        String_ normalPrecision_;
    };
    struct CalibrationReportExecution_ {
        std::optional<Date_> valuationDate_;
        std::optional<String_> todayFixingPolicy_, fixingSnapshotIdentity_, modelIdentity_, modelTimeBasis_;
        std::optional<String_> numeraireConvention_, conventionStateIdentity_;
        Vector_<String_> routeIdentities_;
        std::optional<CalibrationReportSimulation_> simulation_;
        std::optional<CalibrationReportSampling_> sampling_, validationSampling_;
    };
    struct CalibrationReportValidation_ {
        std::optional<String_> observationId_;
        String_ edge_, quantity_, reason_;
        std::optional<double> value_, difference_, standardError_, absoluteTolerance_, sigmaMultiplier_;
        std::optional<bool> accepted_;
    };
    struct CalibrationReportSnapshot_ {
        CalibrationReportSummary_ summary_;
        Vector_<CalibrationReportObservation_> observations_;
        Vector_<CalibrationReportParameter_> parameters_;
        Vector_<CalibrationReportPlan_> plans_;
        CalibrationReportObjective_ objective_;
        CalibrationReportExecution_ execution_;
        Vector_<CalibrationReportValidation_> validation_;
        std::optional<Matrix_<>> quoteJacobian_;
    };
    class CalibrationReport_ final : public Storable_ {
        CalibrationStage_ stage_;
        CalibrationObservationAxes_ axes_;
        Vector_<CalibrationDiagnostic_> errors_;
        CalibrationReportSnapshot_ data_;

    public:
        explicit CalibrationReport_(CalibrationStage_ stage, CalibrationObservationAxes_ axes = {}, Vector_<CalibrationDiagnostic_> errors = {});
        CalibrationReport_(CalibrationStage_ stage,
                           CalibrationObservationAxes_ axes,
                           Vector_<CalibrationDiagnostic_> errors,
                           const CalibrationReportSnapshot_& data);
        [[nodiscard]] const char* Schema() const { return "dal.calibration-report/1"; }
        [[nodiscard]] CalibrationStage_ Stage() const { return stage_; }
        [[nodiscard]] const CalibrationObservationAxes_& Axes() const { return axes_; }
        [[nodiscard]] const Vector_<CalibrationDiagnostic_>& Errors() const { return errors_; }
        [[nodiscard]] const CalibrationReportSnapshot_& Data() const { return data_; }
        [[nodiscard]] bool QuoteRiskAvailable() const { return false; }
        void Write(Archive::Store_& dst) const override;
    };
    struct CalibrationFailure_ {
        CalibrationDiagnostic_ diagnostic_;
        Handle_<CalibrationReport_> report_;
        Handle_<Storable_> candidate_;
    };
    class BASE_EXPORT CalibrationError_ : public Exception_ {
        std::shared_ptr<const CalibrationFailure_> failure_;

    public:
        CalibrationError_(const std::string& file, long line, const std::string& function, std::shared_ptr<const CalibrationFailure_> failure);
        [[nodiscard]] const CalibrationFailure_& Failure() const noexcept { return *failure_; }
        [[nodiscard]] const char* what() const noexcept override { return failure_->diagnostic_.message_.c_str(); }
    };
    Handle_<CalibrationMarketQuote_> NewCalibrationMarketQuote(const String_& id,
                                                               double value,
                                                               const CalibrationValueUnit_& unit,
                                                               const std::optional<CalibrationQuoteConvention_>& convention = std::nullopt);
    Handle_<CalibrationObservation_> NewCalibrationObservation(const String_& id,
                                                               const Handle_<CalibrationInstrument_>& instrument,
                                                               const String_& measureId,
                                                               const Handle_<CalibrationMarketQuote_>& quote,
                                                               double errorScale);
    CalibrationObservationAxes_ BuildCalibrationObservationAxes(const Vector_<Handle_<CalibrationObservation_>>& observations);
} // namespace Dal

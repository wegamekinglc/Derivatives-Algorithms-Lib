#pragma once

#include <limits>
#include <memory>
#include <optional>

#include <dal/curve/curveinstrumentcalibration.hpp>

namespace Dal {
    class ScriptCurveQuoteRiskSnapshot_;
    class ScriptCurveQuoteRiskResult_;
    struct ScriptCurveQuoteRiskRequest_ {
        Vector_<String_> quoteIds_;
        String_ measureId_ = "Price";
        bool liveQuotePayoff_ = false;
    };
    ScriptCurveQuoteRiskResult_ ValueCalibrationInstrumentWithQuoteRisk(const Handle_<CalibrationInstrument_>& instrument,
                                                                        const ScriptCurveQuoteRiskSnapshot_& snapshot,
                                                                        const CurveInstrumentCalibrationSpec_& context,
                                                                        const ScriptCurveQuoteRiskRequest_& request = {});
    struct ScriptCurveQuoteRiskAccuracy_ {
        Vector_<> residualAbsoluteLimits_;
        double minReciprocalCondition_ = 0.;
        double responseBackwardErrorLimit_ = 0.;
        double relativePivotTolerance_ = 64. * std::numeric_limits<double>::epsilon();
    };
    struct ScriptCurveQuoteRiskQuote_ {
        int ordinal_ = 0;
        String_ id_;
        double value_ = 0.;
        CalibrationValueUnit_ unit_;
        CalibrationQuoteConvention_ convention_;
    };
    struct ScriptCurveQuoteRiskObservation_ {
        int ordinal_ = 0, quoteOrdinal_ = 0;
        String_ id_, instrumentId_, measureId_, sourceIdentity_, definitionIdentity_, bindingIdentity_;
        double errorScale_ = 0.;
    };
    struct ScriptCurveQuoteRiskParameter_ {
        int ordinal_ = 0;
        String_ id_, unit_;
        CurveFreeParameter_ coordinate_;
        std::optional<double> value_;
    };
    struct ScriptCurveQuoteRiskAxes_ {
        Vector_<ScriptCurveQuoteRiskQuote_> quotes_;
        Vector_<ScriptCurveQuoteRiskObservation_> observations_;
        Vector_<ScriptCurveQuoteRiskParameter_> parameters_;
    };
    struct ScriptCurveQuoteRiskNumerics_ {
        Vector_<> modelMeasures_, rawResiduals_;
        Matrix_<> nativeJacobian_;
        std::optional<double> reciprocalCondition_;
        Vector_<> transposeBackwardErrors_, responseBackwardErrors_;
        int factorizationCount_ = 0;
    };
    class ScriptCurveQuoteRiskResult_ {
        ScriptCurveQuoteRiskAxes_ axes_;
        ScriptCurveQuoteRiskNumerics_ numerics_;
        Vector_<CalibrationDiagnostic_> diagnostics_;
        std::optional<double> pv_;
        Ccy_ currency_;
        Vector_<> nativeGradient_, raw_, dv01_;
        String_ snapshotIdentity_, contextIdentity_, sourceIdentity_, definitionIdentity_, bindingIdentity_;
        friend ScriptCurveQuoteRiskResult_ ValueCalibrationInstrumentWithQuoteRisk(const Handle_<CalibrationInstrument_>&,
                                                                                   const ScriptCurveQuoteRiskSnapshot_&,
                                                                                   const CurveInstrumentCalibrationSpec_&,
                                                                                   const ScriptCurveQuoteRiskRequest_&);

    public:
        [[nodiscard]] bool Available() const { return pv_.has_value(); }
        [[nodiscard]] const ScriptCurveQuoteRiskAxes_& Axes() const { return axes_; }
        [[nodiscard]] const ScriptCurveQuoteRiskNumerics_& Numerics() const { return numerics_; }
        [[nodiscard]] const Vector_<CalibrationDiagnostic_>& Diagnostics() const { return diagnostics_; }
        [[nodiscard]] double PV() const;
        [[nodiscard]] const Vector_<>& NativeGradient() const;
        [[nodiscard]] const Vector_<>& RawSensitivity() const;
        [[nodiscard]] const Vector_<>& DV01() const;
        [[nodiscard]] const Ccy_& Currency() const { return currency_; }
        [[nodiscard]] String_ Method() const { return "NativeAADImplicitTranspose"; }
        [[nodiscard]] const String_& SnapshotIdentity() const { return snapshotIdentity_; }
        [[nodiscard]] const String_& ContextIdentity() const { return contextIdentity_; }
        [[nodiscard]] const String_& SourceIdentity() const { return sourceIdentity_; }
        [[nodiscard]] const String_& DefinitionIdentity() const { return definitionIdentity_; }
        [[nodiscard]] const String_& BindingIdentity() const { return bindingIdentity_; }
    };
    class ScriptCurveQuoteMapping_ {
        std::shared_ptr<const ScriptCurveQuoteRiskAxes_> axes_;
        ScriptCurveQuoteRiskNumerics_ numerics_;
        Vector_<CalibrationDiagnostic_> diagnostics_;
        std::optional<Matrix_<>> response_;
        ScriptCurveQuoteMapping_(std::shared_ptr<const ScriptCurveQuoteRiskAxes_> axes,
                                 ScriptCurveQuoteRiskNumerics_ numerics,
                                 Vector_<CalibrationDiagnostic_> diagnostics,
                                 std::optional<Matrix_<>> response);
        friend class ScriptCurveQuoteRiskSnapshot_;

    public:
        [[nodiscard]] bool Available() const { return response_.has_value(); }
        [[nodiscard]] const ScriptCurveQuoteRiskAxes_& Axes() const { return *axes_; }
        [[nodiscard]] const ScriptCurveQuoteRiskNumerics_& Numerics() const { return numerics_; }
        [[nodiscard]] const Vector_<CalibrationDiagnostic_>& Diagnostics() const { return diagnostics_; }
        [[nodiscard]] const Matrix_<>& ParameterResponse() const;
    };
    class ScriptCurveQuoteRiskValidation_ {
        bool passed_;
        std::shared_ptr<const ScriptCurveQuoteRiskAxes_> axes_;
        Vector_<CalibrationDiagnostic_> diagnostics_;
        ScriptCurveQuoteRiskValidation_(bool passed,
                                        std::shared_ptr<const ScriptCurveQuoteRiskAxes_> axes,
                                        Vector_<CalibrationDiagnostic_> diagnostics);
        friend class ScriptCurveQuoteRiskSnapshot_;

    public:
        [[nodiscard]] bool Passed() const { return passed_; }
        [[nodiscard]] const ScriptCurveQuoteRiskAxes_& Axes() const { return *axes_; }
        [[nodiscard]] const Vector_<CalibrationDiagnostic_>& Diagnostics() const { return diagnostics_; }
    };
    class ScriptCurveQuoteRiskSnapshot_ {
    public:
        struct Data_;

    private:
        std::shared_ptr<const Data_> data_;
        explicit ScriptCurveQuoteRiskSnapshot_(std::shared_ptr<const Data_> data);
        friend ScriptCurveQuoteRiskSnapshot_ NewScriptCurveQuoteRiskSnapshot(const CurveInstrumentCalibrationResult_&,
                                                                             const ScriptCurveQuoteRiskAccuracy_&);
        friend ScriptCurveQuoteRiskResult_ ValueCalibrationInstrumentWithQuoteRisk(const Handle_<CalibrationInstrument_>&,
                                                                                   const ScriptCurveQuoteRiskSnapshot_&,
                                                                                   const CurveInstrumentCalibrationSpec_&,
                                                                                   const ScriptCurveQuoteRiskRequest_&);

    public:
        [[nodiscard]] bool StructureValid() const;
        [[nodiscard]] bool MappingAvailable() const;
        [[nodiscard]] bool RiskAvailable() const { return MappingAvailable(); }
        [[nodiscard]] const ScriptCurveQuoteRiskNumerics_& Numerics() const;
        [[nodiscard]] const Vector_<CalibrationDiagnostic_>& Diagnostics() const;
        [[nodiscard]] const ScriptCurveQuoteRiskAccuracy_& Accuracy() const;
        [[nodiscard]] const Vector_<ScriptCurveQuoteRiskQuote_>& QuoteAxis() const;
        [[nodiscard]] const Vector_<ScriptCurveQuoteRiskObservation_>& ObservationAxis() const;
        [[nodiscard]] const Vector_<ScriptCurveQuoteRiskParameter_>& ParameterAxis() const;
        [[nodiscard]] const std::shared_ptr<const CurveInstrumentCalibrationSpec_>& ResolvedSpec() const;
        [[nodiscard]] const Vector_<>& Parameters() const;
        [[nodiscard]] const String_& ContextIdentity() const;
        [[nodiscard]] const String_& Identity() const;
        [[nodiscard]] ScriptCurveQuoteRiskValidation_ ValidateContext(const CurveInstrumentCalibrationSpec_& context) const;
        [[nodiscard]] ScriptCurveQuoteMapping_ ParameterQuoteMapping(const CurveInstrumentCalibrationSpec_& context) const;
    };
    ScriptCurveQuoteRiskSnapshot_ NewScriptCurveQuoteRiskSnapshot(const CurveInstrumentCalibrationResult_& fit,
                                                                  const ScriptCurveQuoteRiskAccuracy_& accuracy);
} // namespace Dal

#pragma once

#include <limits>
#include <memory>
#include <optional>

#include <dal/curve/curveinstrumentcalibration.hpp>

namespace Dal {
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

    public:
        [[nodiscard]] bool StructureValid() const;
        [[nodiscard]] bool RiskAvailable() const { return false; }
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
    };
    ScriptCurveQuoteRiskSnapshot_ NewScriptCurveQuoteRiskSnapshot(const CurveInstrumentCalibrationResult_& fit,
                                                                  const ScriptCurveQuoteRiskAccuracy_& accuracy);
} // namespace Dal

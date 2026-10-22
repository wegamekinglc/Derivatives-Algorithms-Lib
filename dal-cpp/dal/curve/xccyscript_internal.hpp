#pragma once

#include <dal/calibration/validation_internal.hpp>
#include <dal/curve/xccyscript.hpp>

namespace Dal::Script {
    struct NodePays_;
} // namespace Dal::Script

namespace Dal::XccyScriptInternal {
    Vector_<ScriptSourceRow_>
    ExportRows(const XccyCashflowPlan_& plan, double coupon, bool receiveNonSpreadPaySpread, const CalibrationDiagnostic_& diagnostic);
    CalibrationXccy_ Lower(const CalibrationInstrument_& instrument,
                           const XccyScriptContext_& context,
                           const Vector_<const Script::NodePays_*>& payments,
                           const Vector_<CalibrationDiagnostic_>& sources);
    String_ ContextIdentity(const XccyScriptContext_& context, const MarketFixingSnapshot_* captured, const CalibrationDiagnostic_& diagnostic);
    String_ ContextIdentity(const CalibrationPreparationContext_& context, const CalibrationDiagnostic_& diagnostic);
    CalibrationPreparationCapability_ Admit(const CalibrationEvaluationOptions_& options, const CalibrationDiagnostic_& diagnostic);
} // namespace Dal::XccyScriptInternal

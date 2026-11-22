#pragma once

#include <dal/curve/scriptcurvequoterisk.hpp>

namespace Dal {
    CompoundedOisParRateRiskResult_ ValueCompoundedOisParRateWithQuoteRisk(const Handle_<CalibrationInstrument_>& instrument,
                                                                           const Handle_<PreparedCompoundedOis_>& tables,
                                                                           const ScriptCurveQuoteRiskSnapshot_& snapshot,
                                                                           const CurveInstrumentCalibrationSpec_& context,
                                                                           const CompoundedOisParRateRequest_& request = {});
    Matrix_<Cell_> ScriptCurveQuoteRiskTable(const CompoundedOisParRateRiskResult_& result, const String_& table);
    Matrix_<Cell_> ScriptCurveQuoteRiskTable(const ScriptCurveQuoteRiskSnapshot_& snapshot, const String_& table);
    Matrix_<Cell_> ScriptCurveQuoteRiskTable(const ScriptCurveQuoteRiskResult_& result, const String_& table);
} // namespace Dal

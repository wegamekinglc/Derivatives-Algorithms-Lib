#pragma once

#include <dal-public/src/scriptcurvequoterisk.hpp>

#include "__calibrationinstrument.hpp"

#if defined(_WIN32) && defined(DAL_EXCEL_TEST_API_EXPORTS)
#define DAL_SCRIPT_QUOTE_RISK_API __declspec(dllexport)
#elif defined(_WIN32) && defined(DAL_EXCEL_TEST_API_IMPORTS)
#define DAL_SCRIPT_QUOTE_RISK_API __declspec(dllimport)
#else
#define DAL_SCRIPT_QUOTE_RISK_API
#endif

namespace Dal {
    using StorableScriptCurveQuoteRiskAccuracy_ = StorableCalibrationValue_<ScriptCurveQuoteRiskAccuracy_>;
    using StorableScriptCurveQuoteRiskRequest_ = StorableCalibrationValue_<ScriptCurveQuoteRiskRequest_>;
    using StorableScriptCurveQuoteIds_ = StorableCalibrationValue_<Vector_<String_>>;
    using StorableScriptCurveQuoteRiskSnapshot_ = StorableCalibrationValue_<ScriptCurveQuoteRiskSnapshot_>;
    using StorableScriptCurveQuoteRiskResult_ = StorableCalibrationValue_<ScriptCurveQuoteRiskResult_>;

    DAL_SCRIPT_QUOTE_RISK_API void ScriptCurveQuoteRiskAccuracy_New(const Matrix_<Cell_>& settings,
                                                                    Handle_<StorableScriptCurveQuoteRiskAccuracy_>* value);
    DAL_SCRIPT_QUOTE_RISK_API void ScriptCurveQuoteRiskRequest_New(const Matrix_<Cell_>& settings,
                                                                   Handle_<StorableScriptCurveQuoteRiskRequest_>* value);
    DAL_SCRIPT_QUOTE_RISK_API void ScriptCurveQuoteIds_New(const Vector_<Cell_>& ids, Handle_<StorableScriptCurveQuoteIds_>* value);
    DAL_SCRIPT_QUOTE_RISK_API void ScriptCurveQuoteRiskSnapshot_New(const Handle_<StorableCalibrationOutcome_>& calibration,
                                                                    const Handle_<StorableScriptCurveQuoteRiskAccuracy_>& accuracy,
                                                                    Handle_<StorableScriptCurveQuoteRiskSnapshot_>* value);
    DAL_SCRIPT_QUOTE_RISK_API void CalibrationInstrument_ValueWithQuoteRisk(const Handle_<CalibrationInstrument_>& instrument,
                                                                            const Handle_<StorableScriptCurveQuoteRiskSnapshot_>& snapshot,
                                                                            const Handle_<StorableCurveInstrumentCalibrationSpec_>& context,
                                                                            const Handle_<StorableScriptCurveQuoteRiskRequest_>& request,
                                                                            Handle_<StorableScriptCurveQuoteRiskResult_>* value);
    DAL_SCRIPT_QUOTE_RISK_API void ScriptCurveQuoteRiskSnapshot_Get_Status(const Handle_<StorableScriptCurveQuoteRiskSnapshot_>& snapshot,
                                                                           String_* value);
    DAL_SCRIPT_QUOTE_RISK_API void ScriptCurveQuoteRiskSnapshot_Get_Summary(const Handle_<StorableScriptCurveQuoteRiskSnapshot_>& snapshot,
                                                                            Matrix_<Cell_>* value);
    DAL_SCRIPT_QUOTE_RISK_API void ScriptCurveQuoteRiskSnapshot_Get_QuoteAxis(const Handle_<StorableScriptCurveQuoteRiskSnapshot_>& snapshot,
                                                                              Matrix_<Cell_>* value);
    DAL_SCRIPT_QUOTE_RISK_API void ScriptCurveQuoteRiskSnapshot_Get_Diagnostics(const Handle_<StorableScriptCurveQuoteRiskSnapshot_>& snapshot,
                                                                                Matrix_<Cell_>* value);
    DAL_SCRIPT_QUOTE_RISK_API void ScriptCurveQuoteRiskResult_Get_Status(const Handle_<StorableScriptCurveQuoteRiskResult_>& result, String_* value);
    DAL_SCRIPT_QUOTE_RISK_API void ScriptCurveQuoteRiskResult_Get_Summary(const Handle_<StorableScriptCurveQuoteRiskResult_>& result,
                                                                          Matrix_<Cell_>* value);
    DAL_SCRIPT_QUOTE_RISK_API void ScriptCurveQuoteRiskResult_Get_QuoteAxis(const Handle_<StorableScriptCurveQuoteRiskResult_>& result,
                                                                            Matrix_<Cell_>* value);
    DAL_SCRIPT_QUOTE_RISK_API void ScriptCurveQuoteRiskResult_Get_Diagnostics(const Handle_<StorableScriptCurveQuoteRiskResult_>& result,
                                                                              Matrix_<Cell_>* value);
    DAL_SCRIPT_QUOTE_RISK_API void ScriptCurveQuoteRiskResult_Get_Sensitivity(const Handle_<StorableScriptCurveQuoteRiskResult_>& result,
                                                                              Matrix_<Cell_>* value);
    DAL_SCRIPT_QUOTE_RISK_API void ScriptCurveQuoteRiskResult_Get_PV(const Handle_<StorableScriptCurveQuoteRiskResult_>& result, double* value);
    DAL_SCRIPT_QUOTE_RISK_API void ScriptCurveQuoteRiskResult_Get_RawSensitivity(const Handle_<StorableScriptCurveQuoteRiskResult_>& result,
                                                                                 Vector_<>* value);
    DAL_SCRIPT_QUOTE_RISK_API void ScriptCurveQuoteRiskResult_Get_DV01(const Handle_<StorableScriptCurveQuoteRiskResult_>& result, Vector_<>* value);
} // namespace Dal

#undef DAL_SCRIPT_QUOTE_RISK_API

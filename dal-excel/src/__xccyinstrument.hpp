#pragma once

#include "__calibrationinstrument.hpp"
#include <dal-public/src/xccycalibration.hpp>

#if defined(_WIN32) && defined(DAL_EXCEL_TEST_API_EXPORTS)
#define DAL_XCCY_INSTRUMENT_EXCEL_API __declspec(dllexport)
#elif defined(_WIN32) && defined(DAL_EXCEL_TEST_API_IMPORTS)
#define DAL_XCCY_INSTRUMENT_EXCEL_API __declspec(dllimport)
#else
#define DAL_XCCY_INSTRUMENT_EXCEL_API
#endif

namespace Dal {
    using StorableXccyScriptContext_ = StorableCalibrationValue_<XccyScriptContext_>;
    using StorableXccyScriptImportSettings_ = StorableCalibrationValue_<XccyScriptImportSettings_>;
    using StorableXccyInstrumentCalibrationSpec_ = StorableCalibrationValue_<XccyInstrumentCalibrationSpec_>;
    DAL_XCCY_INSTRUMENT_EXCEL_API void XccyScriptContext_New(const Matrix_<Cell_>& settings, Handle_<StorableXccyScriptContext_>* value);
    DAL_XCCY_INSTRUMENT_EXCEL_API void XccyScriptImportSettings_New(const Matrix_<Cell_>& settings,
                                                                    Handle_<StorableXccyScriptImportSettings_>* value);
    DAL_XCCY_INSTRUMENT_EXCEL_API void XccyInstrumentCalibrationSpec_New(const Matrix_<Cell_>& settings,
                                                                         Handle_<StorableXccyInstrumentCalibrationSpec_>* value);
    DAL_XCCY_INSTRUMENT_EXCEL_API void XccyScriptInstrument_New(const Handle_<StorableCrossCurrencySwap_>& native,
                                                                const Handle_<StorableXccyScriptImportSettings_>& settings,
                                                                Handle_<CalibrationInstrument_>* value);
    DAL_XCCY_INSTRUMENT_EXCEL_API void Calibrate_XccyInstruments(const Handle_<StorableXccyInstrumentCalibrationSpec_>& spec,
                                                                 const Handle_<StorableCurveInstrumentCalibrationOptions_>& options,
                                                                 Handle_<StorableCalibrationOutcome_>* outcome);
    DAL_XCCY_INSTRUMENT_EXCEL_API void CalibrationInstrument_Get_XccyCapabilities(const Handle_<CalibrationInstrument_>& instrument,
                                                                                  const String_& measureId,
                                                                                  const Handle_<StorableXccyScriptContext_>& context,
                                                                                  Handle_<CalibrationReport_>* report);
} // namespace Dal

#undef DAL_XCCY_INSTRUMENT_EXCEL_API

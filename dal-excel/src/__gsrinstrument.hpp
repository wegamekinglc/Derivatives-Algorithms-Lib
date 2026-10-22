#pragma once

#include "__calibrationinstrument.hpp"
#include <dal-public/src/gsr.hpp>

#if defined(_WIN32) && defined(DAL_EXCEL_TEST_API_EXPORTS)
#define DAL_GSR_INSTRUMENT_EXCEL_API __declspec(dllexport)
#elif defined(_WIN32) && defined(DAL_EXCEL_TEST_API_IMPORTS)
#define DAL_GSR_INSTRUMENT_EXCEL_API __declspec(dllimport)
#else
#define DAL_GSR_INSTRUMENT_EXCEL_API
#endif

namespace Dal {
    using StorableGSRInstrumentCalibrationSettings_ = StorableCalibrationValue_<GSRInstrumentCalibrationSettings_>;
    using StorableGSRCalibrationSolver_ = StorableCalibrationValue_<GSRCalibrationSettings_>;
    using StorableGSRPricingSettings_ = StorableCalibrationValue_<GSRPricingSettings_>;
    DAL_GSR_INSTRUMENT_EXCEL_API void GSRInstrumentCalibrationSettings_New(const Matrix_<Cell_>& settings,
                                                                           Handle_<StorableGSRInstrumentCalibrationSettings_>* value);
    DAL_GSR_INSTRUMENT_EXCEL_API void GSRCalibrationSolver_New(const Matrix_<Cell_>& settings, Handle_<StorableGSRCalibrationSolver_>* value);
    DAL_GSR_INSTRUMENT_EXCEL_API void GSRPricingSettings_New(const Matrix_<Cell_>& settings, Handle_<StorableGSRPricingSettings_>* value);
    DAL_GSR_INSTRUMENT_EXCEL_API void Calibrate_GSRInstruments(const Handle_<ModelData_>& initial,
                                                               const Vector_<Handle_<Storable_>>& observations,
                                                               const Matrix_<Cell_>& parameters,
                                                               const Handle_<StorableGSRInstrumentCalibrationSettings_>& settings,
                                                               Handle_<StorableCalibrationOutcome_>* outcome);
    DAL_GSR_INSTRUMENT_EXCEL_API void CalibrationOutcome_Get_Model(const Handle_<StorableCalibrationOutcome_>& outcome, Handle_<ModelData_>* model);
} // namespace Dal

#undef DAL_GSR_INSTRUMENT_EXCEL_API

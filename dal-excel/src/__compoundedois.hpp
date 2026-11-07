#pragma once

#include "__calibrationinstrument.hpp"

#if defined(_WIN32) && defined(DAL_EXCEL_TEST_API_EXPORTS)
#define DAL_OIS_EXCEL_API __declspec(dllexport)
#elif defined(_WIN32) && defined(DAL_EXCEL_TEST_API_IMPORTS)
#define DAL_OIS_EXCEL_API __declspec(dllimport)
#else
#define DAL_OIS_EXCEL_API
#endif

namespace Dal {
    using StorableCompoundedOisFixedLeg_ = StorableCalibrationValue_<Vector_<CompoundedOisFixedRow_>>;
    using StorableCompoundedOisCoupons_ = StorableCalibrationValue_<Vector_<CompoundedOisCouponRow_>>;
    using StorableCompoundedOisObservations_ = StorableCalibrationValue_<Vector_<CompoundedOisObservationRow_>>;
    using StorableCompoundedOisCalendar_ = StorableCalibrationValue_<HolidayCenterData_>;
    using StorableCompoundedOisScriptSpec_ = StorableCalibrationValue_<CompoundedOisScriptSpec_>;
    using StorablePreparedCompoundedOis_ = StorableCalibrationValue_<Handle_<PreparedCompoundedOis_>>;
    using StorableCompoundedOisPreparations_ = StorableCalibrationValue_<Vector_<Handle_<PreparedCompoundedOis_>>>;
    using StorableCurveScriptEvaluationRequest_ = StorableCalibrationValue_<CurveScriptEvaluationRequest_>;
    using StorableCalibrationOvernightCurves_ = StorableCalibrationValue_<std::map<std::string, Handle_<DiscountCurve_>>>;
    using StorableCalibrationStrings_ = StorableCalibrationValue_<Vector_<String_>>;
    DAL_OIS_EXCEL_API void CompoundedOisFixedLeg_New(const Matrix_<Cell_>& rows, Handle_<StorableCompoundedOisFixedLeg_>* value);
    DAL_OIS_EXCEL_API void CompoundedOisCoupons_New(const Matrix_<Cell_>& rows, Handle_<StorableCompoundedOisCoupons_>* value);
    DAL_OIS_EXCEL_API void CompoundedOisObservations_New(const Matrix_<Cell_>& rows, Handle_<StorableCompoundedOisObservations_>* value);
    DAL_OIS_EXCEL_API void CurveScriptOvernightRoutes_New(const Matrix_<Cell_>& routes, Handle_<StorableCurveScriptRoutes_>* value);
    DAL_OIS_EXCEL_API void CalibrationOvernightCurves_New(const Matrix_<Cell_>& bindings, Handle_<StorableCalibrationOvernightCurves_>* value);
    DAL_OIS_EXCEL_API void CalibrationStrings_New(const Matrix_<Cell_>& rows, Handle_<StorableCalibrationStrings_>* value);
    DAL_OIS_EXCEL_API void CompoundedOisCalendar_New(const Matrix_<Cell_>& settings, Handle_<StorableCompoundedOisCalendar_>* value);
    DAL_OIS_EXCEL_API void CompoundedOisScriptSpec_New(const Matrix_<Cell_>& settings, Handle_<StorableCompoundedOisScriptSpec_>* value);
    DAL_OIS_EXCEL_API void CurveScriptEvaluationRequest_New(const Matrix_<Cell_>& settings, Handle_<StorableCurveScriptEvaluationRequest_>* value);
    DAL_OIS_EXCEL_API void CompoundedOis_Prepare(const Handle_<StorableCompoundedOisScriptSpec_>& spec,
                                                 const Handle_<StorableScriptValuationSettings_>& valuation,
                                                 Handle_<StorablePreparedCompoundedOis_>* value);
    DAL_OIS_EXCEL_API void CompoundedOisPreparations_New(const Vector_<Handle_<Storable_>>& tables,
                                                         Handle_<StorableCompoundedOisPreparations_>* value);
    DAL_OIS_EXCEL_API void CompoundedOisScriptInstrument_New(const Handle_<StorablePreparedCompoundedOis_>& tables,
                                                             Handle_<CalibrationInstrument_>* instrument);
    DAL_OIS_EXCEL_API void CurveScriptInstrument_Evaluate(const Handle_<CalibrationInstrument_>& instrument,
                                                          const Handle_<StorableCurveScriptEvaluationRequest_>& request,
                                                          Handle_<StorableCalibrationOutcome_>* outcome);
    DAL_OIS_EXCEL_API void
    CurveScriptEvaluation_Get_Value(const Handle_<StorableCalibrationOutcome_>& outcome, const String_& measureId, double* value);
    DAL_OIS_EXCEL_API void CurveScriptEvaluation_Get_NativePartials(const Handle_<StorableCalibrationOutcome_>& outcome, Matrix_<>* value);
    DAL_OIS_EXCEL_API void
    CurveScriptEvaluation_Get_Table(const Handle_<StorableCalibrationOutcome_>& outcome, const String_& table, Matrix_<Cell_>* value);
} // namespace Dal

#undef DAL_OIS_EXCEL_API

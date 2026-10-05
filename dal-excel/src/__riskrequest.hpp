//
// Created by Codex on 2026/10/6.
//

#pragma once

#include "__calibrationrisk.hpp"

#if defined(_WIN32) && defined(DAL_EXCEL_TEST_API_EXPORTS)
#define DAL_REQUEST_API __declspec(dllexport)
#elif defined(_WIN32) && defined(DAL_EXCEL_TEST_API_IMPORTS)
#define DAL_REQUEST_API __declspec(dllimport)
#else
#define DAL_REQUEST_API
#endif

namespace Dal {
    namespace Excel {
        inline void CheckRequestText(const String_& text, const String_& field) {
            REQUIRE(text.find('\0') == String_::npos, "InvalidRiskRequest: " + field + "; embedded NUL is unsupported");
        }

        template <class T_> const T_& CheckedRequestValue(const Handle_<StorableCalibrationValue_<T_>>& handle, const String_& field) {
            REQUIRE(handle, "InvalidRiskRequest: " + field + "; handle is null");
            return handle->val_;
        }
    } // namespace Excel

    using StorableCalibrationRiskRequest_ = Excel::StorableCalibrationValue_<CalibrationRiskRequest_>;
    using StorableCalibrationRiskPlan_ = Excel::StorableCalibrationValue_<CalibrationRiskPlan_>;
    using StorableCalibrationRiskResult_ = Excel::StorableCalibrationValue_<CalibrationRiskResult_>;
    using StorableDupireScriptRiskSettings_ = Excel::StorableCalibrationValue_<Excel::DupireScriptRiskSettings_>;
    using StorableDupireScriptRiskRequest_ = Excel::StorableCalibrationValue_<DupireScriptRiskRequest_>;
    using StorableDupireScriptRiskPlan_ = Excel::StorableCalibrationValue_<DupireScriptRiskPlan_>;
    using StorableDupireScriptRiskResult_ = Excel::StorableCalibrationValue_<DupireScriptRiskResult_>;

    DAL_REQUEST_API void CalibrationRiskRequest_New(const String_&, const Matrix_<Cell_>&, Handle_<StorableCalibrationRiskRequest_>*);
    DAL_REQUEST_API void CalibrationRiskRequest_Get_Settings(const Handle_<StorableCalibrationRiskRequest_>&, Matrix_<Cell_>*);
    DAL_REQUEST_API void CalibrationRiskPlan_New(const String_&,
                                                 const Handle_<StorableCalibrationPullback_>&,
                                                 const Handle_<StorableCalibrationRiskRequest_>&,
                                                 Handle_<StorableCalibrationRiskPlan_>*);
    DAL_REQUEST_API void CalibrationRiskPlan_Get_Calibration(const Handle_<StorableCalibrationRiskPlan_>&, Handle_<StorableCalibrationPullback_>*);
    DAL_REQUEST_API void CalibrationRiskPlan_Get_Inputs(const Handle_<StorableCalibrationRiskPlan_>&, bool, Matrix_<Cell_>*);
    DAL_REQUEST_API void CalibrationRiskPlan_Get_Shape(const Handle_<StorableCalibrationRiskPlan_>&, Matrix_<Cell_>*);
    DAL_REQUEST_API void CalibrationRiskResult_New(const String_&,
                                                   const Handle_<StorableCalibrationRiskPlan_>&,
                                                   const Handle_<StorableCalibrationParameterAdjoints_>&,
                                                   const Handle_<StorableCalibrationDirectQuoteAdjoints_>&,
                                                   Handle_<StorableCalibrationRiskResult_>*);
    DAL_REQUEST_API void CalibrationRiskResult_Get_Plan(const Handle_<StorableCalibrationRiskResult_>&, Handle_<StorableCalibrationRiskPlan_>*);
    DAL_REQUEST_API void CalibrationRiskResult_Get_QuoteRisk(const Handle_<StorableCalibrationRiskResult_>&, Handle_<StorableCalibrationQuoteRisk_>*);
    DAL_REQUEST_API void CalibrationRiskResult_Get_Jacobian(const Handle_<StorableCalibrationRiskResult_>&, const String_&, Matrix_<Cell_>*);

    DAL_REQUEST_API void DupireScriptRiskSettings_New(const String_&,
                                                      double,
                                                      const Handle_<StorableScriptValuationSettings_>&,
                                                      const Handle_<StorableMonteCarloSettings_>&,
                                                      Handle_<StorableDupireScriptRiskSettings_>*);
    DAL_REQUEST_API void DupireScriptRiskSettings_Get_Configuration(const Handle_<StorableDupireScriptRiskSettings_>&,
                                                                    double*,
                                                                    Handle_<StorableScriptValuationSettings_>*,
                                                                    Handle_<StorableMonteCarloSettings_>*);
    DAL_REQUEST_API void DupireScriptRiskRequest_New(const String_&,
                                                     const Handle_<StorableDupireScriptRiskSettings_>&,
                                                     const Handle_<StorableCalibrationRiskRequest_>&,
                                                     const Matrix_<Cell_>&,
                                                     const Handle_<StorableCalibrationDirectQuoteAdjoints_>&,
                                                     Handle_<StorableDupireScriptRiskRequest_>*);
    DAL_REQUEST_API void DupireScriptRiskRequest_Get_Configuration(const Handle_<StorableDupireScriptRiskRequest_>&,
                                                                   Handle_<StorableDupireScriptRiskSettings_>*,
                                                                   Handle_<StorableCalibrationRiskRequest_>*,
                                                                   Matrix_<Cell_>*,
                                                                   bool*);
    DAL_REQUEST_API void DupireScriptRiskRequest_Get_Direct(const Handle_<StorableDupireScriptRiskRequest_>&,
                                                            Handle_<StorableCalibrationDirectQuoteAdjoints_>*);
    DAL_REQUEST_API void DupireScriptRiskPlan_New(const String_&,
                                                  const Handle_<ScriptProductData_>&,
                                                  const Handle_<ModelData_>&,
                                                  const Handle_<StorableDupireCalibration_>&,
                                                  const String_&,
                                                  const Handle_<StorableDupireScriptRiskRequest_>&,
                                                  Handle_<StorableDupireScriptRiskPlan_>*);
    DAL_REQUEST_API void DupireScriptRiskPlan_Get_QuotePlan(const Handle_<StorableDupireScriptRiskPlan_>&, Handle_<StorableCalibrationRiskPlan_>*);
    DAL_REQUEST_API void DupireScriptRiskPlan_Get_Inputs(const Handle_<StorableDupireScriptRiskPlan_>&, bool, Matrix_<Cell_>*);
    DAL_REQUEST_API void DupireScriptRiskPlan_Get_Configuration(const Handle_<StorableDupireScriptRiskPlan_>&,
                                                                Handle_<StorableDupireScriptRiskSettings_>*,
                                                                Matrix_<Cell_>*);
    DAL_REQUEST_API void DupireScriptRiskPlan_Get_Provenance(const Handle_<StorableDupireScriptRiskPlan_>&, Matrix_<Cell_>*);
    DAL_REQUEST_API void
    DupireScriptRiskResult_New(const String_&, const Handle_<StorableDupireScriptRiskPlan_>&, Handle_<StorableDupireScriptRiskResult_>*);
    DAL_REQUEST_API void DupireScriptRiskResult_Get_Valuation(const Handle_<StorableDupireScriptRiskResult_>&, Handle_<StorableRiskResult_>*);
    DAL_REQUEST_API void DupireScriptRiskResult_Get_QuoteRisk(const Handle_<StorableDupireScriptRiskResult_>&,
                                                              Handle_<StorableCalibrationRiskResult_>*);
    DAL_REQUEST_API void DupireScriptRiskResult_Get_Provenance(const Handle_<StorableDupireScriptRiskResult_>&, Matrix_<Cell_>*);
} // namespace Dal

#undef DAL_REQUEST_API

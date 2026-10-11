//
// Created by Codex on 2026/10/11.
//

#pragma once

#include <dal-excel/src/__riskrequest.hpp>
#include <dal-public/src/dupirecurvature.hpp>

#include "__curvaturerequest.hpp"

#if defined(_WIN32) && defined(DAL_EXCEL_TEST_API_EXPORTS)
#define DAL_DUPIRE_CURVATURE_API __declspec(dllexport)
#elif defined(_WIN32) && defined(DAL_EXCEL_TEST_API_IMPORTS)
#define DAL_DUPIRE_CURVATURE_API __declspec(dllimport)
#else
#define DAL_DUPIRE_CURVATURE_API
#endif

namespace Dal {
    inline const char* RiskValueType(const DupireScriptCurvatureRequest_&) { return "DupireScriptCurvatureRequest"; }
    inline const char* RiskValueType(const DupireScriptCurvaturePlan_&) { return "DupireScriptCurvaturePlan"; }
    inline const char* RiskValueType(const DupireScriptCurvatureResult_&) { return "DupireScriptCurvatureResult"; }

    using StorableDupireScriptCurvatureRequest_ = Excel::StorableRiskValue_<DupireScriptCurvatureRequest_>;
    using StorableDupireScriptCurvaturePlan_ = Excel::StorableRiskValue_<DupireScriptCurvaturePlan_>;
    using StorableDupireScriptCurvatureResult_ = Excel::StorableRiskValue_<DupireScriptCurvatureResult_>;

    DAL_DUPIRE_CURVATURE_API void DupireScriptCurvatureRequest_New(const String_&,
                                                                   const Handle_<StorableDupireScriptRiskRequest_>&,
                                                                   const Handle_<StorableBumpOverAADRequest_>&,
                                                                   Handle_<StorableDupireScriptCurvatureRequest_>*);
    DAL_DUPIRE_CURVATURE_API void DupireScriptCurvatureRequest_Get_Risk(const Handle_<StorableDupireScriptCurvatureRequest_>&,
                                                                        Handle_<StorableDupireScriptRiskRequest_>*);
    DAL_DUPIRE_CURVATURE_API void DupireScriptCurvatureRequest_Get_Bumps(const Handle_<StorableDupireScriptCurvatureRequest_>&,
                                                                         Handle_<StorableBumpOverAADRequest_>*);

    DAL_DUPIRE_CURVATURE_API void DupireScriptCurvaturePlan_New(const String_&,
                                                                const Handle_<ScriptProductData_>&,
                                                                const Handle_<ModelData_>&,
                                                                const Handle_<StorableDupireCalibration_>&,
                                                                const String_&,
                                                                const Handle_<StorableDupireScriptCurvatureRequest_>&,
                                                                Handle_<StorableDupireScriptCurvaturePlan_>*);
    DAL_DUPIRE_CURVATURE_API void DupireScriptCurvaturePlan_Get_BasePlan(const Handle_<StorableDupireScriptCurvaturePlan_>&,
                                                                         Handle_<StorableDupireScriptRiskPlan_>*);
    DAL_DUPIRE_CURVATURE_API void DupireScriptCurvaturePlan_Get_Point(const Handle_<StorableDupireScriptCurvaturePlan_>&, Matrix_<Cell_>*);
    DAL_DUPIRE_CURVATURE_API void DupireScriptCurvaturePlan_Get_Directions(const Handle_<StorableDupireScriptCurvaturePlan_>&, Matrix_<Cell_>*);
    DAL_DUPIRE_CURVATURE_API void DupireScriptCurvaturePlan_Get_Steps(const Handle_<StorableDupireScriptCurvaturePlan_>&, Matrix_<Cell_>*);
    DAL_DUPIRE_CURVATURE_API void DupireScriptCurvaturePlan_Get_Shape(const Handle_<StorableDupireScriptCurvaturePlan_>&, Matrix_<Cell_>*);
    DAL_DUPIRE_CURVATURE_API void DupireScriptCurvaturePlan_Get_Payload(const Handle_<StorableDupireScriptCurvaturePlan_>&, double*);

    DAL_DUPIRE_CURVATURE_API void DupireScriptCurvatureResult_New(const String_&,
                                                                  const Handle_<StorableDupireScriptCurvaturePlan_>&,
                                                                  Handle_<StorableDupireScriptCurvatureResult_>*);
    DAL_DUPIRE_CURVATURE_API void DupireScriptCurvatureResult_Get_Base(const Handle_<StorableDupireScriptCurvatureResult_>&,
                                                                       Handle_<StorableDupireScriptRiskResult_>*);
    DAL_DUPIRE_CURVATURE_API void DupireScriptCurvatureResult_Get_QuotePlan(const Handle_<StorableDupireScriptCurvatureResult_>&,
                                                                            Handle_<StorableCalibrationRiskPlan_>*);
    DAL_DUPIRE_CURVATURE_API void DupireScriptCurvatureResult_Get_Point(const Handle_<StorableDupireScriptCurvatureResult_>&, Matrix_<Cell_>*);
    DAL_DUPIRE_CURVATURE_API void DupireScriptCurvatureResult_Get_Gradient(const Handle_<StorableDupireScriptCurvatureResult_>&, Matrix_<Cell_>*);
    DAL_DUPIRE_CURVATURE_API void DupireScriptCurvatureResult_Get_Directions(const Handle_<StorableDupireScriptCurvatureResult_>&, Matrix_<Cell_>*);
    DAL_DUPIRE_CURVATURE_API void DupireScriptCurvatureResult_Get_Steps(const Handle_<StorableDupireScriptCurvatureResult_>&, Matrix_<Cell_>*);
    DAL_DUPIRE_CURVATURE_API void DupireScriptCurvatureResult_Get_HessianProducts(const Handle_<StorableDupireScriptCurvatureResult_>&,
                                                                                  Matrix_<Cell_>*);
    DAL_DUPIRE_CURVATURE_API void DupireScriptCurvatureResult_Get_Shape(const Handle_<StorableDupireScriptCurvatureResult_>&, Matrix_<Cell_>*);
    DAL_DUPIRE_CURVATURE_API void DupireScriptCurvatureResult_Get_Execution(const Handle_<StorableDupireScriptCurvatureResult_>&, Matrix_<Cell_>*);
} // namespace Dal

#undef DAL_DUPIRE_CURVATURE_API

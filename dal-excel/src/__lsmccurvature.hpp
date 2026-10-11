//
// Created by Codex on 2026/10/11.
//

#pragma once

#include <dal-public/src/lsmccurvature.hpp>

#include "__curvaturerequest.hpp"

#if defined(_WIN32) && defined(DAL_EXCEL_TEST_API_EXPORTS)
#define DAL_LSMC_CURVATURE_API __declspec(dllexport)
#elif defined(_WIN32) && defined(DAL_EXCEL_TEST_API_IMPORTS)
#define DAL_LSMC_CURVATURE_API __declspec(dllimport)
#else
#define DAL_LSMC_CURVATURE_API
#endif

namespace Dal::Script {
    inline const char* RiskValueType(const ExerciseRegression_&) { return "LsmcExercisePolicy"; }
} // namespace Dal::Script

namespace Dal {
    inline const char* RiskValueType(const BlackScholesLsmcPlan_&) { return "BlackScholesLsmcPlan"; }
    inline const char* RiskValueType(const BlackScholesLsmcCurvatureResult_&) { return "BlackScholesLsmcCurvatureResult"; }

    using StorableBlackScholesLsmcPlan_ = Excel::StorableRiskValue_<BlackScholesLsmcPlan_>;
    using StorableBlackScholesLsmcCurvatureResult_ = Excel::StorableRiskValue_<BlackScholesLsmcCurvatureResult_>;
    using StorableLsmcExercisePolicy_ = Excel::StorableRiskValue_<Script::ExerciseRegression_>;

    DAL_LSMC_CURVATURE_API void BlackScholesLsmcPlan_New(const String_&,
                                                         const Handle_<ScriptProductData_>&,
                                                         const Handle_<StorableScriptValuationSettings_>&,
                                                         const Handle_<StorableMonteCarloSettings_>&,
                                                         Handle_<StorableBlackScholesLsmcPlan_>*);
    DAL_LSMC_CURVATURE_API void BlackScholesLsmcCurvatureResult_New(const String_&,
                                                                    const Handle_<StorableBlackScholesLsmcPlan_>&,
                                                                    const Matrix_<Cell_>&,
                                                                    double,
                                                                    const Handle_<StorableBumpOverAADRequest_>&,
                                                                    Handle_<StorableBlackScholesLsmcCurvatureResult_>*);
    DAL_LSMC_CURVATURE_API void MonteCarloSettings_Get_Configuration(const Handle_<StorableMonteCarloSettings_>&, Matrix_<Cell_>*);
    DAL_LSMC_CURVATURE_API void BlackScholesLsmcPlan_Get_Product(const Handle_<StorableBlackScholesLsmcPlan_>&, Handle_<ScriptProductData_>*);
    DAL_LSMC_CURVATURE_API void BlackScholesLsmcPlan_Get_Valuation(const Handle_<StorableBlackScholesLsmcPlan_>&,
                                                                   Handle_<StorableScriptValuationSettings_>*);
    DAL_LSMC_CURVATURE_API void BlackScholesLsmcPlan_Get_Simulation(const Handle_<StorableBlackScholesLsmcPlan_>&,
                                                                    Handle_<StorableMonteCarloSettings_>*);
    DAL_LSMC_CURVATURE_API void BlackScholesLsmcPlan_Get_ParameterLabels(const Handle_<StorableBlackScholesLsmcPlan_>&, Vector_<String_>*);
    DAL_LSMC_CURVATURE_API void BlackScholesLsmcPlan_Get_ScriptConstants(const Handle_<StorableBlackScholesLsmcPlan_>&, Matrix_<Cell_>*);
    DAL_LSMC_CURVATURE_API void BlackScholesLsmcPlan_Get_Observations(const Handle_<StorableBlackScholesLsmcPlan_>&, Matrix_<Cell_>*);
    DAL_LSMC_CURVATURE_API void BlackScholesLsmcPlan_Get_EventDates(const Handle_<StorableBlackScholesLsmcPlan_>&, Matrix_<Cell_>*);
    DAL_LSMC_CURVATURE_API void BlackScholesLsmcPlan_Get_TimeLine(const Handle_<StorableBlackScholesLsmcPlan_>&, Matrix_<Cell_>*);
    DAL_LSMC_CURVATURE_API void BlackScholesLsmcCurvatureResult_Get_Value(const Handle_<StorableBlackScholesLsmcCurvatureResult_>&, double*);
    DAL_LSMC_CURVATURE_API void BlackScholesLsmcCurvatureResult_Get_Gradient(const Handle_<StorableBlackScholesLsmcCurvatureResult_>&,
                                                                             Matrix_<Cell_>*);
    DAL_LSMC_CURVATURE_API void BlackScholesLsmcCurvatureResult_Get_ParameterLabels(const Handle_<StorableBlackScholesLsmcCurvatureResult_>&,
                                                                                    Vector_<String_>*);
    DAL_LSMC_CURVATURE_API void BlackScholesLsmcCurvatureResult_Get_Point(const Handle_<StorableBlackScholesLsmcCurvatureResult_>&, Matrix_<Cell_>*);
    DAL_LSMC_CURVATURE_API void BlackScholesLsmcCurvatureResult_Get_Directions(const Handle_<StorableBlackScholesLsmcCurvatureResult_>&,
                                                                               Matrix_<Cell_>*);
    DAL_LSMC_CURVATURE_API void BlackScholesLsmcCurvatureResult_Get_Steps(const Handle_<StorableBlackScholesLsmcCurvatureResult_>&, Matrix_<Cell_>*);
    DAL_LSMC_CURVATURE_API void BlackScholesLsmcCurvatureResult_Get_HessianProducts(const Handle_<StorableBlackScholesLsmcCurvatureResult_>&,
                                                                                    Matrix_<Cell_>*);
    DAL_LSMC_CURVATURE_API void BlackScholesLsmcCurvatureResult_Get_Shape(const Handle_<StorableBlackScholesLsmcCurvatureResult_>&, Matrix_<Cell_>*);
    DAL_LSMC_CURVATURE_API void BlackScholesLsmcCurvatureResult_Get_Execution(const Handle_<StorableBlackScholesLsmcCurvatureResult_>&,
                                                                              Matrix_<Cell_>*);
    DAL_LSMC_CURVATURE_API void BlackScholesLsmcCurvatureResult_Get_Plan(const Handle_<StorableBlackScholesLsmcCurvatureResult_>&,
                                                                         Handle_<StorableBlackScholesLsmcPlan_>*);
    DAL_LSMC_CURVATURE_API void BlackScholesLsmcCurvatureResult_Get_Simulation(const Handle_<StorableBlackScholesLsmcCurvatureResult_>&,
                                                                               Handle_<StorableMonteCarloSettings_>*);
    DAL_LSMC_CURVATURE_API void BlackScholesLsmcCurvatureResult_Get_BasePolicyCount(const Handle_<StorableBlackScholesLsmcCurvatureResult_>&,
                                                                                    double*);
    DAL_LSMC_CURVATURE_API void BlackScholesLsmcCurvatureResult_Get_BasePolicy(const Handle_<StorableBlackScholesLsmcCurvatureResult_>&,
                                                                               double,
                                                                               Handle_<StorableLsmcExercisePolicy_>*);
    DAL_LSMC_CURVATURE_API void LsmcExercisePolicy_Get_Coefficients(const Handle_<StorableLsmcExercisePolicy_>&, Matrix_<Cell_>*);
    DAL_LSMC_CURVATURE_API void LsmcExercisePolicy_Get_NormalizationMeans(const Handle_<StorableLsmcExercisePolicy_>&, Matrix_<Cell_>*);
    DAL_LSMC_CURVATURE_API void LsmcExercisePolicy_Get_NormalizationSigmas(const Handle_<StorableLsmcExercisePolicy_>&, Matrix_<Cell_>*);
    DAL_LSMC_CURVATURE_API void LsmcExercisePolicy_Get_BasisPowers(const Handle_<StorableLsmcExercisePolicy_>&, Matrix_<Cell_>*);
    DAL_LSMC_CURVATURE_API void LsmcExercisePolicy_Get_Configuration(const Handle_<StorableLsmcExercisePolicy_>&, Matrix_<Cell_>*);
} // namespace Dal

#undef DAL_LSMC_CURVATURE_API

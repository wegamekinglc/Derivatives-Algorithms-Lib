#pragma once

#include <dal-public/src/earlyexercisepderisk.hpp>

#include "__earlyexercisepde.hpp"

#if defined(_WIN32) && defined(DAL_EXCEL_TEST_API_EXPORTS)
#define DAL_EARLY_RISK_API __declspec(dllexport)
#elif defined(_WIN32) && defined(DAL_EXCEL_TEST_API_IMPORTS)
#define DAL_EARLY_RISK_API __declspec(dllimport)
#else
#define DAL_EARLY_RISK_API
#endif

namespace Dal::PDE {
    inline const char* RiskValueType(const EarlyExercisePdeRiskSettings_&) { return "EarlyExercisePdeRiskSettings"; }
    inline const char* RiskValueType(const EarlyExercisePdeRiskRequest_&) { return "EarlyExercisePdeRiskRequest"; }
    inline const char* RiskValueType(const EarlyExercisePdeRiskResult_&) { return "EarlyExercisePdeRiskResult"; }
} // namespace Dal::PDE

namespace Dal {
    using StorableEarlyExercisePdeRiskSettings_ = Excel::StorableRiskValue_<EarlyExercisePdeRiskSettings_>;
    using StorableEarlyExercisePdeRiskRequest_ = Excel::StorableRiskValue_<EarlyExercisePdeRiskRequest_>;
    using StorableEarlyExercisePdeRiskResult_ = Excel::StorableRiskValue_<EarlyExercisePdeRiskResult_>;
    DAL_EARLY_RISK_API void
    EarlyExercisePdeRiskSettings_New(const String_&, const Matrix_<Cell_>&, const Matrix_<Cell_>&, Handle_<StorableEarlyExercisePdeRiskSettings_>*);
    DAL_EARLY_RISK_API void EarlyExercisePdeRiskSettings_Get_Configuration(const Handle_<StorableEarlyExercisePdeRiskSettings_>&, Matrix_<Cell_>*);
    DAL_EARLY_RISK_API void EarlyExercisePdeRiskSettings_Get_CarryBumps(const Handle_<StorableEarlyExercisePdeRiskSettings_>&, Matrix_<Cell_>*);
    DAL_EARLY_RISK_API void EarlyExercisePdeRiskRequest_New(const String_&,
                                                            const Handle_<StorableEarlyExercisePdeRequest_>&,
                                                            const Handle_<StorableEarlyExercisePdeRiskSettings_>&,
                                                            Handle_<StorableEarlyExercisePdeRiskRequest_>*);
    DAL_EARLY_RISK_API void EarlyExercisePdeRiskRequest_Get_Valuation(const Handle_<StorableEarlyExercisePdeRiskRequest_>&,
                                                                      Handle_<StorableEarlyExercisePdeRequest_>*);
    DAL_EARLY_RISK_API void EarlyExercisePdeRiskRequest_Get_Settings(const Handle_<StorableEarlyExercisePdeRiskRequest_>&,
                                                                     Handle_<StorableEarlyExercisePdeRiskSettings_>*);
    DAL_EARLY_RISK_API void EarlyExercisePdeRiskResult_New(const String_&,
                                                           const Handle_<StorableEarlyExercisePdeRiskRequest_>&,
                                                           Handle_<StorableEarlyExercisePdeRiskResult_>*);
    DAL_EARLY_RISK_API void EarlyExercisePdeRiskResult_Get_Request(const Handle_<StorableEarlyExercisePdeRiskResult_>&,
                                                                   Handle_<StorableEarlyExercisePdeRiskRequest_>*);
    DAL_EARLY_RISK_API void EarlyExercisePdeRiskResult_Get_BaseResult(const Handle_<StorableEarlyExercisePdeRiskResult_>&,
                                                                      Handle_<StorableEarlyExercisePdeResult_>*);
    DAL_EARLY_RISK_API void EarlyExercisePdeRiskResult_Get_ResolvedSettings(const Handle_<StorableEarlyExercisePdeRiskResult_>&,
                                                                            Handle_<StorableEarlyExercisePdeRiskSettings_>*);
    DAL_EARLY_RISK_API void EarlyExercisePdeRiskResult_Get_Price(const Handle_<StorableEarlyExercisePdeRiskResult_>&, double*);
    DAL_EARLY_RISK_API void EarlyExercisePdeRiskResult_Get_RiskValue(const Handle_<StorableEarlyExercisePdeRiskResult_>&, const String_&, double*);
    DAL_EARLY_RISK_API void EarlyExercisePdeRiskResult_Get_Summary(const Handle_<StorableEarlyExercisePdeRiskResult_>&, Matrix_<Cell_>*);
    DAL_EARLY_RISK_API void EarlyExercisePdeRiskResult_Get_Plan(const Handle_<StorableEarlyExercisePdeRiskResult_>&, Matrix_<Cell_>*);
    DAL_EARLY_RISK_API void EarlyExercisePdeRiskResult_Get_PlanGrid(const Handle_<StorableEarlyExercisePdeRiskResult_>&, Matrix_<Cell_>*);
    DAL_EARLY_RISK_API void EarlyExercisePdeRiskResult_Get_PlanTimeGrid(const Handle_<StorableEarlyExercisePdeRiskResult_>&, Matrix_<Cell_>*);
    DAL_EARLY_RISK_API void EarlyExercisePdeRiskResult_Get_PlanSteps(const Handle_<StorableEarlyExercisePdeRiskResult_>&, Matrix_<Cell_>*);
    DAL_EARLY_RISK_API void EarlyExercisePdeRiskResult_Get_BumpOrigins(const Handle_<StorableEarlyExercisePdeRiskResult_>&, Matrix_<Cell_>*);
    DAL_EARLY_RISK_API void EarlyExercisePdeRiskResult_Get_Risks(const Handle_<StorableEarlyExercisePdeRiskResult_>&, Matrix_<Cell_>*);
    DAL_EARLY_RISK_API void EarlyExercisePdeRiskResult_Get_Stencils(const Handle_<StorableEarlyExercisePdeRiskResult_>&, Matrix_<Cell_>*);
    DAL_EARLY_RISK_API void EarlyExercisePdeRiskResult_Get_Scenarios(const Handle_<StorableEarlyExercisePdeRiskResult_>&, Matrix_<Cell_>*);
    DAL_EARLY_RISK_API void EarlyExercisePdeRiskResult_Get_Execution(const Handle_<StorableEarlyExercisePdeRiskResult_>&, Matrix_<Cell_>*);
} // namespace Dal

#undef DAL_EARLY_RISK_API

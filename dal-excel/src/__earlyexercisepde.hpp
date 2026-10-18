#pragma once

#include <dal-public/src/earlyexercisepde.hpp>

#include "__risk.hpp"

#if defined(_WIN32) && defined(DAL_EXCEL_TEST_API_EXPORTS)
#define DAL_EARLY_PDE_API __declspec(dllexport)
#elif defined(_WIN32) && defined(DAL_EXCEL_TEST_API_IMPORTS)
#define DAL_EARLY_PDE_API __declspec(dllimport)
#else
#define DAL_EARLY_PDE_API
#endif

namespace Dal::PDE {
    inline const char* RiskValueType(const EarlyExercisePdeSettings_&) { return "EarlyExercisePdeSettings"; }
    inline const char* RiskValueType(const EarlyExercisePdeRequest_&) { return "EarlyExercisePdeRequest"; }
    inline const char* RiskValueType(const EarlyExercisePdeResult_&) { return "EarlyExercisePdeResult"; }
} // namespace Dal::PDE

namespace Dal {
    using StorableEarlyExercisePdeSettings_ = Excel::StorableRiskValue_<EarlyExercisePdeSettings_>;
    using StorableEarlyExercisePdeRequest_ = Excel::StorableRiskValue_<EarlyExercisePdeRequest_>;
    using StorableEarlyExercisePdeResult_ = Excel::StorableRiskValue_<EarlyExercisePdeResult_>;
    DAL_EARLY_PDE_API void EarlyExercisePdeSettings_New(const String_&, const Matrix_<Cell_>&, Handle_<StorableEarlyExercisePdeSettings_>*);
    DAL_EARLY_PDE_API void EarlyExercisePdeSettings_Get_Configuration(const Handle_<StorableEarlyExercisePdeSettings_>&, Matrix_<Cell_>*);
    DAL_EARLY_PDE_API void EarlyExercisePdeRequest_New(const String_&,
                                                       const Matrix_<Cell_>&,
                                                       const Matrix_<Cell_>&,
                                                       const Matrix_<Cell_>&,
                                                       const Matrix_<Cell_>&,
                                                       const Handle_<StorableEarlyExercisePdeSettings_>&,
                                                       Handle_<StorableEarlyExercisePdeRequest_>*);
    DAL_EARLY_PDE_API void EarlyExercisePdeRequest_Get_Settings(const Handle_<StorableEarlyExercisePdeRequest_>&,
                                                                Handle_<StorableEarlyExercisePdeSettings_>*);
    DAL_EARLY_PDE_API void
    EarlyExercisePdeResult_New(const String_&, const Handle_<StorableEarlyExercisePdeRequest_>&, Handle_<StorableEarlyExercisePdeResult_>*);
    DAL_EARLY_PDE_API void EarlyExercisePdeResult_Get_Request(const Handle_<StorableEarlyExercisePdeResult_>&,
                                                              Handle_<StorableEarlyExercisePdeRequest_>*);
    DAL_EARLY_PDE_API void EarlyExercisePdeResult_Get_Price(const Handle_<StorableEarlyExercisePdeResult_>&, double*);
    DAL_EARLY_PDE_API void EarlyExercisePdeRequest_Get_Contract(const Handle_<StorableEarlyExercisePdeRequest_>&, Matrix_<Cell_>*);
    DAL_EARLY_PDE_API void EarlyExercisePdeRequest_Get_Market(const Handle_<StorableEarlyExercisePdeRequest_>&, Matrix_<Cell_>*);
    DAL_EARLY_PDE_API void EarlyExercisePdeRequest_Get_Carry(const Handle_<StorableEarlyExercisePdeRequest_>&, Matrix_<Cell_>*);
    DAL_EARLY_PDE_API void EarlyExercisePdeRequest_Get_ExerciseDates(const Handle_<StorableEarlyExercisePdeRequest_>&, Matrix_<Cell_>*);
    DAL_EARLY_PDE_API void EarlyExercisePdeResult_Get_Summary(const Handle_<StorableEarlyExercisePdeResult_>&, Matrix_<Cell_>*);
    DAL_EARLY_PDE_API void EarlyExercisePdeResult_Get_Grid(const Handle_<StorableEarlyExercisePdeResult_>&, Matrix_<Cell_>*);
    DAL_EARLY_PDE_API void EarlyExercisePdeResult_Get_TimeGrid(const Handle_<StorableEarlyExercisePdeResult_>&, Matrix_<Cell_>*);
    DAL_EARLY_PDE_API void EarlyExercisePdeResult_Get_Steps(const Handle_<StorableEarlyExercisePdeResult_>&, Matrix_<Cell_>*);
    DAL_EARLY_PDE_API void EarlyExercisePdeResult_Get_ExerciseLevels(const Handle_<StorableEarlyExercisePdeResult_>&, Matrix_<Cell_>*);
    DAL_EARLY_PDE_API void EarlyExercisePdeResult_Get_ExerciseRegions(const Handle_<StorableEarlyExercisePdeResult_>&, Matrix_<Cell_>*);
    DAL_EARLY_PDE_API void EarlyExercisePdeResult_Get_Execution(const Handle_<StorableEarlyExercisePdeResult_>&, Matrix_<Cell_>*);
    DAL_EARLY_PDE_API void EarlyExercisePdeResult_Get_Failure(const Handle_<StorableEarlyExercisePdeResult_>&, Matrix_<Cell_>*);
} // namespace Dal

#undef DAL_EARLY_PDE_API

//
// Created by Codex on 2026/10/11.
//

#pragma once

#include <dal-public/src/europeanpderisk.hpp>

#include "__risk.hpp"

#if defined(_WIN32) && defined(DAL_EXCEL_TEST_API_EXPORTS)
#define DAL_EUROPEAN_PDE_API __declspec(dllexport)
#elif defined(_WIN32) && defined(DAL_EXCEL_TEST_API_IMPORTS)
#define DAL_EUROPEAN_PDE_API __declspec(dllimport)
#else
#define DAL_EUROPEAN_PDE_API
#endif

namespace Dal {
    // Keep value metadata beside its type for argument-dependent lookup.
    inline const char* RiskValueType(const EuropeanPdeRiskRequest_&) { return "EuropeanPdeRiskRequest"; }
    inline const char* RiskValueType(const EuropeanPdeRiskResult_&) { return "EuropeanPdeRiskResult"; }

    namespace Excel {
        struct EuropeanPdeRiskSettings_ {
            EuropeanPdeSettings_ physical_;
            std::optional<size_t> numericPayloadBudgetBytes_, recordingCapacityBudgetBytes_;
        };

        inline const char* RiskValueType(const EuropeanPdeRiskSettings_&) { return "EuropeanPdeRiskSettings"; }
    } // namespace Excel

    using StorableEuropeanPdeRiskSettings_ = Excel::StorableRiskValue_<Excel::EuropeanPdeRiskSettings_>;
    using StorableEuropeanPdeRiskRequest_ = Excel::StorableRiskValue_<EuropeanPdeRiskRequest_>;
    using StorableEuropeanPdeRiskResult_ = Excel::StorableRiskValue_<EuropeanPdeRiskResult_>;

    DAL_EUROPEAN_PDE_API void EuropeanPdeRiskSettings_New(const String_&, const Matrix_<Cell_>&, Handle_<StorableEuropeanPdeRiskSettings_>*);
    DAL_EUROPEAN_PDE_API void EuropeanPdeRiskSettings_Get_Configuration(const Handle_<StorableEuropeanPdeRiskSettings_>&, Matrix_<Cell_>*);
    DAL_EUROPEAN_PDE_API void EuropeanPdeRiskRequest_New(
        const String_&, double, double, double, const Handle_<StorableEuropeanPdeRiskSettings_>&, Handle_<StorableEuropeanPdeRiskRequest_>*);
    DAL_EUROPEAN_PDE_API void EuropeanPdeRiskRequest_Get_Settings(const Handle_<StorableEuropeanPdeRiskRequest_>&,
                                                                  Handle_<StorableEuropeanPdeRiskSettings_>*);
    DAL_EUROPEAN_PDE_API void EuropeanPdeRiskRequest_Get_Point(const Handle_<StorableEuropeanPdeRiskRequest_>&, Matrix_<Cell_>*);
    DAL_EUROPEAN_PDE_API void
    EuropeanPdeRiskResult_New(const String_&, const Handle_<StorableEuropeanPdeRiskRequest_>&, Handle_<StorableEuropeanPdeRiskResult_>*);
    DAL_EUROPEAN_PDE_API void EuropeanPdeRiskResult_Get_Request(const Handle_<StorableEuropeanPdeRiskResult_>&,
                                                                Handle_<StorableEuropeanPdeRiskRequest_>*);
    DAL_EUROPEAN_PDE_API void EuropeanPdeRiskResult_Get_Prices(const Handle_<StorableEuropeanPdeRiskResult_>&, Matrix_<Cell_>*);
    DAL_EUROPEAN_PDE_API void EuropeanPdeRiskResult_Get_Jacobian(const Handle_<StorableEuropeanPdeRiskResult_>&, Matrix_<Cell_>*);
    DAL_EUROPEAN_PDE_API void EuropeanPdeRiskResult_Get_Grid(const Handle_<StorableEuropeanPdeRiskResult_>&, Matrix_<Cell_>*);
    DAL_EUROPEAN_PDE_API void EuropeanPdeRiskResult_Get_ForwardErrors(const Handle_<StorableEuropeanPdeRiskResult_>&, Matrix_<Cell_>*);
    DAL_EUROPEAN_PDE_API void EuropeanPdeRiskResult_Get_TransposeErrors(const Handle_<StorableEuropeanPdeRiskResult_>&, Matrix_<Cell_>*);
    DAL_EUROPEAN_PDE_API void EuropeanPdeRiskResult_Get_Execution(const Handle_<StorableEuropeanPdeRiskResult_>&, Matrix_<Cell_>*);
} // namespace Dal

#undef DAL_EUROPEAN_PDE_API

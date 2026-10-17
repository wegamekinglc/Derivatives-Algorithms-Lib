//
// Created by Codex on 2026/10/7.
//

#pragma once

#include "__risk.hpp"

#if defined(_WIN32) && defined(DAL_EXCEL_TEST_API_EXPORTS)
#define DAL_RISK_API __declspec(dllexport)
#elif defined(_WIN32) && defined(DAL_EXCEL_TEST_API_IMPORTS)
#define DAL_RISK_API __declspec(dllimport)
#else
#define DAL_RISK_API
#endif

namespace Dal {
    using Script::ScriptPortfolioData_;
    using StorablePortfolioWeightedRiskRequest_ = Excel::StorableRiskValue_<PortfolioWeightedRiskRequest_>;
    using StorablePortfolioWeightedRiskResult_ = Excel::StorableRiskValue_<PortfolioWeightedRiskResult_>;
    using StorablePortfolioJacobianRiskRequest_ = Excel::StorableRiskValue_<PortfolioJacobianRiskRequest_>;
    using StorablePortfolioJacobianRiskResult_ = Excel::StorableRiskValue_<PortfolioJacobianRiskResult_>;

    DAL_RISK_API void ScriptPortfolio_New(const String_&, const Matrix_<Cell_>&, Handle_<ScriptPortfolioData_>*);
    DAL_RISK_API void PortfolioWeightedRiskRequest_New(const String_&, const Matrix_<Cell_>&, Handle_<StorablePortfolioWeightedRiskRequest_>*);
    DAL_RISK_API void PortfolioJacobianRiskRequest_New(const String_&, const Matrix_<Cell_>&, Handle_<StorablePortfolioJacobianRiskRequest_>*);
    DAL_RISK_API void PortfolioMonteCarlo_ValueWithWeightedRisk(const Handle_<ScriptPortfolioData_>&,
                                                                double,
                                                                const Handle_<StorablePortfolioWeightedRiskRequest_>&,
                                                                const Handle_<StorableScriptValuationSettings_>&,
                                                                const Handle_<StorableMonteCarloSettings_>&,
                                                                Handle_<StorablePortfolioWeightedRiskResult_>*);
    DAL_RISK_API void PortfolioMonteCarlo_ValueWithJacobianRisk(const Handle_<ScriptPortfolioData_>&,
                                                                double,
                                                                const Handle_<StorablePortfolioJacobianRiskRequest_>&,
                                                                const Handle_<StorableScriptValuationSettings_>&,
                                                                const Handle_<StorableMonteCarloSettings_>&,
                                                                Handle_<StorablePortfolioJacobianRiskResult_>*);
    DAL_RISK_API void PortfolioRiskResult_Get_Objective(const Handle_<Storable_>&, double*);
    DAL_RISK_API void PortfolioRiskResult_Get_Values(const Handle_<Storable_>&, Matrix_<Cell_>*);
    DAL_RISK_API void PortfolioRiskResult_Get_Jacobian(const Handle_<Storable_>&, bool, Matrix_<Cell_>*);
    DAL_RISK_API void PortfolioRiskResult_Get_Shape(const Handle_<Storable_>&, Matrix_<Cell_>*);
    DAL_RISK_API void PortfolioRiskResult_Get_Outputs(const Handle_<Storable_>&, bool, Matrix_<Cell_>*);
    DAL_RISK_API void PortfolioRiskResult_Get_Inputs(const Handle_<Storable_>&, bool, Matrix_<Cell_>*);
    DAL_RISK_API void PortfolioRiskResult_Get_Execution(const Handle_<Storable_>&, Matrix_<Cell_>*);
    DAL_RISK_API void PortfolioRiskResult_Get_Sampling(const Handle_<Storable_>&, Matrix_<Cell_>*);
    DAL_RISK_API void PortfolioRiskResult_Get_Provenance(const Handle_<Storable_>&, Matrix_<Cell_>*);
    DAL_RISK_API void PortfolioRiskResult_Get_Trades(const Handle_<Storable_>&, Matrix_<Cell_>*);
    DAL_RISK_API void PortfolioRiskResult_Get_TradeProvenance(const Handle_<Storable_>&, double, Matrix_<Cell_>*);
    DAL_RISK_API void PortfolioRiskResult_Get_History(const Handle_<Storable_>&, double, Matrix_<Cell_>*);
    DAL_RISK_API void PortfolioRiskResult_Get_Product(const Handle_<Storable_>&, double, Matrix_<Cell_>*);
    DAL_RISK_API void PortfolioRiskResult_Get_ModelSnapshot(const Handle_<Storable_>&, double, Vector_<String_>*);

    namespace Excel {
        DAL_RISK_API String_ PortfolioTestStore(const Handle_<Storable_>&);
    } // namespace Excel
} // namespace Dal

#undef DAL_RISK_API

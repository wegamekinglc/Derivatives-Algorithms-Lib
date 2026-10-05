//
// Created by Codex on 2026/10/5.
//

#pragma once

#include <utility>

#include "__script_storable.hpp"

#if defined(_WIN32) && defined(DAL_EXCEL_TEST_API_EXPORTS)
#define DAL_RISK_API __declspec(dllexport)
#elif defined(_WIN32) && defined(DAL_EXCEL_TEST_API_IMPORTS)
#define DAL_RISK_API __declspec(dllimport)
#else
#define DAL_RISK_API
#endif

namespace Dal {
    namespace Excel {
        inline const char* RiskValueType(const Script::RiskRequest_&) { return "RiskRequest"; }
        inline const char* RiskValueType(const Script::RiskResult_&) { return "RiskResult"; }

        template <class T_> struct StorableRiskValue_ : Storable_ {
            const T_ val_;
            StorableRiskValue_(const String_& name, T_ value) : Storable_(RiskValueType(value), name), val_(std::move(value)) {}
            void Write(Archive::Store_&) const override { THROW("RiskArchiveUnsupported: risk request/result handles do not support serialization"); }
        };
    } // namespace Excel

    using StorableRiskRequest_ = Excel::StorableRiskValue_<Script::RiskRequest_>;
    using StorableRiskResult_ = Excel::StorableRiskValue_<Script::RiskResult_>;

    DAL_RISK_API void RiskRequest_New(const String_& name, const Matrix_<Cell_>& settings, Handle_<StorableRiskRequest_>* request);
    DAL_RISK_API void MonteCarlo_ValueWithRisk(const Handle_<ScriptProductData_>& product,
                                               const Handle_<ModelData_>& modelData,
                                               double nPaths,
                                               const Handle_<StorableRiskRequest_>& request,
                                               const Handle_<StorableScriptValuationSettings_>& valuation,
                                               const Handle_<StorableMonteCarloSettings_>& simulation,
                                               Handle_<StorableRiskResult_>* result);
    DAL_RISK_API void RiskResult_Get_Values(const Handle_<StorableRiskResult_>& result, Matrix_<Cell_>* values);
    DAL_RISK_API void RiskResult_Get_Jacobian(const Handle_<StorableRiskResult_>& result, bool reported, Matrix_<Cell_>* jacobian);
    DAL_RISK_API void RiskResult_Get_Shape(const Handle_<StorableRiskResult_>& result, Matrix_<Cell_>* shape);
    DAL_RISK_API void RiskResult_Get_Inputs(const Handle_<StorableRiskResult_>& result, bool complete, Matrix_<Cell_>* inputs);
    DAL_RISK_API void RiskResult_Get_Outputs(const Handle_<StorableRiskResult_>& result, Vector_<String_>* outputs);
    DAL_RISK_API void RiskResult_Get_Provenance(const Handle_<StorableRiskResult_>& result, Matrix_<Cell_>* provenance);
    DAL_RISK_API void RiskResult_Get_History(const Handle_<StorableRiskResult_>& result, Matrix_<Cell_>* history);
    DAL_RISK_API void RiskResult_Get_Product(const Handle_<StorableRiskResult_>& result, Matrix_<Cell_>* product);
    DAL_RISK_API void RiskResult_Get_ModelSnapshot(const Handle_<StorableRiskResult_>& result, Vector_<String_>* json);
    DAL_RISK_API void RiskResult_Get_LegacyValues(const Handle_<StorableRiskResult_>& result, Matrix_<Cell_>* values);
} // namespace Dal

#undef DAL_RISK_API

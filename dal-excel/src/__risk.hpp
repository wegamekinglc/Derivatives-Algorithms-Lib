//
// Created by Codex on 2026/10/5.
//

#pragma once

#include <utility>

#include "__script_storable.hpp"
#include "__value.hpp"

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
        inline const char* RiskValueType(const Script::WeightedRiskRequest_&) { return "WeightedRiskRequest"; }
        inline const char* RiskValueType(const Script::WeightedRiskResult_&) { return "WeightedRiskResult"; }
        inline const char* RiskValueType(const Script::JacobianRiskRequest_&) { return "JacobianRiskRequest"; }
        inline const char* RiskValueType(const Script::JacobianRiskResult_&) { return "JacobianRiskResult"; }

        template <class T_> struct StorableRiskValue_ : Storable_ {
            const T_ val_;
            StorableRiskValue_(const String_& name, T_ value) : Storable_(RiskValueType(value), name), val_(std::move(value)) {}
            void Write(Archive::Store_&) const override { THROW("RiskArchiveUnsupported: risk request/result handles do not support serialization"); }
        };

        template <auto VALUE_, class R_>
        auto EvaluateRiskValuation(const Handle_<ScriptProductData_>& product,
                                   const Handle_<ModelData_>& model,
                                   double nPaths,
                                   const Handle_<StorableRiskValue_<R_>>& request,
                                   const Handle_<StorableScriptValuationSettings_>& valuation,
                                   const Handle_<StorableMonteCarloSettings_>& simulation,
                                   const char* function) {
            const int count = CheckedMonteCarloPathCount(nPaths, function);
            const auto requested = request ? request->val_ : R_();
            const auto settings = valuation ? valuation->val_ : ScriptValuationSettings_();
            const auto execution = simulation ? simulation->val_ : DefaultRiskMonteCarloSettings();
            return VALUE_(product, model, count, requested, settings, execution);
        }
    } // namespace Excel

    using StorableRiskRequest_ = Excel::StorableRiskValue_<Script::RiskRequest_>;
    using StorableRiskResult_ = Excel::StorableRiskValue_<Script::RiskResult_>;
    using StorableWeightedRiskRequest_ = Excel::StorableRiskValue_<Script::WeightedRiskRequest_>;
    using StorableWeightedRiskResult_ = Excel::StorableRiskValue_<Script::WeightedRiskResult_>;
    using StorableJacobianRiskRequest_ = Excel::StorableRiskValue_<Script::JacobianRiskRequest_>;
    using StorableJacobianRiskResult_ = Excel::StorableRiskValue_<Script::JacobianRiskResult_>;

    DAL_RISK_API void JacobianRiskRequest_New(const String_&, const Matrix_<Cell_>&, Handle_<StorableJacobianRiskRequest_>*);
    DAL_RISK_API void MonteCarlo_ValueWithJacobianRisk(const Handle_<ScriptProductData_>&,
                                                       const Handle_<ModelData_>&,
                                                       double,
                                                       const Handle_<StorableJacobianRiskRequest_>&,
                                                       const Handle_<StorableScriptValuationSettings_>&,
                                                       const Handle_<StorableMonteCarloSettings_>&,
                                                       Handle_<StorableJacobianRiskResult_>*);
    DAL_RISK_API void JacobianRiskResult_Get_Values(const Handle_<StorableJacobianRiskResult_>&, Matrix_<Cell_>*);
    DAL_RISK_API void JacobianRiskResult_Get_Jacobian(const Handle_<StorableJacobianRiskResult_>&, bool, Matrix_<Cell_>*);
    DAL_RISK_API void JacobianRiskResult_Get_Shape(const Handle_<StorableJacobianRiskResult_>&, Matrix_<Cell_>*);
    DAL_RISK_API void JacobianRiskResult_Get_Outputs(const Handle_<StorableJacobianRiskResult_>&, bool, Matrix_<Cell_>*);
    DAL_RISK_API void JacobianRiskResult_Get_Inputs(const Handle_<StorableJacobianRiskResult_>&, bool, Matrix_<Cell_>*);
    DAL_RISK_API void JacobianRiskResult_Get_Execution(const Handle_<StorableJacobianRiskResult_>&, Matrix_<Cell_>*);
    DAL_RISK_API void JacobianRiskResult_Get_Provenance(const Handle_<StorableJacobianRiskResult_>&, Matrix_<Cell_>*);
    DAL_RISK_API void JacobianRiskResult_Get_History(const Handle_<StorableJacobianRiskResult_>&, Matrix_<Cell_>*);
    DAL_RISK_API void JacobianRiskResult_Get_Product(const Handle_<StorableJacobianRiskResult_>&, Matrix_<Cell_>*);
    DAL_RISK_API void JacobianRiskResult_Get_ModelSnapshot(const Handle_<StorableJacobianRiskResult_>&, Vector_<String_>*);

    DAL_RISK_API void WeightedRiskRequest_New(const String_&, const Matrix_<Cell_>&, Handle_<StorableWeightedRiskRequest_>*);
    DAL_RISK_API void MonteCarlo_ValueWithWeightedRisk(const Handle_<ScriptProductData_>&,
                                                       const Handle_<ModelData_>&,
                                                       double,
                                                       const Handle_<StorableWeightedRiskRequest_>&,
                                                       const Handle_<StorableScriptValuationSettings_>&,
                                                       const Handle_<StorableMonteCarloSettings_>&,
                                                       Handle_<StorableWeightedRiskResult_>*);
    DAL_RISK_API void Product_Get_RiskOutputs(const Handle_<ScriptProductData_>&, Matrix_<Cell_>*);
    DAL_RISK_API void WeightedRiskResult_Get_WeightedValue(const Handle_<StorableWeightedRiskResult_>&, double*);
    DAL_RISK_API void WeightedRiskResult_Get_Components(const Handle_<StorableWeightedRiskResult_>&, Matrix_<Cell_>*);
    DAL_RISK_API void WeightedRiskResult_Get_Jacobian(const Handle_<StorableWeightedRiskResult_>&, bool, Matrix_<Cell_>*);
    DAL_RISK_API void WeightedRiskResult_Get_Shape(const Handle_<StorableWeightedRiskResult_>&, Matrix_<Cell_>*);
    DAL_RISK_API void WeightedRiskResult_Get_Inputs(const Handle_<StorableWeightedRiskResult_>&, bool, Matrix_<Cell_>*);
    DAL_RISK_API void WeightedRiskResult_Get_Provenance(const Handle_<StorableWeightedRiskResult_>&, Matrix_<Cell_>*);
    DAL_RISK_API void WeightedRiskResult_Get_History(const Handle_<StorableWeightedRiskResult_>&, Matrix_<Cell_>*);
    DAL_RISK_API void WeightedRiskResult_Get_Product(const Handle_<StorableWeightedRiskResult_>&, Matrix_<Cell_>*);
    DAL_RISK_API void WeightedRiskResult_Get_ModelSnapshot(const Handle_<StorableWeightedRiskResult_>&, Vector_<String_>*);

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

//
// Created by Codex on 2026/9/27.
//

#pragma once

#include <dal-public/src/models.hpp>

namespace Dal {
    void CorrelatedBSModelData_New(const String_& name,
                                   const Vector_<String_>& indices,
                                   const Vector_<>& spots,
                                   const Vector_<>& vols,
                                   const Vector_<>& divs,
                                   double rate,
                                   const Matrix_<>& correlations,
                                   Handle_<ModelData_>* model);
    void HybridBSEquityData_New(const String_& name,
                                const String_& index,
                                const String_& currency,
                                const String_& factor,
                                double spot,
                                double vol,
                                double div,
                                Handle_<HybridComponentData_>* component);
    void HybridDeterministicRateData_New(const String_& name, const String_& currency, double rate, Handle_<HybridComponentData_>* component);
    void HybridConstantCorrelationData_New(const String_& name,
                                           const Vector_<String_>& factors,
                                           const Matrix_<>& correlations,
                                           Handle_<HybridCorrelationData_>* provider);
    void HybridModelData_New(const String_& name,
                             const String_& domesticCurrency,
                             const Vector_<Handle_<Storable_>>& components,
                             const Handle_<HybridCorrelationData_>& correlation,
                             Handle_<ModelData_>* model);
} // namespace Dal

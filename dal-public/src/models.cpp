//
// Created by wegam on 2022/11/20.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal-public/src/models.hpp>

namespace Dal {
    Handle_<ModelData_> NewCorrelatedBSModelData(const String_& name,
                                                 const Vector_<String_>& indices,
                                                 const Vector_<>& spots,
                                                 const Vector_<>& vols,
                                                 const Vector_<>& divs,
                                                 double rate,
                                                 const Matrix_<>& correlations) {
        return Handle_<ModelData_>(new CorrelatedBSModelData_(name, indices, spots, vols, divs, rate, correlations));
    }
    Handle_<HybridComponentData_> NewHybridBSEquityData(
        const String_& name, const String_& index, const String_& currency, const String_& factor, double spot, double vol, double div) {
        return Handle_<HybridComponentData_>(std::make_shared<HybridBSEquityData_>(name, index, currency, factor, spot, vol, div));
    }
    Handle_<HybridComponentData_> NewHybridDeterministicRateData(const String_& name, const String_& currency, double rate) {
        return Handle_<HybridComponentData_>(std::make_shared<HybridDeterministicRateData_>(name, currency, rate));
    }
    Handle_<HybridCorrelationData_>
    NewHybridConstantCorrelationData(const String_& name, const Vector_<String_>& factors, const Matrix_<>& correlations) {
        return Handle_<HybridCorrelationData_>(std::make_shared<HybridConstantCorrelationData_>(name, factors, correlations));
    }
} // namespace Dal

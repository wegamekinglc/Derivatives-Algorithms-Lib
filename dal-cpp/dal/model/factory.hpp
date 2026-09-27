//
// Created by wegamekinglc on 9/28/24.
//

#pragma once

#include <dal/model/blackscholes.hpp>
#include <dal/model/correlatedblackscholes.hpp>
#include <dal/model/dupire.hpp>
#include <dal/model/hybrid.hpp>

namespace Dal {

    template <class D_> Matrix_<D_> CastMatrix(const Matrix_<>& src) {
        Matrix_<D_> dst(src.Rows(), src.Cols());
        for (int i = 0; i < src.Rows(); ++i)
            for (int j = 0; j < src.Cols(); ++j)
                dst(i, j) = D_(src(i, j));
        return dst;
    }

    template <class D_> Vector_<D_> CastVector(const Vector_<>& src) {
        Vector_<D_> dst;
        dst.reserve(src.size());
        for (const double value : src)
            dst.push_back(D_(value));
        return dst;
    }

    template <class T_> std::unique_ptr<AAD::Model_<T_>> CreateModel(const Handle_<ModelData_>& model_data) {
        auto modelBSImp = dynamic_cast<const BSModelData_*>(model_data.get());
        if (modelBSImp)
            return std::make_unique<AAD::BlackScholes_<T_>>(T_(modelBSImp->spot_), T_(modelBSImp->vol_), T_(modelBSImp->rate_), T_(modelBSImp->div_));

        auto modelDupireImp = dynamic_cast<const DupireModelData_*>(model_data.get());
        if (modelDupireImp)
            return std::make_unique<AAD::Dupire_<T_>>(T_(modelDupireImp->spot_), T_(modelDupireImp->rate_), T_(modelDupireImp->repo_),
                                                      modelDupireImp->spots_, modelDupireImp->times_, CastMatrix<T_>(modelDupireImp->vols_));

        auto modelCorrelatedImp = dynamic_cast<const CorrelatedBSModelData_*>(model_data.get());
        if (modelCorrelatedImp)
            return std::make_unique<AAD::CorrelatedBlackScholes_<T_>>(
                modelCorrelatedImp->indices_, CastVector<T_>(modelCorrelatedImp->spots_), CastVector<T_>(modelCorrelatedImp->vols_),
                CastVector<T_>(modelCorrelatedImp->divs_), T_(modelCorrelatedImp->rate_), modelCorrelatedImp->correlations_);

        auto modelHybridImp = dynamic_cast<const HybridModelData_*>(model_data.get());
        if (modelHybridImp) {
            REQUIRE(modelHybridImp->correlation_, "InvalidHybridCorrelation: provider is required");
            Vector_<std::unique_ptr<AAD::HybridComponent_<T_>>> components;
            for (const auto& data : modelHybridImp->components_) {
                REQUIRE(data, "InvalidHybridComponent: null component");
                components.push_back(AAD::CreateHybridComponent<T_>(*data));
            }
            return std::make_unique<AAD::HybridModel_<T_>>(modelHybridImp->domesticCurrency_, std::move(components), *modelHybridImp->correlation_);
        }

        THROW("can't find matched model type");
    }
} // namespace Dal

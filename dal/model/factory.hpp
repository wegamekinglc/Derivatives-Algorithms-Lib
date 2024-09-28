//
// Created by wegamekinglc on 9/28/24.
//

#pragma once

#include <dal/model/blackscholes.hpp>
#include <dal/model/dupire.hpp>


namespace Dal {

    namespace {
        template <class T_>
        auto ToMatrix(const Matrix_<>& src) {
            Matrix_<T_> rtn(src.Rows(), src.Cols());
            for (int i = 0; i < rtn.Rows(); ++i)
                for (int j = 0; j < rtn.Cols(); ++j)
                    rtn(i, j) = T_(src(i, j));
            return rtn;
        }
    }

    template <class T_>
    std::unique_ptr<AAD::Model_<T_>> CreateModel(const Handle_<ModelData_>& model_data) {
        auto modelBSImp = dynamic_cast<const BSModelData_*>(model_data.get());
        if (modelBSImp)
            return std::make_unique<AAD::BlackScholes_<T_>>(T_(modelBSImp->spot_),
                                                     T_(modelBSImp->vol_),
                                                     T_(modelBSImp->rate_),
                                                     T_(modelBSImp->div_));

        auto modelDupireImp = dynamic_cast<const DupireModelData_*>(model_data.get());
        if (modelDupireImp)
            return std::make_unique<AAD::Dupire_<T_>>(T_(modelDupireImp->spot_),
                                             T_(modelDupireImp->rate_),
                                             T_(modelDupireImp->repo_),
                                               modelDupireImp->spots_,
                                               modelDupireImp->times_,
                                               ToMatrix<T_>(modelDupireImp->vols_));
        THROW("can't find matched model type");
    }
}


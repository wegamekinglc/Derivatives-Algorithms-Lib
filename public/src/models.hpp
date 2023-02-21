//
// Created by wegam on 2022/11/20.
//

#pragma once

#include <dal/math/aad/models/blackscholes.hpp>
#include <dal/math/aad/models/dupire.hpp>

namespace Dal {
    using Dal::AAD::ModelData_;

    FORCE_INLINE Handle_<ModelData_> NewBSModelData(const String_& name,
                                                    double spot,
                                                    double vol,
                                                    double rate,
                                                    double div) {
        return Handle_<ModelData_>(new AAD::BSModelData_(name, spot, vol, rate, div));
    }

    FORCE_INLINE Handle_<ModelData_> NewDupireModelData(const String_& name,
                                                        double spot,
                                                        double rate,
                                                        double repo,
                                                        const Vector_<>& spots,
                                                        const Vector_<>& times,
                                                        const Matrix_<>& vols) {
        return Handle_<ModelData_>(new AAD::DupireModelData_(name, spot, rate, repo, spots, times, vols));
    }
}

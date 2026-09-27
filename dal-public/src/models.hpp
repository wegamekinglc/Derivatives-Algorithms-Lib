//
// Created by wegam on 2022/11/20.
//

#pragma once

#include <dal/model/blackscholes.hpp>
#include <dal/model/correlatedblackscholes.hpp>
#include <dal/model/dupire.hpp>
#include <dal/model/hybriddata.hpp>

namespace Dal {
    FORCE_INLINE Handle_<ModelData_> NewBSModelData(const String_& name,
                                                    double spot,
                                                    double vol,
                                                    double rate,
                                                    double div) {
        return Handle_<ModelData_>(new BSModelData_(name, spot, vol, rate, div));
    }

    FORCE_INLINE Handle_<ModelData_> NewCorrelatedBSModelData(const String_& name, const CorrelatedBSSettings_& settings) {
        return Handle_<ModelData_>(new CorrelatedBSModelData_(name, settings));
    }

    FORCE_INLINE Handle_<ModelData_> NewHybridModelData(const String_& name, const HybridSettings_& settings) {
        return Handle_<ModelData_>(new HybridModelData_(name, settings));
    }

    FORCE_INLINE Handle_<ModelData_> NewDupireModelData(const String_& name,
                                                        double spot,
                                                        double rate,
                                                        double repo,
                                                        const Vector_<>& spots,
                                                        const Vector_<>& times,
                                                        const Matrix_<>& vols) {
        return Handle_<ModelData_>(new DupireModelData_(name, spot, rate, repo, spots, times, vols));
    }
} // namespace Dal

//
// Created by wegam on 2022/11/20.
//

#pragma once

#include <dal/math/aad/models/blackscholes.hpp>

namespace Dal {
    using Dal::AAD::BSModelData_;

    FORCE_INLINE Handle_<BSModelData_> NewBSModelData(const String_& name,
                                                      double spot,
                                                      double vol,
                                                      double rate,
                                                      double div) {
        return Handle_<BSModelData_>(new BSModelData_(name, spot, vol, false, rate, div));
    }
}

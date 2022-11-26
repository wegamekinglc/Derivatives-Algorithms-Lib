//
// Created by wegam on 2022/11/20.
//

#pragma once

#include <dal/math/aad/models/blackscholes.hpp>
#include <dal/script/simulation.hpp>

namespace Dal {

    using AAD::Model_;
    using Script::ScriptProduct_;
    using AAD::ModelData_;

    std::map<String_, double> ValueByMonteCarlo(const Handle_<ScriptProduct_>& product,
                                                const Handle_<ModelData_>& modelData,
                                                int num_path,
                                                const String_& rsg = "sobol",
                                                bool use_bb = false,
                                                bool enable_aad = false,
                                                double smooth = 0.01);


}
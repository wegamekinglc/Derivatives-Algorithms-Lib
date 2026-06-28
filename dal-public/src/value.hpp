//
// Created by wegam on 2022/11/20.
//

#pragma once

#include <dal/script/simulation.hpp>

namespace Dal {

    using AAD::Model_;
    using Script::ScriptProductData_;

    std::map<String_, double> ValueByMonteCarlo(const Handle_<ScriptProductData_>& product,
                                                const Handle_<ModelData_>& modelData,
                                                int numPath,
                                                const String_& rsg = "sobol",
                                                bool useBb = false,
                                                bool enableAad = false,
                                                double smooth = 0.01);


} // namespace Dal

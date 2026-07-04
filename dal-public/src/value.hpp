//
// Created by wegam on 2022/11/20.
//

#pragma once

#include <optional>
#include <dal/script/simulation.hpp>

namespace Dal {

    using AAD::Model_;
    using Script::ScriptProductData_;

    //  compiled: run the script through the compiled (flat-stream) evaluator.
    //  Unset means the library default (tree-walk) — compiled and tree-walk
    //  produce the same numbers; the flag is a performance knob only.
    std::map<String_, double> ValueByMonteCarlo(const Handle_<ScriptProductData_>& product,
                                                const Handle_<ModelData_>& modelData,
                                                int numPath,
                                                const String_& rsg = "sobol",
                                                bool useBb = false,
                                                bool enableAad = false,
                                                double smooth = 0.01,
                                                std::optional<bool> compiled = std::nullopt);


} // namespace Dal

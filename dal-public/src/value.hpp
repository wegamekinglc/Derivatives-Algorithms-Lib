//
// Created by wegam on 2022/11/20.
//

#pragma once

#include <dal/script/simulation.hpp>
#include <optional>

namespace Dal {

    using AAD::Model_;
    using Script::MonteCarloSettings_;
    using Script::ScriptProductData_;
    using Script::ScriptValuationSettings_;

    String_ ExplainScriptValuation(const Handle_<ScriptProductData_>& product,
                                   const Handle_<ModelData_>& modelData,
                                   const ScriptValuationSettings_& valuation = ScriptValuationSettings_());

    std::map<String_, double> ValueByMonteCarlo(const Handle_<ScriptProductData_>& product,
                                                const Handle_<ModelData_>& modelData,
                                                int numPath,
                                                const ScriptValuationSettings_& valuation,
                                                const MonteCarloSettings_& simulation = MonteCarloSettings_());

    //  compiled: opt into the script flat-stream evaluator.
    std::map<String_, double> ValueByMonteCarlo(const Handle_<ScriptProductData_>& product,
                                                const Handle_<ModelData_>& modelData,
                                                int numPath,
                                                const String_& rsg = "sobol",
                                                bool useBb = false,
                                                bool enableAad = false,
                                                double smooth = 0.01,
                                                std::optional<bool> compiled = std::nullopt);

} // namespace Dal

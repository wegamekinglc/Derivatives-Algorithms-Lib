//
// Created by Codex on 2026/9/15.
//

#pragma once

#include <dal/platform/platform.hpp>
#include <dal/string/strings.hpp>

namespace Dal {
    class ModelData_;

    namespace Script {
        class ScriptProductData_;
        class PreparedScript_;
        struct MonteCarloSettings_;
        struct ScriptValuationSettings_;
        String_ DescribeScriptProductData(const ScriptProductData_& data);
        String_ ExplainPreparedScript(const PreparedScript_& prepared);
        String_ ExplainScriptSimulation(const ScriptProductData_& data,
                                        const Handle_<ModelData_>& modelData,
                                        size_t nPaths,
                                        const ScriptValuationSettings_& settings,
                                        const MonteCarloSettings_& simulation);
    } // namespace Script
} // namespace Dal

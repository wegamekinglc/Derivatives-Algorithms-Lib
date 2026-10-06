//
// Created by Codex on 2026/10/5.
//

#pragma once

#include <dal/model/base.hpp>
#include <dal/script/event.hpp>
#include <dal/script/riskresults.hpp>

namespace Dal::Script {
    class PreparedScript_;
} // namespace Dal::Script

namespace Dal::Detail {
    [[nodiscard]] Vector_<Script::RiskCoordinate_> ScriptRiskInputAxis(const AAD::Model_<double>& model, const Script::ScriptProduct_& product);
    [[nodiscard]] Script::RiskResultProvenance_ CaptureScriptRiskProvenance(const Script::PreparedScript_& prepared,
                                                                            const Script::ScriptProductData_& product,
                                                                            const ModelData_& model,
                                                                            int paths);
} // namespace Dal::Detail

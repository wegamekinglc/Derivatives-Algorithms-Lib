//
// Created by Codex on 2026/10/5.
//

#pragma once

#include <dal/model/base.hpp>
#include <dal/script/event.hpp>
#include <dal/script/riskresults.hpp>

namespace Dal::Detail {
    [[nodiscard]] Vector_<Script::RiskCoordinate_> ScriptRiskInputAxis(const AAD::Model_<double>& model, const Script::ScriptProduct_& product);
} // namespace Dal::Detail

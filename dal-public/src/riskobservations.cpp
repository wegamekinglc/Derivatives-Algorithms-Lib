//
// Created by Codex on 2026/10/11.
//

#include <dal/script/preparation.hpp>

#include "riskvalueinternal.hpp"

namespace Dal::Detail {
    Vector_<Script::RiskObservationSnapshot_> CaptureScriptRiskObservations(const Script::PreparedScript_& prepared) {
        Vector_<Script::RiskObservationSnapshot_> observations;
        for (const auto& request : prepared.Plan().Requests())
            observations.push_back(
                {request.key_.canonicalIndex_, request.key_.fixingTime_, request.historical_,
                 request.historyValueId_ ? std::optional<double>(prepared.Plan().KnownValue(*request.historyValueId_)) : std::nullopt});
        return observations;
    }
} // namespace Dal::Detail

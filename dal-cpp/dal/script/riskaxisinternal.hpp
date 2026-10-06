//
// Created by Codex on 2026/10/06.
//

#pragma once

#include <dal/script/weightedrisk.hpp>

namespace Dal::Script::Detail {
    [[nodiscard]] Vector_<size_t> RiskInputPositions(const Vector_<RiskCoordinate_>& axis, const RiskRequest_& request);
    void ValidateRiskResultMetadata(const RiskResultProvenance_& provenance, int paths);
    [[nodiscard]] Vector_<RiskOutputCoordinate_>
    SelectRiskOutputs(const Vector_<RiskOutputCoordinate_>& axis, const std::optional<Vector_<String_>>& requested, const char* requestKind);
    void RequireLiveRiskProduct(
        const ScriptProduct_& product, const Date_& date, const char* requestKind, const char* unsupportedKind, const char* estimator);
    void
    ValidatePreparedRiskOutputs(const Vector_<RiskOutputCoordinate_>& expected, const Vector_<RiskOutputCoordinate_>& prepared, const char* planKind);
    void ValidatePreparedRiskInputs(const Vector_<RiskCoordinate_>& expected, const Vector_<RiskCoordinate_>& prepared, const char* planKind);
} // namespace Dal::Script::Detail

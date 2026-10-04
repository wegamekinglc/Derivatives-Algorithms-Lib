//
// Created by Codex on 2026/10/5.
//

#pragma once

#include <dal/model/dupirerisk.hpp>
#include <dal/script/riskresults.hpp>

namespace Dal {
    DupireParameterAdjoints_
    ExtractDupireParameterAdjoints(const Script::RiskResult_& valuation, const DupireCalibrationSnapshot_& calibration, const String_& component);

    class DupireScriptQuoteRisk_ {
        Script::RiskResult_ valuation_;
        DupireQuoteRisk_ quoteRisk_;
        String_ component_;
        String_ method_;

        DupireScriptQuoteRisk_(const Script::RiskResult_&, DupireQuoteRisk_&&, const String_&);
        friend DupireScriptQuoteRisk_ PullbackDupireScriptRisk(const Script::RiskResult_&,
                                                               const DupireCalibrationSnapshot_&,
                                                               const String_&,
                                                               const std::optional<DupireDirectQuoteAdjoints_>&);

    public:
        [[nodiscard]] const Script::RiskResult_& Valuation() const { return valuation_; }
        [[nodiscard]] const DupireQuoteRisk_& QuoteRisk() const { return quoteRisk_; }
        [[nodiscard]] const String_& Component() const { return component_; }
        [[nodiscard]] const String_& Method() const { return method_; }
    };

    DupireScriptQuoteRisk_ PullbackDupireScriptRisk(const Script::RiskResult_& valuation,
                                                    const DupireCalibrationSnapshot_& calibration,
                                                    const String_& component,
                                                    const std::optional<DupireDirectQuoteAdjoints_>& direct = {});
} // namespace Dal

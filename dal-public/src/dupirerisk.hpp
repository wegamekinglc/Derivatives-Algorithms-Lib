//
// Created by Codex on 2026/10/5.
//

#pragma once

#include <dal/model/dupirerisk.hpp>
#include <dal/model/ivs.hpp>
#include <dal/script/riskresults.hpp>

namespace Dal {
    struct BSModelData_;
    struct ModelData_;

    AAD::MertonIVS_ NewMertonIVS(double spot, double vol, double intensity, double averageJump, double jumpStd);

    Handle_<ModelData_> NewDupireModelData(const String_& name,
                                           const DupireCalibrationSnapshot_& calibration,
                                           const String_& index,
                                           const String_& currency,
                                           const String_& factor,
                                           double maxStep = 1.0 / 12.0);

    DupireCalibrationSnapshot_ CalibrateDupireWithRisk(const BSModelData_& baseModel, const DupireRiskInputs_& inputs, const String_& name = {});

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

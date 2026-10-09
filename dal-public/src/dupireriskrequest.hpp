//
// Created by Codex on 2026/10/5.
//

#pragma once

#include <dal-public/src/calibrationriskrequest.hpp>
#include <dal-public/src/value.hpp>

namespace Dal {
    class DupireScriptRiskResult_;

    struct DupireQuoteBinding_ {
        size_t constantOrdinal_ = 0;
        String_ quoteId_;
    };

    struct DupireScriptRiskRequest_ {
        int numPaths_ = 0;
        CalibrationRiskRequest_ quotes_;
        Vector_<DupireQuoteBinding_> directBindings_;
        std::optional<CalibrationDirectQuoteAdjoints_> direct_;
        ScriptValuationSettings_ valuation_;
        MonteCarloSettings_ simulation_ = DefaultRiskMonteCarloSettings();
    };

    class DupireScriptRiskPlan_ {
        struct Data_;
        std::shared_ptr<const Data_> data_;

        explicit DupireScriptRiskPlan_(std::shared_ptr<const Data_>);
        friend DupireScriptRiskPlan_ PlanDupireScriptRisk(const Handle_<ScriptProductData_>&,
                                                          const Handle_<ModelData_>&,
                                                          const DupireCalibrationSnapshot_&,
                                                          const String_&,
                                                          const DupireScriptRiskRequest_&);
        friend DupireScriptRiskResult_ ValueByMonteCarloWithDupireRisk(const DupireScriptRiskPlan_&);
        friend DupireScriptRiskPlan_ RecalibrateDupireScriptRisk(const DupireScriptRiskPlan_&, const Matrix_<>&);

    public:
        [[nodiscard]] const String_& Component() const;
        [[nodiscard]] const CalibrationRiskPlan_& QuotePlan() const;
        [[nodiscard]] const Vector_<Script::RiskCoordinate_>& CompleteInputAxis() const;
        [[nodiscard]] const Vector_<Script::RiskCoordinate_>& RequiredInputAxis() const;
        [[nodiscard]] const Vector_<DupireQuoteBinding_>& DirectBindings() const;
        [[nodiscard]] int NumPaths() const;
        [[nodiscard]] const ScriptValuationSettings_& ValuationSettings() const;
        [[nodiscard]] const MonteCarloSettings_& SimulationSettings() const;
        [[nodiscard]] size_t NumericPayloadBytes() const;
    };

    [[nodiscard]] DupireScriptRiskPlan_ PlanDupireScriptRisk(const Handle_<ScriptProductData_>& product,
                                                             const Handle_<ModelData_>& model,
                                                             const DupireCalibrationSnapshot_& calibration,
                                                             const String_& component,
                                                             const DupireScriptRiskRequest_& request);

    [[nodiscard]] DupireScriptRiskPlan_ RecalibrateDupireScriptRisk(const DupireScriptRiskPlan_& plan, const Matrix_<>& quoteSpreads);

    class DupireScriptRiskResult_ {
        Script::RiskResult_ valuation_;
        CalibrationRiskResult_ quoteRisk_;
        String_ component_;
        String_ method_;
        size_t numericPayloadBytes_;

        DupireScriptRiskResult_(Script::RiskResult_&&, CalibrationRiskResult_&&, const String_&, size_t);
        friend DupireScriptRiskResult_ ValueByMonteCarloWithDupireRisk(const DupireScriptRiskPlan_&);

    public:
        [[nodiscard]] const Script::RiskResult_& Valuation() const { return valuation_; }
        [[nodiscard]] const CalibrationRiskResult_& QuoteRisk() const { return quoteRisk_; }
        [[nodiscard]] const String_& Component() const { return component_; }
        [[nodiscard]] const String_& Method() const { return method_; }
        [[nodiscard]] size_t NumericPayloadBytes() const { return numericPayloadBytes_; }
    };

    [[nodiscard]] DupireScriptRiskResult_ ValueByMonteCarloWithDupireRisk(const DupireScriptRiskPlan_& plan);
} // namespace Dal

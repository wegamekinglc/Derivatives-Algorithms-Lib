//
// Created by Codex on 2026/10/6.
//

#pragma once

#include <dal/script/portfoliobatch.hpp>

namespace Dal::Detail {
    class PortfolioWeightedPlan_;
    class PortfolioJacobianPlan_;

    [[nodiscard]] Script::Detail::PreparedPortfolio_ PreparePortfolioWeightedReplay(const PortfolioWeightedPlan_& plan,
                                                                                    int paths,
                                                                                    const Script::ScriptValuationSettings_& valuation,
                                                                                    const Script::MonteCarloSettings_& simulation,
                                                                                    Script::Detail::PortfolioCapacityLimits_ limits = {});

    struct PortfolioGroupCounters_ {
        size_t generatedScenarios_ = 0;
        size_t evaluatorCalls_ = 0;
        size_t suffixReversals_ = 0;
        size_t prefixReversals_ = 0;
        Vector_<size_t> actualWidths_;
        size_t replayAttempts_ = 0;
    };

    struct PortfolioWeightedReplayResult_ {
        double weightedValue_ = 0.0;
        Vector_<double> componentMeans_;
        Vector_<double> gradient_;
        Vector_<PortfolioGroupCounters_> groupCounters_;
        size_t peakScratchBytes_ = 0;
        size_t peakTapeBytes_ = 0;

        PortfolioWeightedReplayResult_(size_t outputs, size_t inputs, size_t groups)
            : componentMeans_(outputs, 0.0), gradient_(inputs, 0.0), groupCounters_(groups) {}
    };

    [[nodiscard]] PortfolioWeightedReplayResult_ EvaluatePortfolioWeightedReplay(const Script::Detail::PreparedPortfolio_& portfolio,
                                                                                 const Vector_<Script::Detail::PortfolioBatchOutput_>& outputs,
                                                                                 const Vector_<size_t>& selectedInputs,
                                                                                 Script::Detail::PortfolioCapacityLimits_ limits = {});

    struct PortfolioJacobianReplayResult_ {
        Vector_<double> componentMeans_;
        Matrix_<double> jacobian_;
        Vector_<PortfolioGroupCounters_> groupCounters_;
        size_t peakScratchBytes_ = 0;
        size_t peakTapeBytes_ = 0;

        PortfolioJacobianReplayResult_(size_t outputs, size_t inputs, size_t groups)
            : componentMeans_(outputs, 0.0), jacobian_(static_cast<int>(outputs), static_cast<int>(inputs), 0.0), groupCounters_(groups) {}
    };

    struct PortfolioJacobianPreparation_ {
        Script::Detail::PreparedPortfolio_ portfolio_;
        Vector_<size_t> groupWidths_;
    };

    [[nodiscard]] PortfolioJacobianPreparation_ PreparePortfolioJacobianReplay(const PortfolioJacobianPlan_& plan,
                                                                               int paths,
                                                                               const Script::ScriptValuationSettings_& valuation,
                                                                               const Script::MonteCarloSettings_& simulation,
                                                                               Script::Detail::PortfolioCapacityLimits_ limits = {});

    [[nodiscard]] PortfolioJacobianReplayResult_ EvaluatePortfolioJacobianReplay(const Script::Detail::PreparedPortfolio_& portfolio,
                                                                                 const Vector_<Script::Detail::PortfolioBatchOutput_>& outputs,
                                                                                 const Vector_<size_t>& selectedInputs,
                                                                                 const Vector_<size_t>& groupWidths,
                                                                                 Script::Detail::PortfolioCapacityLimits_ limits = {});
} // namespace Dal::Detail

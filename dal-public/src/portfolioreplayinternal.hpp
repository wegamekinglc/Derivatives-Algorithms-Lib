//
// Created by Codex on 2026/10/6.
//

#pragma once

#include <dal/script/portfoliobatch.hpp>

namespace Dal::Detail {
    struct PortfolioGroupCounters_ {
        size_t generatedScenarios_ = 0;
        size_t evaluatorCalls_ = 0;
        size_t suffixReversals_ = 0;
        size_t prefixReversals_ = 0;
    };

    struct PortfolioWeightedReplayResult_ {
        double weightedValue_ = 0.0;
        Vector_<double> componentMeans_;
        Vector_<double> gradient_;
        Vector_<PortfolioGroupCounters_> groupCounters_;

        PortfolioWeightedReplayResult_(size_t outputs, size_t inputs, size_t groups)
            : componentMeans_(outputs, 0.0), gradient_(inputs, 0.0), groupCounters_(groups) {}
    };

    [[nodiscard]] PortfolioWeightedReplayResult_ EvaluatePortfolioWeightedReplay(const Script::Detail::PreparedPortfolio_& portfolio,
                                                                                 const Vector_<Script::Detail::PortfolioBatchOutput_>& outputs,
                                                                                 const Vector_<size_t>& selectedInputs);
} // namespace Dal::Detail

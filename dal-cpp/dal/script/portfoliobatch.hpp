//
// Created by Codex on 2026/10/6.
//

#pragma once

#include <dal/script/portfoliopreparation.hpp>
#include <dal/script/weightedrisk.hpp>

namespace Dal::Script {
    struct PathBatch_;

    namespace Detail {
        struct PortfolioBatchOutput_ {
            size_t tradePosition_;
            RiskOutputCoordinate_ coordinate_;
            double weight_;
        };

        struct PortfolioWeightedBatchResult_ {
            double weightedSum_ = 0.0;
            Vector_<double> componentSums_;
            Vector_<double> modelGradientSums_;
            Vector_<size_t> tradePositions_;
            Vector_<Vector_<double>> constantGradientSums_;
            size_t generatedScenarios_ = 0;
            size_t evaluatorCalls_ = 0;
            size_t suffixReversals_ = 0;
            size_t prefixReversals_ = 0;

            explicit PortfolioWeightedBatchResult_(size_t components) : componentSums_(components, 0.0) {}
        };

        [[nodiscard]] PortfolioWeightedBatchResult_ EvaluatePortfolioWeightedBatch(const PreparedPortfolio_& portfolio,
                                                                                   size_t group,
                                                                                   const PathBatch_& batch,
                                                                                   const Vector_<PortfolioBatchOutput_>& outputs);
    } // namespace Detail
} // namespace Dal::Script

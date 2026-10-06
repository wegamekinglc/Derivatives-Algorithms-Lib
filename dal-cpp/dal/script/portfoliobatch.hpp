//
// Created by Codex on 2026/10/6.
//

#pragma once

#include <dal/math/aad/tapecapacity.hpp>
#include <dal/math/buffercapacity.hpp>
#include <dal/script/portfoliopreparation.hpp>
#include <dal/script/weightedrisk.hpp>

namespace Dal::Script {
    struct PathBatch_;

    namespace Detail {
        struct PortfolioCapacityLimits_ {
            std::optional<size_t> scratchBudgetBytes_;
            std::optional<size_t> tapeBudgetBytes_;
        };

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
                                                                                   const Vector_<PortfolioBatchOutput_>& outputs,
                                                                                   BufferCapacityBudget_* scratch = nullptr,
                                                                                   AAD::TapeCapacityBudget_* tape = nullptr);

        [[nodiscard]] size_t PortfolioWeightedWorkerFixedBytes(bool compiled, size_t trades, bool native = true, bool checkedPaths = false);

        void AdmitPortfolioWeightedWorker(const Vector_<const PreparedScript_*>& trades,
                                          const Handle_<ModelData_>& model,
                                          const Vector_<PortfolioBatchOutput_>& outputs,
                                          size_t scratchQuota,
                                          size_t tapeQuota);

        void AdmitPortfolioPassiveWorker(const Vector_<const PreparedScript_*>& trades,
                                         const Handle_<ModelData_>& model,
                                         const Vector_<PortfolioBatchOutput_>& outputs,
                                         size_t scratchQuota);
    } // namespace Detail
} // namespace Dal::Script

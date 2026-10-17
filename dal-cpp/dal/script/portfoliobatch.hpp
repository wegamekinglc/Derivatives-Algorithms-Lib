//
// Created by Codex on 2026/10/6.
//

#pragma once

#include <limits>
#include <optional>
#include <type_traits>
#include <utility>

#include <dal/math/aad/tapecapacity.hpp>
#include <dal/math/buffercapacity.hpp>
#include <dal/script/portfoliopreparation.hpp>
#include <dal/script/weightedrisk.hpp>

namespace Dal::Script {
    struct PathBatch_;
    class BatchPlan_;

    namespace Detail {
        struct PortfolioCapacityLimits_ {
            std::optional<size_t> scratchBudgetBytes_;
            std::optional<size_t> tapeBudgetBytes_;
        };

        class PortfolioInputColumns_ {
            size_t sourceExtent_ = 0;
            std::optional<Vector_<size_t>> ordinals_;

        public:
            PortfolioInputColumns_() = default;
            explicit PortfolioInputColumns_(size_t extent) : sourceExtent_(extent) {}
            PortfolioInputColumns_(size_t extent, Vector_<size_t> ordinals) : sourceExtent_(extent), ordinals_(std::move(ordinals)) {
                for (size_t column = 0; column < ordinals_->size(); ++column)
                    REQUIRE2((*ordinals_)[column] < extent && (column == 0 || (*ordinals_)[column - 1] < (*ordinals_)[column]),
                             "InvalidPortfolioInputs: original ordinals must be increasing and in range", ScriptError_);
            }
            [[nodiscard]] size_t SourceExtent() const { return sourceExtent_; }
            [[nodiscard]] size_t Size() const { return ordinals_ ? ordinals_->size() : sourceExtent_; }
            [[nodiscard]] size_t Original(size_t column) const { return ordinals_ ? (*ordinals_)[column] : column; }
            void ValidateExtent(size_t extent) const {
                REQUIRE2(sourceExtent_ == extent, "InvalidPortfolioInputs: source extent changed", ScriptError_);
            }
        };

        struct PortfolioGradientSelection_ {
            Vector_<PortfolioInputColumns_> models_;
            Vector_<PortfolioInputColumns_> constants_;

            [[nodiscard]] const PortfolioInputColumns_& Model(size_t owner) const {
                REQUIRE2(owner < models_.size(), "InvalidPortfolioInputs: model owner is out of range", ScriptError_);
                return models_[owner];
            }
            [[nodiscard]] const PortfolioInputColumns_& Constants(size_t trade) const {
                REQUIRE2(trade < constants_.size(), "InvalidPortfolioInputs: trade position is out of range", ScriptError_);
                return constants_[trade];
            }
        };

        struct PortfolioBatchSettings_ {
            BufferCapacityBudget_* scratch_ = nullptr;
            AAD::TapeCapacityBudget_* tape_ = nullptr;
            const PortfolioGradientSelection_* gradients_ = nullptr;
        };

        struct PortfolioAdmissionSettings_ {
            size_t width_ = 1;
            size_t scratchQuota_ = std::numeric_limits<size_t>::max();
            size_t tapeQuota_ = std::numeric_limits<size_t>::max();
            const PortfolioGradientSelection_* gradients_ = nullptr;
            size_t modelOwner_ = 0;
            const Vector_<size_t>* tradePositions_ = nullptr;
        };

        struct PortfolioBatchOutput_ {
            size_t tradePosition_;
            RiskOutputCoordinate_ coordinate_;
            double weight_;
        };

        template <class G_> struct PortfolioBatchResult_ {
            double weightedSum_ = 0.0;
            Vector_<double> componentSums_;
            G_ modelGradientSums_;
            Vector_<size_t> tradePositions_;
            Vector_<G_> constantGradientSums_;
            size_t generatedScenarios_ = 0;
            size_t evaluatorCalls_ = 0;
            size_t suffixReversals_ = 0;
            size_t prefixReversals_ = 0;

            explicit PortfolioBatchResult_(size_t components) : componentSums_(components, 0.0) {}
        };

        using PortfolioWeightedBatchResult_ = PortfolioBatchResult_<Vector_<double>>;
        using PortfolioJacobianBatchResult_ = PortfolioBatchResult_<Matrix_<double>>;

        template <class G_> void ResizePortfolioGradientStorage(G_* gradients, size_t width, size_t inputs) {
            if constexpr (std::is_same_v<G_, Vector_<double>>)
                gradients->Resize(inputs);
            else {
                REQUIRE2(inputs <= static_cast<size_t>(std::numeric_limits<int>::max()),
                         "InvalidPortfolioAdmission: input extent exceeds matrix columns", ScriptError_);
                gradients->Resize(static_cast<int>(width), static_cast<int>(inputs));
            }
        }

        [[nodiscard]] PortfolioWeightedBatchResult_ EvaluatePortfolioWeightedBatch(const PreparedPortfolio_& portfolio,
                                                                                   size_t group,
                                                                                   const PathBatch_& batch,
                                                                                   const Vector_<PortfolioBatchOutput_>& outputs,
                                                                                   const PortfolioBatchSettings_& settings = {});

        [[nodiscard]] PortfolioJacobianBatchResult_ EvaluatePortfolioJacobianBatch(const PreparedPortfolio_& portfolio,
                                                                                   size_t group,
                                                                                   const PathBatch_& batch,
                                                                                   const Vector_<PortfolioBatchOutput_>& outputs,
                                                                                   size_t width,
                                                                                   const PortfolioBatchSettings_& settings = {});

        [[nodiscard]] size_t PortfolioWeightedWorkerFixedBytes(bool compiled, size_t trades, bool native = true, bool checkedPaths = false);

        void EvaluatePortfolioWeightedWorker(const PreparedPortfolio_& portfolio,
                                             size_t group,
                                             const BatchPlan_& batches,
                                             size_t worker,
                                             size_t workers,
                                             const Vector_<PortfolioBatchOutput_>& outputs,
                                             Vector_<PortfolioWeightedBatchResult_>* slots,
                                             const PortfolioBatchSettings_& settings = {});

        void EvaluatePortfolioJacobianWorker(const PreparedPortfolio_& portfolio,
                                             size_t group,
                                             const BatchPlan_& batches,
                                             size_t worker,
                                             size_t workers,
                                             const Vector_<PortfolioBatchOutput_>& outputs,
                                             size_t width,
                                             Vector_<PortfolioJacobianBatchResult_>* slots,
                                             const PortfolioBatchSettings_& settings = {});
        [[nodiscard]] size_t PortfolioJacobianWorkerFixedBytes(bool compiled, size_t trades);

        void AdmitPortfolioWeightedWorker(const Vector_<const PreparedScript_*>& trades,
                                          const Handle_<ModelData_>& model,
                                          const Vector_<PortfolioBatchOutput_>& outputs,
                                          const PortfolioAdmissionSettings_& settings);

        void AdmitPortfolioJacobianWorker(const Vector_<const PreparedScript_*>& trades,
                                          const Handle_<ModelData_>& model,
                                          const Vector_<PortfolioBatchOutput_>& outputs,
                                          const PortfolioAdmissionSettings_& settings);

        void AdmitPortfolioPassiveWorker(const Vector_<const PreparedScript_*>& trades,
                                         const Handle_<ModelData_>& model,
                                         const Vector_<PortfolioBatchOutput_>& outputs,
                                         size_t scratchQuota);
    } // namespace Detail
} // namespace Dal::Script

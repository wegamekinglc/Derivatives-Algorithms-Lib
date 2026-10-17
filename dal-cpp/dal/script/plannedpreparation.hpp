//
// Created by Codex on 2026/10/6.
//

#pragma once

#include <dal/script/preparation.hpp>

namespace Dal::Script {
    class PreparedScriptBuilder_;

    namespace Detail {
        class PlannedScript_ {
            PreparedScript_ prepared_;
            ScriptProduct_* product_;
            std::unique_ptr<AAD::Model_<double>> model_;

            // The writable product is owned by prepared_ until completion seals it.
            PlannedScript_(PreparedScript_&& prepared, ScriptProduct_* product, std::unique_ptr<AAD::Model_<double>>&& model)
                : prepared_(std::move(prepared)), product_(product), model_(std::move(model)) {}
            friend class Dal::Script::PreparedScriptBuilder_;

        public:
            [[nodiscard]] const PreparedScript_& View() const { return prepared_; }
            [[nodiscard]] const AAD::Model_<double>& Model() const {
                REQUIRE2(model_, "InvalidPreparationPlan: planning model has been consumed", ScriptError_);
                return *model_;
            }
        };

        [[nodiscard]] PlannedScript_ PlanScript(const ScriptProductData_& product,
                                                std::unique_ptr<AAD::Model_<double>> model,
                                                const ScriptValuationSettings_& valuation = {},
                                                const MonteCarloSettings_& simulation = {});

        [[nodiscard]] PreparedScript_ CompleteScriptPreparation(PlannedScript_&& planned, const Handle_<MarketFixingSnapshot_>& snapshot);
    } // namespace Detail
} // namespace Dal::Script

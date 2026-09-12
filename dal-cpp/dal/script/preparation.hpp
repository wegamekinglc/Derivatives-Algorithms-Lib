//
// Created by Codex on 2026/9/13.
//

#pragma once

#include <dal/indice/fixingsnapshot.hpp>
#include <dal/script/event.hpp>
#include <dal/script/observationplan.hpp>
#include <dal/script/settings.hpp>

namespace Dal::Script {
    class PreparedScript_ {
        std::unique_ptr<const ScriptProduct_> product_;
        Date_ evaluationDate_;
        ScriptValuationSettings_ settings_;
        ObservationPlan_ plan_;

        PreparedScript_(std::unique_ptr<ScriptProduct_>&& product,
                        const Date_& evaluationDate,
                        const ScriptValuationSettings_& settings,
                        ObservationPlan_&& plan)
            : product_(std::move(product)), evaluationDate_(evaluationDate), settings_(settings), plan_(std::move(plan)) {}
        friend PreparedScript_ PrepareScript(const ScriptProductData_&, const ScriptValuationSettings_&, const Handle_<MarketFixingSnapshot_>&);

    public:
        [[nodiscard]] const ScriptProduct_& Product() const { return *product_; }
        [[nodiscard]] const Date_& EvaluationDate() const { return evaluationDate_; }
        [[nodiscard]] const ScriptValuationSettings_& Settings() const { return settings_; }
        [[nodiscard]] const ObservationPlan_& Plan() const { return plan_; }
        [[nodiscard]] bool AllExpired() const { return product_->EventDates().empty(); }
    };

    PreparedScript_ PrepareScript(const ScriptProductData_& product,
                                  const ScriptValuationSettings_& settings = {},
                                  const Handle_<MarketFixingSnapshot_>& snapshot = {});
} // namespace Dal::Script

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
        std::unique_ptr<ObservationPlan_> plan_;
        MonteCarloSettings_ simulation_;
        bool executable_ = false;

        PreparedScript_(std::unique_ptr<ScriptProduct_>&& product,
                        const Date_& evaluationDate,
                        const ScriptValuationSettings_& settings,
                        ObservationPlan_&& plan)
            : product_(std::move(product)), evaluationDate_(evaluationDate), settings_(settings),
              plan_(std::make_unique<ObservationPlan_>(std::move(plan))) {}
        friend class PreparedScriptBuilder_;

    public:
        [[nodiscard]] const ScriptProduct_& Product() const { return *product_; }
        [[nodiscard]] const Date_& EvaluationDate() const { return evaluationDate_; }
        [[nodiscard]] const ScriptValuationSettings_& Settings() const { return settings_; }
        [[nodiscard]] const ObservationPlan_& Plan() const { return *plan_; }
        [[nodiscard]] bool AllExpired() const { return product_->EventDates().empty(); }
        [[nodiscard]] const MonteCarloSettings_& Simulation() const { return simulation_; }
        [[nodiscard]] const Vector_<Date_>& EventDates() const { return product_->EventDates(); }
        [[nodiscard]] const Vector_<String_>& ConstVarNames() const { return product_->ConstVarNames(); }
        [[nodiscard]] const Vector_<>& TimeLine() const { return plan_->TimeLine(); }
        [[nodiscard]] const Vector_<AAD::SampleDef_>& DefLine() const { return plan_->DefLine(); }
        [[nodiscard]] size_t PayOffIdx() const { return product_->PayOffIdx(); }
        void RequireExecutable() const {
            REQUIRE2(executable_ || AllExpired(), "UnsupportedExecutionMode: history-only preparation has no model plan", ScriptError_);
        }
        template <class T_> Evaluator_<T_> BuildEvaluator() const { return product_->BuildEvaluator<T_>(); }
        template <class T_> EvalState_<T_> BuildEvalState() const { return product_->BuildEvalState<T_>(); }
        [[nodiscard]] ScriptCompiled_ Compile() const {
            REQUIRE2(plan_->Requests().empty(), "UnsupportedExecutionMode: named compiled evaluation", ScriptError_);
            return product_->Compile();
        }
        void Evaluate(const AAD::Scenario_<double>& scenario, Evaluator_<double>& eval) const {
            RequireExecutable();
            eval.SetScenario(&scenario);
            eval.SetObservations(plan_.get());
            eval.Init();
            for (size_t event = 0; event < product_->Events().size(); ++event) {
                eval.SetCurEvt(plan_->EventToSample()[event]);
                for (const auto& statement : product_->Events()[event])
                    statement->Accept(eval);
            }
        }
    };

    PreparedScript_ PrepareScript(const ScriptProductData_& product,
                                  const ScriptValuationSettings_& settings = {},
                                  const Handle_<MarketFixingSnapshot_>& snapshot = {});

    PreparedScript_ PrepareScript(const ScriptProductData_& product,
                                  AAD::Model_<double>* model,
                                  const ScriptValuationSettings_& settings,
                                  const MonteCarloSettings_& simulation,
                                  const Handle_<MarketFixingSnapshot_>& snapshot = {},
                                  const ScriptProductSettings_& contract = {});
} // namespace Dal::Script

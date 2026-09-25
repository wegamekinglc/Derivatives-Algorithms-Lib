//
// Created by Codex on 2026/9/13.
//

#pragma once

#include <dal/indice/fixingsnapshot.hpp>
#include <dal/model/base.hpp>
#include <dal/script/event.hpp>
#include <dal/script/observationplan.hpp>
#include <dal/script/settings.hpp>

namespace Dal::Script {
    class PreparedScript_ {
        std::unique_ptr<const ScriptProduct_> product_;
        Date_ evaluationDate_;
        ScriptValuationSettings_ settings_;
        std::shared_ptr<ObservationPlan_> plan_;
        MonteCarloSettings_ simulation_;
        bool executable_ = false;
        size_t maxNestedIfs_ = 0;
        std::optional<ScriptCompiled_> compiled_;
        std::optional<ScriptCompiled_> pastCompiled_;

        PreparedScript_(std::unique_ptr<ScriptProduct_>&& product,
                        const Date_& evaluationDate,
                        const ScriptValuationSettings_& settings,
                        ObservationPlan_&& plan)
            : product_(std::move(product)), evaluationDate_(evaluationDate), settings_(settings),
              plan_(std::make_shared<ObservationPlan_>(std::move(plan))) {}
        friend class PreparedScriptBuilder_;

    public:
        [[nodiscard]] const ScriptProduct_& Product() const { return *product_; }
        [[nodiscard]] const Date_& EvaluationDate() const { return evaluationDate_; }
        [[nodiscard]] const ScriptValuationSettings_& Settings() const { return settings_; }
        [[nodiscard]] const ObservationPlan_& Plan() const { return *plan_; }
        //  Shared ownership for building additional compiled artifacts over the sealed plan
        [[nodiscard]] std::shared_ptr<const ObservationPlan_> PlanHandle() const { return plan_; }
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
        //  The ignored first parameter keeps generic call sites (shared with ScriptProduct_) compiling; preparation's maxNestedIfs_ is used
        template <class T_> EvalState_<T_> BuildEvalState(size_t = 0, double eps = 0.0) const {
            REQUIRE2(eps == 0.0 || eps == simulation_.smooth_, "UnsupportedExecutionMode: smoothing differs from preparation", ScriptError_);
            return product_->BuildEvalState<T_>(maxNestedIfs_, eps == 0.0 ? simulation_.smooth_ : eps);
        }
        //  Same: the ignored int keeps generic call sites compiling; preparation's maxNestedIfs_ is authoritative
        template <class T_> FuzzyEvaluator_<T_> BuildFuzzyEvaluator(int, double eps) const {
            REQUIRE2(simulation_.enableAad_ && eps == simulation_.smooth_, "UnsupportedExecutionMode: smoothing differs from preparation",
                     ScriptError_);
            return product_->BuildFuzzyEvaluator<T_>(static_cast<int>(maxNestedIfs_), eps);
        }
        template <class E_> void InitializeHistoricalState(E_* evaluator) const {
            PastEvaluator_<AAD::Number_> past(Vector_<>(product_->VarNames().size(), 0.0), evaluator->ConstVarVals(), product_->VectorCapacities());
            past.SetObservations(plan_.get());
            product_->Visit(past, true, false);
            evaluator->SetHistoricalSeed(past.VarVals());
            evaluator->SetHistoricalVectorSeed(past.VectorVals());
        }
        template <class T_> void InitializeHistoricalState(EvalState_<T_>* evaluator) const {
            REQUIRE2(pastCompiled_, "PreparationRequired: historical bytecode is not prepared", ScriptError_);
            EvalState_<T_> past(Vector_<>(product_->VarNames().size(), 0.0), evaluator->ConstVarVals(), 0, 0.0, product_->VectorCapacities());
            pastCompiled_->Evaluate(AAD::Scenario_<T_>(), past);
            evaluator->SetHistoricalSeed(past.VarVals());
            evaluator->SetHistoricalVectorSeed(past.VectorVals());
        }
        //  Borrow the prepared program for an evaluation whose lifetime stays
        //  inside this PreparedScript_; Compile() retains its owning-copy API.
        [[nodiscard]] const ScriptCompiled_& CompiledProgram(bool fuzzy = false) const {
            REQUIRE2(compiled_ && fuzzy == simulation_.enableAad_, "UnsupportedExecutionMode: compilation differs from preparation", ScriptError_);
            return *compiled_;
        }
        [[nodiscard]] ScriptCompiled_ Compile(bool fuzzy = false) const {
            REQUIRE2(compiled_ && fuzzy == simulation_.enableAad_, "UnsupportedExecutionMode: compilation differs from preparation", ScriptError_);
            return *compiled_;
        }
        template <class T_, class E_> void Evaluate(const AAD::Scenario_<T_>& scenario, E_& eval) const {
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

//
// Created by wegam on 2022/4/4.
//

#pragma once

#include <dal/math/aad/sample.hpp>
#include <dal/math/vectors.hpp>
#include <dal/script/node.hpp>
#include <dal/script/settings.hpp>
#include <dal/script/visitor/all.hpp>
#include <dal/storage/archive.hpp>
#include <dal/time/date.hpp>
#include <dal/utilities/algorithms.hpp>
#include <regex>
#include <utility>

/*IF--------------------------------------------------------------------------
storable ScriptProductData
   data for script product from an events table
version 2
manual
&members
name is ?string
dates is cell[]
events is string[]
default_index is ?string
-IF-------------------------------------------------------------------------*/

namespace Dal::Script {
    using AAD::Scenario_;

    template <class T_> Vector_<Vector_<T_>> TypedVectorValues(const Vector_<Vector_<>>& values) {
        Vector_<Vector_<T_>> result(values.size());
        for (size_t i = 0; i < values.size(); ++i) {
            result[i].reserve(values[i].size());
            for (const double value : values[i])
                result[i].emplace_back(value);
        }
        return result;
    }

    //  Flat per-event artifact produced by ScriptProduct_::Compile().
    class ScriptCompiled_ {
        Vector_<Vector_<int>> nodeStreams_;
        Vector_<Vector_<>> constStreams_;
        std::shared_ptr<const ObservationPlan_> observations_;
        bool historical_ = false;
        bool legacy_ = false;

        template <bool Prepared_, class T_, class S_> void EvaluateSamples(const S_& sampleAt, EvalState_<T_>* state) const {
            Detail::EvalCompiledEvents<Prepared_>(
                nodeStreams_.size(), [&](size_t i) { return Detail::CompiledEventView_<T_>{nodeStreams_[i], constStreams_[i], sampleAt(i)}; }, state);
        }

    public:
        ScriptCompiled_(Vector_<Vector_<int>>&& nodeStreams, Vector_<Vector_<>>&& constStreams)
            : nodeStreams_(std::move(nodeStreams)), constStreams_(std::move(constStreams)) {}

        [[nodiscard]] const Vector_<Vector_<int>>& NodeStreams() const { return nodeStreams_; }
        [[nodiscard]] const Vector_<Vector_<>>& ConstStreams() const { return constStreams_; }

        //  lsmc lowers PAYS/EXERCISE into the LSMC recording opcodes; only the prepared
        //  future artifact of an EXERCISE product uses it (S8 keeps EXERCISE out of history)
        static ScriptCompiled_ Build(const Vector_<Event_>& events,
                                     bool fuzzy,
                                     std::shared_ptr<const ObservationPlan_> observations = {},
                                     bool historical = false,
                                     bool lsmc = false) {
            Vector_<Vector_<int>> nodes;
            Vector_<Vector_<>> constants;
            for (const auto& event : events) {
                Compiler_ compiler(fuzzy, observations.get(), historical, lsmc);
                for (const auto& statement : event)
                    statement->Accept(compiler);
                nodes.push_back(compiler.NodeStream());
                constants.push_back(compiler.ConstStream());
            }
            ScriptCompiled_ result(std::move(nodes), std::move(constants));
            result.observations_ = std::move(observations);
            result.historical_ = historical;
            // Only compiler-produced legacy streams are known to exclude prepared opcodes.
            result.legacy_ = !result.observations_ && !historical;
            return result;
        }

        template <class T_> void Evaluate(const Scenario_<T_>& scenario, EvalState_<T_>& state) const { EvaluateImpl(scenario, state); }

    private:
        template <class T_> void EvaluateImpl(const Scenario_<T_>& scenario, EvalState_<T_>& state) const {
            state.Init();
            state.observations_ = observations_.get();
            state.scenario_ = &scenario;
            if (legacy_) {
                EvaluateSamples<false>([&](size_t i) -> const auto& { return scenario[i]; }, &state);
                return;
            }
            const AAD::Sample_<T_> pastSample{};
            EvaluateSamples<true>(
                [&](size_t i) -> const auto& { return historical_ ? pastSample : scenario[observations_ ? observations_->EventToSample()[i] : i]; },
                &state);
        }
    };

    class ScriptProduct_ {
        String_ payoff_;
        size_t payoffIdx_;

        Vector_<Date_> parsedEventDates_;
        Vector_<Vector_<SourceOrigin_>> parsedEventSources_;
        std::optional<Date_> evaluationDate_;
        Vector_<Date_> pastEventDates_;
        Vector_<Event_> pastEvents_;
        Vector_<Date_> eventDates_;
        Vector_<Event_> events_;
        Vector_<> variableValues_;
        Vector_<String_> variables_;
        Vector_<String_> consVariables_;
        Vector_<> consVariablesValues_;
        Vector_<String_> vectorNames_;
        Vector_<size_t> vectorCapacities_;
        Vector_<Vector_<>> vectorValues_;

        Vector_<> timeLine_;
        Vector_<AAD::SampleDef_> defLine_;

        // Recorded while parsing so payoff queries stay O(1) on hot paths (dump, indexing, gates)
        bool hasPays_ = false;
        bool hasExercise_ = false;

        //  Set by PreProcess().
        bool preProcessed_ = false;
        String_ preparationError_;
        [[noreturn]] void ThrowPreparationError() const;
        void RequirePreparedFixings() const {
            if (!preparationError_.empty())
                ThrowPreparationError();
        }

    public:
        ScriptProduct_(const Vector_<Cell_>& dates, const Vector_<String_>& events, String_ payoff = "")
            : payoff_(std::move(payoff)), payoffIdx_(-1) {
            REQUIRE2(dates.size() == events.size(), "dates size is not equal to events size", ScriptError_);
            auto dateEvents = Zip(dates, events);
            ParseEvents(dateEvents);
        }

        [[nodiscard]] const Vector_<Date_>& PastEventDates() const { return pastEventDates_; }
        [[nodiscard]] const Vector_<Date_>& ParsedEventDates() const { return parsedEventDates_; }
        [[nodiscard]] const Vector_<Vector_<SourceOrigin_>>& ParsedEventSources() const { return parsedEventSources_; }
        // PAYS or EXERCISE constitute a payoff; HasPays routes the payoff receiver slot
        [[nodiscard]] bool HasPayoff() const { return hasPays_ || hasExercise_; }
        [[nodiscard]] bool HasPays() const { return hasPays_; }
        [[nodiscard]] bool ContainsExercise() const { return hasExercise_; }
        [[nodiscard]] const std::optional<Date_>& EvaluationDate() const { return evaluationDate_; }
        [[nodiscard]] const Vector_<Event_>& PastEvents() const { return pastEvents_; }
        [[nodiscard]] const Vector_<Date_>& EventDates() const { return eventDates_; }
        [[nodiscard]] const Vector_<Event_>& Events() const { return events_; }
        [[nodiscard]] const Vector_<String_>& VarNames() const { return variables_; }
        [[nodiscard]] const Vector_<>& VarValues() const { return variableValues_; }
        [[nodiscard]] const Vector_<String_>& ConstVarNames() const { return consVariables_; }
        [[nodiscard]] const Vector_<>& ConstVarValues() const { return consVariablesValues_; }
        [[nodiscard]] const Vector_<String_>& VectorNames() const { return vectorNames_; }
        [[nodiscard]] const Vector_<size_t>& VectorCapacities() const { return vectorCapacities_; }
        [[nodiscard]] const Vector_<Vector_<>>& VectorValues() const { return vectorValues_; }
        [[nodiscard]] const Vector_<>& TimeLine() const { return timeLine_; }
        [[nodiscard]] const Vector_<AAD::SampleDef_>& DefLine() const { return defLine_; }

        template <class T_> Evaluator_<T_> BuildEvaluator() const {
            Evaluator_<T_> evaluator(variableValues_, Apply([](double x) { return T_(x); }, consVariablesValues_), vectorCapacities_);
            evaluator.SetHistoricalVectorSeed(TypedVectorValues<T_>(vectorValues_));
            return evaluator;
        }

        template <class T_> FuzzyEvaluator_<T_> BuildFuzzyEvaluator(int maxNestedIfs, double defEps) const {
            FuzzyEvaluator_<T_> evaluator(variableValues_, Apply([](double x) { return T_(x); }, consVariablesValues_), maxNestedIfs, defEps,
                                          vectorCapacities_);
            evaluator.SetHistoricalVectorSeed(TypedVectorValues<T_>(vectorValues_));
            return evaluator;
        }

        template <class T_> EvalState_<T_> BuildEvalState(size_t maxNestedIfs = 0, double defEps = 0.0) const {
            EvalState_<T_> state(variableValues_, Apply([](double x) { return T_(x); }, consVariablesValues_), maxNestedIfs, defEps,
                                 vectorCapacities_);
            state.SetHistoricalVectorSeed(TypedVectorValues<T_>(vectorValues_));
            return state;
        }

        template <class T_> std::unique_ptr<Scenario_<T_>> BuildScenario() const {
            return std::unique_ptr<Scenario_<T_>>(new Scenario_<T_>(eventDates_.size()));
        }

        void ParseEvents(const Vector_<std::pair<Cell_, String_>>& events);
        void PartitionEvents(const Date_& evaluationDate);
        void RequireExecutable() const {
            RequirePreparedFixings();
            REQUIRE2(preProcessed_, "product is not pre-processed: call PreProcess() before simulation", ScriptError_);
        }

        template <class V_> void Visit(Visitor_<V_>& v, bool past = true, bool future = true) {
            if (past)
                for (auto& evt : pastEvents_)
                    for (auto& stat : evt)
                        stat->Accept(static_cast<V_&>(v));

            if (future)
                for (auto& evt : events_)
                    for (auto& stat : evt)
                        stat->Accept(static_cast<V_&>(v));
        }

        template <class V_> void Visit(ConstVisitor_<V_>& v, bool past = true, bool future = true) const {
            if (past)
                for (const auto& evt : pastEvents_)
                    for (const auto& stat : evt)
                        stat->Accept(static_cast<V_&>(v));

            if (future)
                for (const auto& evt : events_)
                    for (const auto& stat : evt)
                        stat->Accept(static_cast<V_&>(v));
        }

        template <class T_, class E_> void Evaluate(const Scenario_<T_>& scenario, E_& eval) const { EvaluateImpl(scenario, eval); }

    private:
        template <class T_, class E_> void EvaluateImpl(const Scenario_<T_>& scenario, E_& eval) const {
            RequirePreparedFixings();
            eval.SetScenario(&scenario);
            eval.Init();
            for (size_t i = 0; i < events_.size(); ++i) {
                eval.SetCurEvt(i);
                for (auto& statIt : events_[i])
                    statIt->Accept(eval);
            }
        }

    public:
        void IndexVariables();
        void InitializePastObservations(const ObservationPlan_& plan);
        [[nodiscard]] Vector_<> PastEvaluate() const;
        size_t IFProcess();
        void DomainProcess(bool fuzzy);
        void ConstProcess();
        void ConstCondProcess();
        void OptimizeLsmc();
        void ValidateFuzzyVectorMutations() const;

        size_t PreProcess(bool fuzzy, bool skip_domain);
        void Debug(std::ostream& ost = std::cout) const;
        //  Machine-friendly JSON dump, schema dal.script-product/1; includes past events
        void DebugJson(std::ostream& ost = std::cout) const;
        //  Human-friendly tree dump; `width` caps the line width, `ascii` selects the fallback style
        void DebugTree(std::ostream& ost = std::cout, bool ascii = false, int width = 125) const;
        [[nodiscard]] ScriptCompiled_ Compile(bool fuzzy = false) const;

        [[nodiscard]] auto PayOffIdx() const { return payoffIdx_; }
    };

    class ScriptProductData_ : public Storable_ {
        Vector_<Cell_> eventDates_;
        Vector_<String_> eventDesc_;
        ScriptProductSettings_ settings_;

    public:
        ScriptProductData_(const String_& name, const Vector_<Cell_>& dates, const Vector_<String_>& events)
            : ScriptProductData_(name, dates, events, {}) {}
        ScriptProductData_(const String_& name, const Vector_<Cell_>& dates, const Vector_<String_>& events, const ScriptProductSettings_& settings)
            : Storable_("ScriptProduct", name), eventDates_(dates), eventDesc_(events), settings_(settings) {
            REQUIRE2(dates.size() == events.size(),
                     "InvalidSetting: dates.size=" + String_(std::to_string(dates.size())) +
                         "; events.size=" + String_(std::to_string(events.size())) + "; expected equal lengths",
                     ScriptError_);
        }
        void Write(Archive::Store_& dst) const override;
        [[nodiscard]] const ScriptProductSettings_& Settings() const { return settings_; }
        [[nodiscard]] const Vector_<Cell_>& Dates() const { return eventDates_; }
        [[nodiscard]] const Vector_<String_>& EventTexts() const { return eventDesc_; }
        [[nodiscard]] ScriptProduct_ Product() const { return {eventDates_, eventDesc_, ""}; }
    };

    // Keep tree AAD execution in core to avoid expanding recording loops in public callers.
    template <>
    void ScriptProduct_::Evaluate<AAD::Number_, FuzzyEvaluator_<AAD::Number_>>(const Scenario_<AAD::Number_>& scenario,
                                                                               FuzzyEvaluator_<AAD::Number_>& eval) const;
} // namespace Dal::Script

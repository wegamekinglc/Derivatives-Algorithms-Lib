//
// Created by wegam on 2022/4/4.
//

#pragma once

#include <regex>
#include <utility>
#include <dal/math/aad/sample.hpp>
#include <dal/math/vectors.hpp>
#include <dal/script/node.hpp>
#include <dal/script/visitor/all.hpp>
#include <dal/storage/archive.hpp>
#include <dal/time/date.hpp>
#include <dal/utilities/algorithms.hpp>



/*IF--------------------------------------------------------------------------
storable ScriptProductData
   data for script product from an events table
version 1
&members
name is ?string
dates is cell[]
events is string[]
-IF-------------------------------------------------------------------------*/

namespace Dal::Script {
    using AAD::Scenario_;

    //  Flat per-event artifact produced by ScriptProduct_::Compile().
    class ScriptCompiled_ {
        Vector_<Vector_<int>> nodeStreams_;
        Vector_<Vector_<>> constStreams_;

    public:
        ScriptCompiled_(Vector_<Vector_<int>>&& nodeStreams,
                        Vector_<Vector_<>>&& constStreams)
            : nodeStreams_(std::move(nodeStreams)),
              constStreams_(std::move(constStreams)) {}

        [[nodiscard]] const Vector_<Vector_<int>>& NodeStreams() const { return nodeStreams_; }

        template <class T_> void Evaluate(const Scenario_<T_>& scenario, EvalState_<T_>& state) const {
            state.Init();
            for (size_t i = 0; i < nodeStreams_.size(); ++i)
                EvalCompiled(nodeStreams_[i],
                             constStreams_[i],
                             scenario[i],
                             state);
        }
    };

    class ScriptProduct_ {
        String_ payoff_;
        size_t payoffIdx_;

        Vector_<Date_> pastEventDates_;
        Vector_<Event_> pastEvents_;
        Vector_<Date_> eventDates_;
        Vector_<Event_> events_;
        Vector_<> variableValues_;
        Vector_<String_> variables_;
        Vector_<String_> consVariables_;
        Vector_<> consVariablesValues_;

        Vector_<> timeLine_;
        Vector_<AAD::SampleDef_> defLine_;

        //  Set by PreProcess().
        bool preProcessed_ = false;

    public:
        ScriptProduct_(const Vector_<Cell_>& dates, const Vector_<String_>& events, String_ payoff = "")
        : payoff_(std::move(payoff)), payoffIdx_(-1) {
            REQUIRE2(dates.size() == events.size(), "dates size is not equal to events size", ScriptError_);
            auto dateEvents = Zip(dates, events);
            ParseEvents(dateEvents);
        }

        [[nodiscard]] const Vector_<Date_>& PastEventDates() const { return pastEventDates_; }
        [[nodiscard]] const Vector_<Event_>& PastEvents() const { return pastEvents_; }
        [[nodiscard]] const Vector_<Date_>& EventDates() const { return eventDates_; }
        [[nodiscard]] const Vector_<Event_>& Events() const { return events_; }
        [[nodiscard]] const Vector_<String_>& VarNames() const { return variables_; }
        [[nodiscard]] const Vector_<>& VarValues() const { return variableValues_; }
        [[nodiscard]] const Vector_<String_>& ConstVarNames() const { return consVariables_; }
        [[nodiscard]] const Vector_<>& TimeLine() const { return timeLine_; }
        [[nodiscard]] const Vector_<AAD::SampleDef_>& DefLine() const { return defLine_; }

        template <class T_> Evaluator_<T_> BuildEvaluator() const {
            return Evaluator_<T_>(variableValues_,
                                  Apply([](double x) {return T_(x);}, consVariablesValues_));
        }

        template <class T_> FuzzyEvaluator_<T_> BuildFuzzyEvaluator(int maxNestedIfs, double defEps) const {
            return FuzzyEvaluator_<T_>(variableValues_,
                                       Apply([](double x) {return T_(x);}, consVariablesValues_),
                                       maxNestedIfs,
                                       defEps);
        }

        template <class T_> EvalState_<T_> BuildEvalState(size_t maxNestedIfs = 0, double defEps = 0.0) const {
            return EvalState_<T_>(variableValues_,
                                  Apply([](double x) {return T_(x);}, consVariablesValues_),
                                  maxNestedIfs,
                                  defEps);
        }

        template <class T_> std::unique_ptr<Scenario_<T_>> BuildScenario() const {
            return std::unique_ptr<Scenario_<T_>>(new Scenario_<T_>(eventDates_.size()));
        }

        void ParseEvents(const Vector_<std::pair<Cell_, String_>>& events);

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

        template <class T_, class E_> void Evaluate(const Scenario_<T_>& scenario, E_& eval) const {
            eval.SetScenario(&scenario);
            eval.Init();
            for (size_t i = 0; i < events_.size(); ++i) {
                eval.SetCurEvt(i);
                for (auto& statIt : events_[i])
                    statIt->Accept(eval);
            }
        }

        void IndexVariables();
        [[nodiscard]] Vector_<> PastEvaluate() const;
        size_t IFProcess();
        void DomainProcess(bool fuzzy);
        void ConstProcess();
        void ConstCondProcess();

        size_t PreProcess(bool fuzzy, bool skip_domain);
        void Debug(std::ostream& ost = std::cout) const;
        [[nodiscard]] ScriptCompiled_ Compile(bool fuzzy = false) const;

        [[nodiscard]] auto PayOffIdx() const { return payoffIdx_; }
    };

    class ScriptProductData_ : public Storable_ {
        Vector_<Cell_> eventDates_;
        Vector_<String_> eventDesc_;

    public:
        ScriptProductData_(const String_& name, const Vector_<Cell_>& dates, const Vector_<String_>& events)
            : Storable_("ScriptProduct", name), eventDates_(dates), eventDesc_(events) {}
        void Write(Archive::Store_& dst) const override;
        [[nodiscard]] ScriptProduct_ Product() const { return {eventDates_, eventDesc_, ""}; }
    };
} // namespace Dal::Script

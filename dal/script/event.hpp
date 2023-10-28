//
// Created by wegam on 2022/4/4.
//

#pragma once

#include <regex>
#include <utility>
#include <dal/math/aad/sample.hpp>
#include <dal/math/vectors.hpp>
#include <dal/script/node.hpp>
#include <dal/script/parser.hpp>
#include <dal/script/visitor/all.hpp>
#include <dal/storage/archive.hpp>
#include <dal/storage/globals.hpp>
#include <dal/time/date.hpp>
#include <dal/utilities/algorithms.hpp>
#include <dal/time/schedules.hpp>
#include <dal/time/dateutils.hpp>
#include <dal/time/dateincrement.hpp>
#include <dal/time/holidays.hpp>

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
    using Dal::AAD::Scenario_;

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
        Vector_<double> consVariablesValues_;

        Vector_<double> timeLine_;
        Vector_<Dal::AAD::SampleDef_> defLine_;

        //  Compiled form
        Vector_<Vector_<int>> nodeStreams_;
        Vector_<Vector_<double>> constStreams_;
        Vector_<Vector_<const void*>> dataStreams_;

    public:
        ScriptProduct_(const Vector_<Cell_>& dates, const Vector_<String_>& events, String_ payoff = "")
        : payoff_(std::move(payoff)), payoffIdx_(-1) {
            REQUIRE2(dates.size() == events.size(), "dates size is not equal to events size", ScriptError_);
            auto dateEvents = Dal::Zip(dates, events);
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
        [[nodiscard]] const Vector_<Dal::AAD::SampleDef_>& DefLine() const { return defLine_; }

        template <class T_> Evaluator_<T_> BuildEvaluator() const {
            return Evaluator_<T_>(variableValues_,
                                  Apply([](double x) {return T_(x);}, consVariablesValues_));
        }

        template <class T_> FuzzyEvaluator_<T_> BuildFuzzyEvaluator(int max_nested_ifs, double def_eps) const {
            return FuzzyEvaluator_<T_>(variableValues_,
                                       Apply([](double x) {return T_(x);}, consVariablesValues_),
                                       max_nested_ifs,
                                       def_eps);
        }

        template <class T_> EvalState_<T_> BuildEvalState() const {
            return EvalState_<T_>(variableValues_,
                                  Apply([](double x) {return T_(x);}, consVariablesValues_));
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
            // evaluation will only do on future events
            eval.SetScenario(&scenario);
            eval.Init();
            for (size_t i = 0; i < events_.size(); ++i) {
                eval.SetCurEvt(i);
                for (auto& statIt : events_[i])
                    statIt->Accept(eval);
            }
        }

        template <class T_> void EvaluateCompiled(const Scenario_<T_>& scenario, EvalState_<T_>& state) const {
            // Initialize state
            state.Init();

            // Loop over events
            for (size_t i = 0; i < events_.size(); ++i)
                // Evaluate the compiled events
                EvalCompiled(nodeStreams_[i],
                             constStreams_[i],
                             dataStreams_[i],
                             scenario[i],
                             state);
        }

        void IndexVariables();
        Vector_<> PastEvaluate();
        size_t IFProcess();
        void DomainProcess(bool fuzzy);
        void ConstProcess();
        void ConstCondProcess();

        size_t PreProcess(bool fuzzy, bool skip_domain);
        void Debug(std::ostream& ost = std::cout) const;
        void Compile();

        [[nodiscard]] auto PayOffIdx() const { return payoffIdx_; }
    };

    class ScriptProductData_ : public Storable_ {
        Vector_<Cell_> eventDates_;
        Vector_<String_> eventDesc_;

    public:
        ScriptProductData_(const String_& name, const Vector_<Cell_>& dates, const Vector_<String_>& events)
            : Storable_("ScriptProduct", name), eventDates_(dates), eventDesc_(events) {}
        void Write(Archive::Store_& dst) const override;
        ScriptProduct_ Product() const { return {eventDates_, eventDesc_}; }
    };
} // namespace Dal::Script

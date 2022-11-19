//
// Created by wegam on 2022/4/4.
//

#pragma once

#include <dal/math/vectors.hpp>
#include <dal/storage/archive.hpp>
#include <dal/time/date.hpp>
#include <dal/script/node.hpp>
#include <dal/script/visitor/evaluator.hpp>
#include <dal/script/visitor/fuzzy.hpp>
#include <dal/script/visitor/varindexer.hpp>
#include <dal/script/visitor/ifprocessor.hpp>
#include <dal/script/visitor/domainproc.hpp>
#include <dal/script/visitor/constcondprocessor.hpp>
#include <dal/script/visitor/debugger.hpp>
#include <dal/script/parser.hpp>
#include <dal/math/aad/sample.hpp>


/*IF--------------------------------------------------------------------------
storable ScriptProduct
        Script product from an events table
version 1
&members
name is ?string
dates is date[]
events is string[]
-IF-------------------------------------------------------------------------*/


namespace Dal::Script {
    using Dal::AAD::Scenario_;

    using Event_ = Vector_<ScriptNode_>;

    class ScriptProduct_ : public Storable_ {
        Vector_<Date_> eventDates_;
        Vector_<Event_> events_;
        Vector_<String_> eventStrings_;
        Vector_<String_> variables_;

        Vector_<double> timeLine_;
        Vector_<Dal::AAD::SampleDef_> defLine_;

    public:
        ScriptProduct_(const String_& name = ""): Storable_("ScriptProduct", name) {}
        ScriptProduct_(const String_& name, const std::map<Date_, String_>& events)
        : Storable_("ScriptProduct", name) { ParseEvents(events.begin(), events.end()); }

        ScriptProduct_(const String_& name, const Vector_<Date_>& dates, const Vector_<String_>& events)
        : Storable_("ScriptProduct", name) {
            REQUIRE(dates.size() == events.size(), "");
            for (int i = 0; i < dates.size(); ++i) {
                eventDates_.push_back(dates[i]);
                eventStrings_.push_back(events[i]);
                events_.push_back(Parse(events[i]));
            }
        }

        [[nodiscard]] const Vector_<Date_> &EventDates() const { return eventDates_; }
        [[nodiscard]] const Vector_<String_> &VarNames() const { return variables_; }
        [[nodiscard]] const Vector_<Time_>& TimeLine() const { return timeLine_; }
        [[nodiscard]] const Vector_<Dal::AAD::SampleDef_>& DefLine() const { return defLine_; }

        template<class T_>
        std::unique_ptr<Evaluator_<T_>> BuildEvaluator() const {
            return std::unique_ptr<Evaluator_<T_>>(new Evaluator_<T_>(variables_.size()));
        }

        template<class T_>
        std::unique_ptr<Evaluator_<T_>> BuildFuzzyEvaluator(int maxNestedIfs, double defEps) const {
            return std::unique_ptr<Evaluator_<T_>>(new FuzzyEvaluator_<T_>(variables_.size(), maxNestedIfs, defEps));
        }

        template<class T_>
        std::unique_ptr<Scenario_<T_>> BuildScenario() const {
            return std::unique_ptr<Scenario_<T_>>(new Scenario_<T_>(eventDates_.size()));
        }

        template<class EvtIt_>
        void ParseEvents(EvtIt_ begin, EvtIt_ end) {
            for (EvtIt_ evtIt = begin; evtIt != end; ++evtIt) {
                eventDates_.push_back(evtIt->first);
                eventStrings_.push_back(evtIt->second);
                events_.push_back(Parse(evtIt->second));
            }
        }

        template <class V_>
        void Visit(V_& v) {
            for (auto &evt: events_) {
                for (auto &stat: evt)
                    v.Visit(stat);
            }
        }

        template<class T_>
        void Evaluate(const Scenario_<T_> &scen, Evaluator_<T_> &eval) const {
            eval.SetScenario(&scen);
            eval.Init();
            for (size_t i = 0; i < events_.size(); ++i) {
                eval.SetCurEvt(i);
                for (auto &statIt: events_[i])
                    eval.Visit(statIt);
            }
        }

        void IndexVariables();
        size_t IFProcess();
        void DomainProcess(bool fuzzy);
        void ConstCondProcess();

        size_t PreProcess(bool fuzzy, bool skipDoms);
        void Debug(std::ostream &ost);

        void Write(Archive::Store_& dst) const override;
    };
}

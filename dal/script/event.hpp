//
// Created by wegam on 2022/4/4.
//

#pragma once

#include <dal/math/vectors.hpp>
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


namespace Dal::Script {
    using Dal::AAD::Scenario_;

    using Event_ = Vector_<ScriptNode_>;

    class ScriptProduct_ {
        Vector_<Date_> eventDates_;
        Vector_<Event_> events_;
        Vector_<String_> variables_;

        Vector_<double> timeLine_;
        Vector_<Dal::AAD::SampleDef_> defLine_;

    public:
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
    };

}

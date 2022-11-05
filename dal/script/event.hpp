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
#include <dal/math/aad/sample.hpp>


namespace Dal::Script {
    using Dal::AAD::Scenario_;
    using Event_ = Vector_<std::unique_ptr<ScriptNode_>>;

    class ScriptProduct_ {
        Vector_<Date_> eventDates_;
        Vector_<Event_> events_;
        Vector_<String_> variables_;

    public:
        [[nodiscard]] const Vector_<Date_> &EventDates() const { return eventDates_; }

        [[nodiscard]] const Vector_<String_> &VarNames() const { return variables_; }

        template<class T_>
        std::unique_ptr<Evaluator_<T_>> BuildEvaluator() {
            return std::unique_ptr<Evaluator_<T_>>(new Evaluator_<T_>(variables_.size()));
        }

        template<class T_>
        std::unique_ptr<Evaluator_<T_>> BuildFuzzyEvaluator(const size_t maxNestedIfs, const double defEps) {
            return std::unique_ptr<Evaluator_<T_>>(new FuzzyEvaluator_<T_>(variables_.size(), maxNestedIfs, defEps));
        }

        template<class T_>
        std::unique_ptr<Scenario_<T_>> BuildScenario() {
            return std::unique_ptr<Scenario_<T_>> (new Scenario_<T_>(eventDates_.size()));
        }

        template<class EvtIt_>
        void ParseEvents(EvtIt_ begin, EvtIt_ end) {
            for (EvtIt_ evtIt = begin; evtIt != end; ++evtIt) {
                eventDates_.push_back(evtIt->first);
                events_.push_back(parse(evtIt->second));
            }
        }

        void Visit(Visitor_& v);

        template<class T_>
        void Evaluate(const Scenario_<T_> &scen, Evaluator_<T_> &eval) const {
            eval.SetScenario(&scen);
            eval.Init();
            for (size_t i = 0; i < events_.size(); ++i) {
                eval.SetCurEvt(i);
                for (auto &statIt: events_[i])
                    eval.VisitTree(statIt);
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

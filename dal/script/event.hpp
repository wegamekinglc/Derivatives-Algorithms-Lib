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
storable ScriptProductData
   data for script product from an events table
version 1
&members
name is ?string
dates is date[]
events is string[]
-IF-------------------------------------------------------------------------*/


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
        ScriptProduct_(const std::map<Date_, String_>& events) { ParseEvents(events.begin(), events.end()); }
        ScriptProduct_(const Vector_<Date_>& dates, const Vector_<String_>& events) {
            REQUIRE(dates.size() == events.size(), "");
            for (int i = 0; i < dates.size(); ++i) {
                eventDates_.push_back(dates[i]);
                events_.push_back(Parse(events[i]));
            }
        }

        [[nodiscard]] const Vector_<Date_> &EventDates() const { return eventDates_; }
        [[nodiscard]] const Vector_<String_> &VarNames() const { return variables_; }
        [[nodiscard]] const Vector_<>& TimeLine() const { return timeLine_; }
        [[nodiscard]] const Vector_<Dal::AAD::SampleDef_>& DefLine() const { return defLine_; }

        template<class T_>
        std::unique_ptr<Evaluator_<T_>> BuildEvaluator() const {
            return std::unique_ptr<Evaluator_<T_>>(new Evaluator_<T_>(variables_.size()));
        }

        template<class T_>
        std::unique_ptr<FuzzyEvaluator_<T_>> BuildFuzzyEvaluator(int maxNestedIfs, double defEps) const {
            return std::unique_ptr<FuzzyEvaluator_<T_>>(new FuzzyEvaluator_<T_>(variables_.size(), maxNestedIfs, defEps));
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

        template<class T_, class E_>
        void Evaluate(const Scenario_<T_>& scenario, E_ &eval) const {
            eval.SetScenario(&scenario);
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

        size_t PreProcess(bool fuzzy, bool skip_domain);
        void Debug(std::ostream &ost) const;
    };

    class ScriptProductData_ : public Storable_ {
        Vector_<Date_> eventDates_;
        Vector_<String_> eventDesc_;
        mutable ScriptProduct_ product_;

    public:

        ScriptProductData_(const String_& name, const Vector_<Date_>& dates, const Vector_<String_>& events)
            : Storable_("ScriptProduct", name), eventDates_(dates), eventDesc_(events), product_(eventDates_, eventDesc_) {}
        void Write(Archive::Store_& dst) const override;

        ScriptProduct_& Product() const {
            return product_;
        }

    };
}

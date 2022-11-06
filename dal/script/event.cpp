//
// Created by wegam on 2022/11/5.
//

#include <dal/platform/strict.hpp>
#include <dal/script/event.hpp>
#include <dal/storage/globals.hpp>


namespace Dal::Script {
    void ScriptProduct_::Visit(Visitor_& v) {
        for (auto &evt: events_) {
            for (auto &stat: evt)
                v.VisitTree(stat);
        }
    }

    void ScriptProduct_::IndexVariables() {
        VarIndexer_ indexer;
        Visit(indexer);
        variables_ = indexer.GetVarNames();
    }

    size_t ScriptProduct_::IFProcess() {
        IFProcessor_ ifProc;
        Visit(ifProc);
        return ifProc.MaxNestedIFs();
    }

    //	Domain processing
    void ScriptProduct_::DomainProcess(bool fuzzy) {
        DomainProcessor_ domProc(variables_.size(), fuzzy);
        Visit(domProc);
    }

    void ScriptProduct_::ConstCondProcess() {
        ConstCondProcessor_ ccProc;
        for (auto &evt: events_) {
            for (auto &stat: evt)
                ccProc.ProcessFromTop(stat);
        }
    }

    //	All preprocessing
    size_t ScriptProduct_::PreProcess(bool fuzzy, bool skipDoms) {
        IndexVariables();

        size_t maxNestedIfs = 0;
        if (fuzzy || !skipDoms) {
            maxNestedIfs = IFProcess();
            DomainProcess(fuzzy);
            ConstCondProcess();
        }

        // generate time line and definition
        const auto evaluationDate = Global::Dates_().EvaluationDate();
        for(auto& date: eventDates_) {
            const double ttm = (date - evaluationDate) / 365.0;
            timeLine_.emplace_back(ttm);
            Dal::AAD::SampleDef_ sampleDef;
            sampleDef.numeraire_ = true;
            sampleDef.forwardMats_.push_back({ttm});
            sampleDef.discountMats_.push_back(ttm);
            defLine_.emplace_back(sampleDef);
        }
        return maxNestedIfs;
    }

    //	Debug whole product
    void ScriptProduct_::Debug(std::ostream &ost) {
        size_t v = 0;
        for (auto& variable: variables_)
            ost << "Var[" << v++ << "] = " << variable << std::endl;

        Debugger_ d;
        size_t e = 0;
        for (auto& evtIt: events_) {
            ost << "Event: " << ++e << std::endl;
            unsigned s = 0;
            for (const auto &stat: evtIt) {
                d.VisitTree(stat);
                ost << "Statement: " << ++s << std::endl;
                ost << d.String() << std::endl;
            }
        }
    }
}

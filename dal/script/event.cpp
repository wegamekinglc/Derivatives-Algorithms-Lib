//
// Created by wegam on 2022/11/5.
//

#include <dal/platform/strict.hpp>
#include <dal/script/event.hpp>


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

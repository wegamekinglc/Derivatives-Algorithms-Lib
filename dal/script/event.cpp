//
// Created by wegam on 2022/11/5.
//

#include <dal/platform/strict.hpp>
#include <dal/script/event.hpp>
#include <dal/script/visitor/debugger.hpp>
#include <dal/storage/globals.hpp>

namespace Dal::Script {

#include <dal/auto/MG_ScriptProductData_v1_Read.inc>
#include <dal/auto/MG_ScriptProductData_v1_Write.inc>

    void ScriptProduct_::IndexVariables() {
        VarIndexer_ indexer;
        Visit(indexer);
        variables_ = indexer.getVarNames();
    }

    size_t ScriptProduct_::IFProcess() {
        IFProcessor_ ifProc;
        Visit(ifProc);
        return ifProc.maxNestedIfs();
    }

    //	Domain processing
    void ScriptProduct_::DomainProcess(bool fuzzy) {
        DomainProcessor_ domProc(variables_.size(), fuzzy);
        Visit(domProc);
    }

    void ScriptProduct_::ConstProcess() {
        ConstProcessor_ domProc(variables_.size());
        Visit(domProc);
    }

    void ScriptProduct_::ConstCondProcess() {
        ConstCondProcessor_ ccProc;
        for (auto& evt : events_) {
            for (auto& stat : evt)
                ccProc.processFromTop(stat);
        }
    }

    //	All preprocessing
    size_t ScriptProduct_::PreProcess(bool fuzzy, bool skip_domain) {
        IndexVariables();
        size_t maxNestedIfs = 0;
        if (fuzzy || !skip_domain) {
            maxNestedIfs = IFProcess();
            DomainProcess(fuzzy);
            ConstCondProcess();
        }

        // generate time line and definition
        // TODO: more specific data settings
        const auto evaluationDate = Global::Dates_().EvaluationDate();
        for (auto& date : eventDates_) {
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
    void ScriptProduct_::Debug(std::ostream& ost) const {
        size_t v = 0;
        for (auto& variable : variables_)
            ost << "Var[" << v++ << "] = " << variable << std::endl;

        Debugger_ d;
        size_t e = 0;
        for (auto& evtIt : events_) {
            ost << "Event_: " << ++e << std::endl;
            unsigned s = 0;
            for (const auto& stat : evtIt) {
                stat->Accept(d);
                ost << "Statement_: " << ++s << std::endl;
                ost << d.String() << std::endl;
            }
        }
    }

    void ScriptProduct_::Compile() {
        //  First, identify constants
        ConstProcess();

        //  Clear
        myNodeStreams.clear();
        myConstStreams.clear();
        myDataStreams.clear();

        //  One per event date
        myNodeStreams.reserve(events_.size());
        myConstStreams.reserve(events_.size());
        myDataStreams.reserve(events_.size());

        //	Visit
        for (auto& evt : events_) {
            //	The compiler
            Compiler_ comp;

            //	Loop over statements in event
            for (auto& stat : evt) {
                //	Visit statement
                stat->Accept(comp);
            }

            //  Get compiled
            myNodeStreams.push_back(comp.nodeStream());
            myConstStreams.push_back(comp.constStream());
            myDataStreams.push_back(comp.dataStream());
        }
    }

    void ScriptProductData_::Write(Archive::Store_& dst) const {
        ScriptProductData_v1::XWrite(dst, name_, eventDates_, eventDesc_);
    }
} // namespace Dal::Script

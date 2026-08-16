//
// Created by wegam on 2022/11/5.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/script/event.hpp>
#include <dal/script/visitor/debugger.hpp>
#include <dal/storage/globals.hpp>
#include <dal/script/parser.hpp>
#include <dal/script/preprocessor.hpp>

namespace Dal::Script {
    void ScriptProduct_::ParseEvents(const Vector_<std::pair<Cell_, String_>> &events) {
        // 1. Definition front-end: resolve macros, const variables and schedules.
        Preprocessor_ preprocessor;
        auto preprocessed = preprocessor.Process(events);

        // 2. Payoff back-end: parse the resolved event descriptions into AST.
        Parser_ parser(preprocessed.constVariables_);
        const auto eval_data = Global::Dates_::EvaluationDate();
        for (const auto &processedEvent: preprocessed.events_) {
            if (processedEvent.first >= eval_data) {
                eventDates_.push_back(processedEvent.first);
                events_.push_back(parser.Parse(processedEvent.second));
            } else {
                pastEventDates_.push_back(processedEvent.first);
                pastEvents_.push_back(parser.Parse(processedEvent.second));
            }
        }
    }

    void ScriptProduct_::IndexVariables() {
        VarIndexer_ indexer;
        Visit(indexer);
        variables_ = indexer.VarNames();
        consVariables_ = indexer.ConstVarNames();
        consVariablesValues_ = indexer.ConstVarValues();

        for (auto i = 0; i < variables_.size(); ++i)
            if (variables_[i] == payoff_) {
                payoffIdx_ = i;
                break;
            }
        if (payoffIdx_ == -1)
            payoffIdx_ = variables_.size() - 1;
    }

    Vector_<> ScriptProduct_::PastEvaluate() const {
        PastEvaluator_<double> pastEvaluator(Vector_<double>(variables_.size(), 0.0), consVariablesValues_);
        Visit(pastEvaluator, true, false);
        return pastEvaluator.Variables();
    }

    size_t ScriptProduct_::IFProcess() {
        IFProcessor_ ifProc;
        Visit(ifProc);
        return ifProc.MaxNestedIFs();
    }

    void ScriptProduct_::DomainProcess(bool fuzzy) {
        DomainProcessor_ domProc(variables_.size(), fuzzy);
        Visit(domProc);
    }

    void ScriptProduct_::ConstProcess() {
        ConstProcessor_ domProc(variables_.size());
        Visit(domProc);
    }

    void ScriptProduct_::ConstCondProcess() {
        ConstCondProcessor_ ccProc{};
        for (auto& evt : events_) {
            for (auto& stat : evt)
                ccProc.ProcessFromTop(stat);
        }
    }

    size_t ScriptProduct_::PreProcess(bool fuzzy, bool skip_domain) {
        IndexVariables();
        variableValues_ = PastEvaluate();

        size_t maxNestedIfs = 0;
        if (fuzzy || !skip_domain) {
            maxNestedIfs = IFProcess();
            DomainProcess(fuzzy);
            ConstCondProcess();
        }

        // TODO: more specific data settings
        constexpr double DAYS_PER_YEAR = 365.0;
        const auto evaluationDate = Global::Dates_::EvaluationDate();
        for (auto& date : eventDates_) {
            const double ttm = (date - evaluationDate) / DAYS_PER_YEAR;
            timeLine_.emplace_back(ttm);
            Dal::AAD::SampleDef_ sampleDef;
            sampleDef.numeraire_ = true;
            sampleDef.forwardMats_.push_back({ttm});
            sampleDef.discountMats_.push_back(ttm);
            defLine_.emplace_back(sampleDef);
        }

        // Const metadata is finalized after condition folding.
        ConstProcess();
        preProcessed_ = true;

        return maxNestedIfs;
    }

    namespace {
        //  A fresh debugger per statement: the IR of previous statements would
        //  otherwise stay on the stack and grow the dump's footprint
        void DumpEventsJson(const Vector_<Date_>& dates,
                            const Vector_<Event_>& events,
                            const char* phase,
                            size_t& eventId,
                            size_t& nodeId,
                            bool& firstEvent,
                            std::ostream& ost) {
            for (size_t i = 0; i < events.size(); ++i) {
                if (!firstEvent)
                    ost << ',';
                firstEvent = false;
                ost << "{\"index\":" << eventId++ << ",\"date\":\"" << Date::ToString(dates[i])
                    << "\",\"phase\":\"" << phase << "\",\"statements\":[";
                for (size_t s = 0; s < events[i].size(); ++s) {
                    if (s)
                        ost << ',';
                    Debugger_ d;
                    events[i][s]->Accept(d);
                    DebugNodeJson(d.Top(), nodeId, ost);
                }
                ost << "]}";
            }
        }

        void DumpStatementTree(const Event_& statements, const TreeStyle_& st, int width, std::ostream& ost) {
            for (size_t s = 0; s < statements.size(); ++s) {
                Debugger_ d;
                statements[s]->Accept(d);
                Vector_<String_> lines;
                const String_ first =
                    String_(s + 1 == statements.size() ? st.elbow : st.tee) + "(" + String_(std::to_string(s + 1)) + ") ";
                const String_ cont = String_(s + 1 == statements.size() ? st.blank : st.pipe);
                DebugNodeTree(d.Top(), first, cont, st, width, lines);
                for (const auto& line : lines)
                    ost << line << '\n';
            }
        }

        void DumpEventsTree(const Vector_<Date_>& dates,
                            const Vector_<Event_>& events,
                            const char* phase,
                            size_t& eventId,
                            const TreeStyle_& st,
                            int width,
                            std::ostream& ost) {
            for (size_t i = 0; i < events.size(); ++i) {
                ost << st.eventS << ' ' << ++eventId << ' ' << st.dotS << ' ' << Date::ToString(dates[i]) << ' '
                    << st.dotS << ' ' << phase << '\n';
                DumpStatementTree(events[i], st, width, ost);
                ost << '\n';
            }
        }
    } // namespace

    void ScriptProduct_::Debug(std::ostream& ost) const {
        size_t v = 0;
        for (auto& variable : variables_)
            ost << "Var[" << v++ << "] = " << variable << std::endl;

        size_t e = 0;
        for (auto i = 0; i < events_.size(); ++i) {
            auto& evtIt = events_[i];
            ost << "EventTime_: " << Date::ToString(eventDates_[i]) << "\tEvent_: " << ++e << std::endl;
            unsigned s = 0;
            for (const auto& stat : evtIt) {
                Debugger_ d;
                stat->Accept(d);
                ost << "Statement_: " << ++s << std::endl;
                ost << d.String() << std::endl;
            }
        }
    }

    void ScriptProduct_::DebugJson(std::ostream& ost) const {
        ost << "{\"schema\":\"dal.script-product/1\"";
        if (!variables_.empty()) {
            ost << ",\"variables\":[";
            for (size_t i = 0; i < variables_.size(); ++i) {
                if (i)
                    ost << ',';
                ost << "{\"index\":" << i << ",\"name\":";
                JsonWriteString(variables_[i], ost);
                ost << '}';
            }
            ost << "],\"payoff_index\":" << payoffIdx_;
        }
        if (!consVariables_.empty()) {
            ost << ",\"constants\":[";
            for (size_t i = 0; i < consVariables_.size(); ++i) {
                if (i)
                    ost << ',';
                ost << "{\"index\":" << i << ",\"name\":";
                JsonWriteString(consVariables_[i], ost);
                ost << ",\"value\":" << DebugNumber(consVariablesValues_[i]) << '}';
            }
            ost << ']';
        }
        ost << ",\"events\":[";
        size_t eventId = 0;
        size_t nodeId = 0;
        bool firstEvent = true;
        DumpEventsJson(pastEventDates_, pastEvents_, "past", eventId, nodeId, firstEvent, ost);
        DumpEventsJson(eventDates_, events_, "future", eventId, nodeId, firstEvent, ost);
        ost << "]}";
    }

    void ScriptProduct_::DebugTree(std::ostream& ost, bool ascii, int width) const {
        const TreeStyle_& st = TreeStyle(ascii);
        if (!variables_.empty()) {
            ost << "Variables:";
            for (size_t i = 0; i < variables_.size(); ++i) {
                if (i)
                    ost << ',';
                ost << ' ' << variables_[i];
                if (i == payoffIdx_)
                    ost << '*';
            }
            ost << '\n';
            if (!consVariables_.empty()) {
                ost << "Constants:";
                for (size_t i = 0; i < consVariables_.size(); ++i) {
                    if (i)
                        ost << ',';
                    ost << ' ' << consVariables_[i] << '=' << DebugNumber(consVariablesValues_[i]);
                }
                ost << '\n';
            }
            ost << '\n';
        }
        size_t eventId = 0;
        DumpEventsTree(pastEventDates_, pastEvents_, "past", eventId, st, width, ost);
        DumpEventsTree(eventDates_, events_, "future", eventId, st, width, ost);
    }

    ScriptCompiled_ ScriptProduct_::Compile(bool fuzzy) const {
        REQUIRE2(preProcessed_, "product is not pre-processed: call PreProcess() before Compile()", ScriptError_);

        Vector_<Vector_<int>> nodeStreams;
        Vector_<Vector_<>> constStreams;

        nodeStreams.reserve(events_.size());
        constStreams.reserve(events_.size());

        for (const auto& evt : events_) {
            Compiler_ comp(fuzzy);

            for (const auto& stat : evt)
                stat->Accept(comp);

            nodeStreams.push_back(comp.NodeStream());
            constStreams.push_back(comp.ConstStream());
        }

        return {std::move(nodeStreams), std::move(constStreams)};
    }


#include <dal/auto/MG_ScriptProductData_v1_Read.inc>
#include <dal/auto/MG_ScriptProductData_v1_Write.inc>

    void ScriptProductData_::Write(Archive::Store_& dst) const {
        ScriptProductData_v1::XWrite(dst, name_, eventDates_, eventDesc_);
    }
} // namespace Dal::Script

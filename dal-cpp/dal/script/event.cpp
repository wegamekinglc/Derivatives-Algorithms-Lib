//
// Created by wegam on 2022/11/5.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/script/event.hpp>
#include <dal/script/parser.hpp>
#include <dal/script/preprocessor.hpp>
#include <dal/script/visitor/debugger.hpp>
#include <dal/storage/globals.hpp>

namespace Dal::Script {
    namespace {
        void RequireBoundPastSpots(const Node_& node) {
            REQUIRE2(!FindNode(node, [](const Node_& visited) { return dynamic_cast<const NodeSpot_*>(&visited) != nullptr; }),
                     "UnboundHistoricalSpot: SPOT() requires a default index", ScriptError_);
        }

        bool ValidateLsmcPayoffNode(const Node_& node, int payoffIdx, bool historical, bool paid) {
            const auto validateRange = [&](size_t first, size_t last, bool initialPaid) {
                for (size_t i = first; i < last; ++i)
                    initialPaid = ValidateLsmcPayoffNode(*node.arguments_[i], payoffIdx, historical, initialPaid);
                return initialPaid;
            };
            if (const auto* branch = dynamic_cast<const NodeIf_*>(&node)) {
                const size_t firstElse = branch->HasElse() ? branch->firstElse_ : node.arguments_.size();
                const bool thenPaid = validateRange(1, firstElse, paid);
                const bool elsePaid = validateRange(firstElse, node.arguments_.size(), paid);
                return thenPaid || elsePaid;
            }
            if (const auto* pays = dynamic_cast<const NodePays_*>(&node))
                return paid || (!historical && Downcast<NodeVar_>(pays->arguments_[0])->index_ == payoffIdx);
            if (const auto* assign = dynamic_cast<const NodeAssign_*>(&node)) {
                if (Downcast<NodeVar_>(assign->arguments_[0])->index_ == payoffIdx) {
                    const auto* zero = dynamic_cast<const NodeConst_*>(assign->arguments_[1].get());
                    REQUIRE2(!paid && zero && zero->constVal_ == 0.0,
                             "UnsupportedExercisePayoff: " + String_(historical ? "historical" : "future") +
                                 " payoff receiver permits only literal-zero initialization before PAYS",
                             ScriptError_);
                }
                return paid;
            }
            return validateRange(0, node.arguments_.size(), paid);
        }

        void ValidateLsmcPayoffAssignments(const Vector_<Event_>& events, int payoffIdx, bool historical) {
            bool paid = false;
            for (const auto& event : events)
                for (const auto& statement : event)
                    paid = ValidateLsmcPayoffNode(*statement, payoffIdx, historical, paid);
        }
    } // namespace

    void ScriptProduct_::ParseEvents(const Vector_<std::pair<Cell_, String_>>& events) {
        REQUIRE2(!evaluationDate_, "cannot append events after preparation has partitioned the product", ScriptError_);
        // 1. Definition front-end: resolve macros, const variables and schedules.
        Preprocessor_ preprocessor;
        auto preprocessed = preprocessor.Process(events);

        // 2. Payoff back-end: parse the resolved event descriptions into AST.
        Parser_ parser(preprocessed.constVariables_);
        for (const auto& processedEvent : preprocessed.events_) {
            REQUIRE2(processedEvent.first.IsValid(),
                     "InvalidFixingDate: dates/events; row=" + String_(std::to_string(preprocessed.sources_.at(processedEvent.first).front().row_)) +
                         "; expected a valid event date",
                     ScriptError_);
            auto event = parser.Parse(processedEvent.second, preprocessed.sources_.at(processedEvent.first));
            if (preparationError_.empty())
                preparationError_ = parser.PreparationError();
            hasPays_ = hasPays_ || parser.HasPays();
            hasExercise_ = hasExercise_ || parser.HasExercise();
            parsedEventDates_.push_back(processedEvent.first);
            parsedEventSources_.push_back(preprocessed.sources_.at(processedEvent.first));
            eventDates_.push_back(processedEvent.first);
            events_.push_back(std::move(event));
        }
    }

    void ScriptProduct_::PartitionEvents(const Date_& evaluationDate) {
        REQUIRE2(evaluationDate.IsValid(), "invalid script evaluation date", ScriptError_);
        REQUIRE2(!evaluationDate_, "script events are already partitioned", ScriptError_);
        Vector_<Date_> futureDates;
        Vector_<Event_> futureEvents;
        for (size_t i = 0; i < events_.size(); ++i) {
            const bool past = eventDates_[i] < evaluationDate;
            (past ? pastEventDates_ : futureDates).push_back(eventDates_[i]);
            (past ? pastEvents_ : futureEvents).push_back(std::move(events_[i]));
        }
        eventDates_ = std::move(futureDates);
        events_ = std::move(futureEvents);
        evaluationDate_ = evaluationDate;
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
        // The default receiver slot requires a PAYS statement. An EXERCISE-only product has no
        // receiver variable: the payoffIdx_ sentinel stays and the LSMC driver aggregates by itself.
        if (payoffIdx_ == -1 && !variables_.empty() && (HasPays() || !ContainsExercise()))
            payoffIdx_ = variables_.size() - 1;
    }

    Vector_<> ScriptProduct_::PastEvaluate() const {
        RequirePreparedFixings();
        PastEvaluator_<double> pastEvaluator(Vector_<double>(variables_.size(), 0.0), consVariablesValues_);
        Visit(pastEvaluator, true, false);
        return pastEvaluator.Variables();
    }

    void ScriptProduct_::InitializePastObservations(const ObservationPlan_& plan) {
        PastEvaluator_<double> evaluator(Vector_<>(variables_.size(), 0.0), consVariablesValues_);
        evaluator.SetObservations(&plan);
        Visit(evaluator, true, false);
        variableValues_ = evaluator.VarVals();
    }

    size_t ScriptProduct_::IFProcess() {
        IFProcessor_ ifProc;
        Visit(ifProc);
        return ifProc.MaxNestedIFs();
    }

    void ScriptProduct_::OptimizeLsmc() {
        if (HasPays()) {
            REQUIRE2(variableValues_[payoffIdx_] == 0.0, "UnsupportedExercisePayoff: EXERCISE requires a zero initial payoff receiver", ScriptError_);
            // A zero historical value can still carry live parameter risk on the AAD tape.
            ValidateLsmcPayoffAssignments(pastEvents_, payoffIdx_, true);
            ValidateLsmcPayoffAssignments(events_, payoffIdx_, false);
        }
        LsmcProcessor_ processor(variables_.size(), HasPays() ? static_cast<size_t>(payoffIdx_) : static_cast<size_t>(-1));
        processor.Process(&events_);
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
        RequirePreparedFixings();
        REQUIRE2(!preProcessed_, "script product is already pre-processed", ScriptError_);
        if (!evaluationDate_)
            PartitionEvents(Global::Dates_::EvaluationDate());
        REQUIRE2(!ContainsExercise(), "UnsupportedExecutionMode: the legacy PreProcess pipeline cannot value EXERCISE", ScriptError_);
        for (const auto& event : pastEvents_)
            for (const auto& statement : event)
                RequireBoundPastSpots(*statement);
        IndexVariables();
        REQUIRE2(!variables_.empty(), "InvalidScriptStructure: script has no payoff variable", ScriptError_);
        variableValues_ = PastEvaluate();

        size_t maxNestedIfs = 0;
        if (fuzzy || !skip_domain) {
            IFProcess(); //  Populates NodeIf_::affectedVars_ for DomainProcessor_; the count is only meaningful after folding
            DomainProcess(fuzzy);
            ConstCondProcess();
            maxNestedIfs = IFProcess();
        }

        // TODO: more specific data settings
        const auto evaluationDate = *evaluationDate_;
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

    void ScriptProduct_::ThrowPreparationError() const { THROW2(preparationError_, ScriptError_); }

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
                ost << "{\"index\":" << eventId++ << ",\"date\":\"" << Date::ToString(dates[i]) << "\",\"phase\":\"" << phase
                    << "\",\"statements\":[";
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
                const String_ first = String_(s + 1 == statements.size() ? st.elbow : st.tee) + "(" + String_(std::to_string(s + 1)) + ") ";
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
                ost << st.eventS << ' ' << ++eventId << ' ' << st.dotS << ' ' << Date::ToString(dates[i]) << ' ' << st.dotS << ' ' << phase << '\n';
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
        REQUIRE2(preparationError_.empty(),
                 "DebugSchemaUnsupported: dal.script-product/1 does not support FIX; use DescribeScriptProduct (dal.script-product/2)", ScriptError_);
        REQUIRE2(!ContainsExercise(),
                 "DebugSchemaUnsupported: dal.script-product/1 does not support EXERCISE; use DescribeScriptProduct (dal.script-product/2)",
                 ScriptError_);
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
        RequirePreparedFixings();
        REQUIRE2(preProcessed_, "product is not pre-processed: call PreProcess() before Compile()", ScriptError_);

        return ScriptCompiled_::Build(events_, fuzzy);
    }

#include <dal/auto/MG_ScriptProductData_v1_Read.inc>
#include <dal/auto/MG_ScriptProductData_v2_Read.inc>
#include <dal/auto/MG_ScriptProductData_v2_Write.inc>

    Storable_* ScriptProductData_v2::Reader_::Build() const {
        return new ScriptProductData_(name_, dates_, events_, ScriptProductSettings_{default_index_});
    }

    void ScriptProductData_::Write(Archive::Store_& dst) const {
        ScriptProductData_v2::XWrite(dst, name_, eventDates_, eventDesc_, settings_.defaultIndex_);
    }
} // namespace Dal::Script

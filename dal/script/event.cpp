//
// Created by wegam on 2022/11/5.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/script/event.hpp>
#include <dal/script/visitor/debugger.hpp>
#include <dal/storage/globals.hpp>

namespace Dal::Script {

    void ScriptProduct_::ParseEvents(const Vector_<std::pair<Cell_, String_>> &events) {
        std::map<String_, String_> macros;
        std::map<String_, double> constVariables;
        std::map<Date_, String_> processedEvents;
        std::map<Date_, String_> pastProcessedEvents;

        for (const auto & event : events) {
            Cell_ cell = event.first;
            if (!Cell::IsDate(cell)) {
                // distinguish macro and schedules
                auto desc = Cell::ToString(cell);
                if (desc.find(":") < desc.size()) {
                    // find a schedule
                    Vector_<String_> tokens = Tokenize(desc);

                    int i = 0;
                    Date_ startDate;
                    Date_ endDate;
                    Handle_<Date::Increment_> tenor;
                    Holidays_ holidays("");
                    DateGeneration_ genRule("Forward");
                    BizDayConvention_ bizRule("Unadjusted");

                    for (; i < tokens.size() - 2; ++i) {
                        REQUIRE2(tokens[i + 1] == ":", "schedule parameter name not followed by `:`", ScriptError_);

                        if (tokens[i] == "START") {
                            String_ dt_str = std::accumulate(tokens.begin() + i + 2, tokens.begin() + i + 7,
                                                             String_(""),
                                                             [](const String_ &x, const String_ &y) { return x + y; });
                            startDate = Date::FromString(dt_str);
                            i += 6;
                        } else if (tokens[i] == "END") {
                            String_ dt_str = std::accumulate(tokens.begin() + i + 2, tokens.begin() + i + 7,
                                                             String_(""),
                                                             [](const String_ &x, const String_ &y) { return x + y; });
                            endDate = Date::FromString(dt_str);
                            i += 6;
                        } else if (tokens[i] == "FREQ") {
                            tenor = Date::ParseIncrement(tokens[i + 2]);
                            i += 2;
                        } else if (tokens[i] == "CALENDAR") {
                            holidays = Holidays_(tokens[i + 2]);
                            i += 2;
                        } else if (tokens[i] == "BizRule") {
                            bizRule = BizDayConvention_(tokens[i + 2]);
                            i += 2;
                        } else
                            THROW2("unknown token", ScriptError_);
                    }

                    Vector_<Date_> schedule = Dal::MakeSchedule(startDate,
                                                                Cell_(endDate),
                                                                holidays,
                                                                tenor, genRule, bizRule);
                    String_ replaced = event.second;
                    for (const auto &macro: macros)
                        replaced = std::regex_replace(replaced,
                                                      std::regex(macro.first,std::regex_constants::icase),
                                                      macro.second);

                    for (auto k = 1; k < schedule.size(); ++k) {
                        auto d = schedule[k];
                        // Replace placeholder of `PeriodBegin` and `PeriodEnd`
                        auto final_statement = std::regex_replace(replaced,
                                                                  std::regex("PeriodBegin", std::regex_constants::icase),
                                                                  Date::ToString(schedule[k-1]));
                        final_statement = std::regex_replace(final_statement,
                                                             std::regex("PeriodEnd", std::regex_constants::icase),
                                                             Date::ToString(d));

                        if (processedEvents.find(d) != processedEvents.end())
                            processedEvents[d] += "\n" + final_statement;
                        else
                            processedEvents[d] = final_statement;
                    }
                } else {
                    REQUIRE2(macros.find(desc) == macros.end(), "macro name has already registered", ScriptError_);
                    REQUIRE2(constVariables.find(desc) == constVariables.end(), "const macro name has already registered", ScriptError_);
                    REQUIRE2(processedEvents.empty(), "macros should always at the front", ScriptError_);

                    if (String::IsNumber(event.second))
                        constVariables[desc] = String::ToDouble(event.second);
                    else
                        macros[desc] = event.second;
                }
            } else if (Cell::IsDate(cell)) {
                String_ replaced = event.second;
                for (const auto &macro: macros)
                    replaced = std::regex_replace(replaced, std::regex(macro.first, std::regex_constants::icase),
                                                  macro.second);
                auto d = Cell::ToDate(cell);
                if (processedEvents.find(d) != processedEvents.end())
                    processedEvents[d] += "\n" + replaced;
                else
                    processedEvents[d] = replaced;
            }
        }

        Parser_ parser(constVariables);
        const auto eval_data = Global::Dates_::EvaluationDate();
        for (const auto &processed_event: processedEvents) {
            if (processed_event.first >= eval_data) {
                eventDates_.push_back(processed_event.first);
                events_.push_back(parser.Parse(processed_event.second));
            } else {
                pastEventDates_.push_back(processed_event.first);
                pastEvents_.push_back(parser.Parse(processed_event.second));
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

    Vector_<> ScriptProduct_::PastEvaluate() {
        PastEvaluator_<double> pastEvaluator(Vector_<double>(variables_.size(), 0.0), consVariablesValues_);
        Visit(pastEvaluator, true, false);
        return pastEvaluator.Variables();
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

    //	All preprocessing
    size_t ScriptProduct_::PreProcess(bool fuzzy, bool skip_domain) {
        IndexVariables();
        variableValues_ = PastEvaluate();

        size_t maxNestedIfs = 0;
        if (fuzzy || !skip_domain) {
            maxNestedIfs = IFProcess();
            DomainProcess(fuzzy);
            ConstCondProcess();
        }

        // generate time line and definition
        // TODO: more specific data settings
        const auto evaluationDate = Global::Dates_::EvaluationDate();
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
        for (auto i = 0; i < events_.size(); ++i) {
            auto& evtIt = events_[i];
            ost << "EventTime_: " << Date::ToString(eventDates_[i]) << "\tEvent_: " << ++e << std::endl;
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
        nodeStreams_.clear();
        constStreams_.clear();
        dataStreams_.clear();

        //  One per event date
        nodeStreams_.reserve(events_.size());
        constStreams_.reserve(events_.size());
        dataStreams_.reserve(events_.size());

        //	Visit
        for (auto& evt : events_) {
            //	The compiler
            Compiler_ comp;

            //	Loop over statements in event
            for (auto& stat : evt)
                stat->Accept(comp);

            //  Get compiled
            nodeStreams_.push_back(comp.NodeStream());
            constStreams_.push_back(comp.ConstStream());
            dataStreams_.push_back(comp.DataStream());
        }
    }


#include <dal/auto/MG_ScriptProductData_v1_Read.inc>
#include <dal/auto/MG_ScriptProductData_v1_Write.inc>

    void ScriptProductData_::Write(Archive::Store_& dst) const {
        ScriptProductData_v1::XWrite(dst, name_, eventDates_, eventDesc_);
    }
} // namespace Dal::Script

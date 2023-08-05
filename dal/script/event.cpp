//
// Created by wegam on 2022/11/5.
//

#include <dal/platform/strict.hpp>
#include <dal/script/event.hpp>
#include <dal/math/matrix/matrixutils.hpp>
#include <dal/script/visitor/debugger.hpp>
#include <dal/storage/globals.hpp>

namespace Dal::Script {

#include <dal/auto/MG_ScriptProductData_v1_Read.inc>
#include <dal/auto/MG_ScriptProductData_v1_Write.inc>

    void ScriptProduct_::ParseEvents(const Vector_<std::pair<Cell_, String_>> &events) {
        Date_ evaluationDate = Global::Dates_().EvaluationDate();
        std::map<String_, String_> macros;
        std::map<String_, double> macro_variables;
        std::map<Date_, String_> processed_events;
        /*
         * we only keep the events after evaluation date
         * TODO: need to keep the historical events and visits them with a dedicated past evaluator
         * */
        for (const auto & event : events) {
            Cell_ cell = event.first;
            if (!Cell::IsDate(cell)) {
                // distinguish macro and schedules
                auto desc = Cell::ToString(cell);
                if (desc.find(":") < desc.size()) {
                    // find a schedule
                    Vector_<String_> tokens = Tokenize(desc);

                    int i = 0;
                    Date_ start_date;
                    Date_ end_date;
                    Handle_<Date::Increment_> tenor;
                    Holidays_ holidays("");
                    DateGeneration_ gen_rule("Forward");
                    BizDayConvention_ biz_rule("Unadjusted");

                    for (; i < tokens.size() - 2; ++i) {
                        REQUIRE2(tokens[i + 1] == ":", "schedule parameter name not followed by `:`", ScriptError_);

                        if (tokens[i] == "START") {
                            String_ dt_str = std::accumulate(tokens.begin() + i + 2, tokens.begin() + i + 7,
                                                             String_(""),
                                                             [](const String_ &x, const String_ &y) { return x + y; });
                            start_date = Date::FromString(dt_str);
                            i += 6;
                        } else if (tokens[i] == "END") {
                            String_ dt_str = std::accumulate(tokens.begin() + i + 2, tokens.begin() + i + 7,
                                                             String_(""),
                                                             [](const String_ &x, const String_ &y) { return x + y; });
                            end_date = Date::FromString(dt_str);
                            i += 6;
                        } else if (tokens[i] == "FREQ") {
                            tenor = Date::ParseIncrement(tokens[i + 2]);
                            i += 2;
                        } else if (tokens[i] == "CALENDAR") {
                            holidays = Holidays_(tokens[i + 2]);
                            i += 2;
                        } else if (tokens[i] == "BizRule") {
                            biz_rule = BizDayConvention_(tokens[i + 2]);
                            i += 2;
                        } else
                            THROW2("unknown token", ScriptError_);
                    }

                    Vector_<Date_> schedule = Dal::MakeSchedule(start_date,
                                                                Cell_(end_date),
                                                                holidays,
                                                                tenor,
                                                                gen_rule,
                                                                biz_rule);
                    String_ replaced = event.second;
                    for (const auto &macro: macros)
                        replaced = std::regex_replace(replaced,
                                                      std::regex(macro.first,std::regex_constants::icase),
                                                      macro.second);

                    for (auto k = 1; k < schedule.size(); ++k) {
                        auto d = schedule[k];
                        if (d >= evaluationDate) {
                            // Replace placeholder of `PeriodBegin` and `PeriodEnd`
                            auto final_statement = std::regex_replace(replaced,
                                                                      std::regex("PeriodBegin", std::regex_constants::icase),
                                                                      Date::ToString(schedule[k-1]));
                            final_statement = std::regex_replace(final_statement,
                                                                 std::regex("PeriodEnd", std::regex_constants::icase),
                                                                 Date::ToString(d));

                            if (processed_events.find(d) != processed_events.end())
                                processed_events[d] += "\n" + final_statement;
                            else
                                processed_events[d] = final_statement;
                        }
                    }
                } else {
                    REQUIRE2(macros.find(desc) == macros.end(), "macro name has already registered", ScriptError_);
                    REQUIRE2(macro_variables.find(desc) == macro_variables.end(), "macro variable name has already registered", ScriptError_);
                    REQUIRE2(processed_events.empty(), "macros should always at the front", ScriptError_);

                    if (String::IsNumber(event.second))
                        macro_variables[Cell::ToString(cell)] = String::ToDouble(event.second);
                    else
                        macros[Cell::ToString(cell)] = event.second;
                }
            } else if (Cell::IsDate(cell) && Cell::ToDate(cell) >= evaluationDate) {
                String_ replaced = event.second;
                for (const auto &macro: macros)
                    replaced = std::regex_replace(replaced, std::regex(macro.first, std::regex_constants::icase),
                                                  macro.second);
                auto d = Cell::ToDate(cell);
                if (processed_events.find(d) != processed_events.end())
                    processed_events[d] += "\n" + replaced;
                else
                    processed_events[d] = replaced;
            }
        }

        for (const auto &processed_event: processed_events) {
            eventDates_.push_back(processed_event.first);
            if (events_.empty()) {
                String_ e = processed_event.second;
                for (auto it = macro_variables.rbegin(); it != macro_variables.rend(); ++it)
                    e = it->first + " = " + ToString(it->second) + "\n" + e;
                events_.push_back(Parse(e));
            }
            else
                events_.push_back(Parse(processed_event.second));
        }
    }

    void ScriptProduct_::IndexVariables() {
        VarIndexer_ indexer;
        Visit(indexer);
        variables_ = indexer.VarNames();
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

    void ScriptProductData_::Write(Archive::Store_& dst) const {
        ScriptProductData_v1::XWrite(dst, name_, eventDates_, eventDesc_);
    }
} // namespace Dal::Script

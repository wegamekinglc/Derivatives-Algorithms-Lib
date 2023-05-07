//
// Created by wegam on 2022/4/4.
//

#pragma once

#include <regex>
#include <dal/math/aad/sample.hpp>
#include <dal/math/vectors.hpp>
#include <dal/script/node.hpp>
#include <dal/script/parser.hpp>
#include <dal/script/visitor/all.hpp>
#include <dal/storage/archive.hpp>
#include <dal/storage/globals.hpp>
#include <dal/time/date.hpp>
#include <dal/utilities/algorithms.hpp>
#include <dal/time/schedules.hpp>
#include <dal/time/dateutils.hpp>
#include <dal/time/dateincrement.hpp>
#include <dal/time/holidays.hpp>

/*IF--------------------------------------------------------------------------
storable ScriptProductData
   data for script product from an events table
version 1
&members
name is ?string
dates is cell[]
events is string[]
-IF-------------------------------------------------------------------------*/

namespace Dal::Script {
    using Dal::AAD::Scenario_;

    class ScriptProduct_ {
        Vector_<Date_> eventDates_;
        Vector_<Event_> events_;
        Vector_<String_> variables_;

        Vector_<double> timeLine_;
        Vector_<Dal::AAD::SampleDef_> defLine_;

        //  Compiled form
        Vector_<Vector_<int>> nodeStreams_;
        Vector_<Vector_<double>> constStreams_;
        Vector_<Vector_<const void*>> dataStreams_;

    public:
        explicit ScriptProduct_(const std::map<Cell_, String_>& events) { ParseEvents(events.begin(), events.end()); }
        ScriptProduct_(const Vector_<Cell_>& dates, const Vector_<String_>& events) {
            REQUIRE2(dates.size() == events.size(), "dates size is not equal to events size", ScriptError_);
            auto date_events = Dal::Zip(dates, events);
            ParseEvents(date_events.begin(), date_events.end());
        }

        [[nodiscard]] const Vector_<Date_>& EventDates() const { return eventDates_; }
        [[nodiscard]] const Vector_<String_>& VarNames() const { return variables_; }
        [[nodiscard]] const Vector_<>& TimeLine() const { return timeLine_; }
        [[nodiscard]] const Vector_<Dal::AAD::SampleDef_>& DefLine() const { return defLine_; }

        template <class T_> Evaluator_<T_> BuildEvaluator() const { return Evaluator_<T_>(variables_.size()); }

        template <class T_> FuzzyEvaluator_<T_> BuildFuzzyEvaluator(int maxNestedIfs, double defEps) const {
            return FuzzyEvaluator_<T_>(variables_.size(), maxNestedIfs, defEps);
        }

        template <class T_> std::unique_ptr<Scenario_<T_>> BuildScenario() const {
            return std::unique_ptr<Scenario_<T_>>(new Scenario_<T_>(eventDates_.size()));
        }

        template <class EvtIt_> void ParseEvents(EvtIt_ begin, EvtIt_ end) {
            Date_ evaluationDate = Global::Dates_().EvaluationDate();
            std::map<String_, String_> macros;
            std::map<Date_, String_> processed_events;
            /*
             * we only keep the events after evaluation date
             * TODO: need to keep the historical events and visits them with dedicated a past evaluator
             * */
            for (auto evtIt = begin; evtIt != end; ++evtIt) {
                Cell_ cell = evtIt->first;
                if (!Cell::IsDate(cell)) {
                    // distinguish macro and schedules
                    auto desc = Cell::ToString(cell);
                    if (desc.find(":") < desc.size() ) {
                        // find a schedule
                        Vector_<String_> tokens = Tokenize(desc);

                        int i = 0;
                        Date_ start_date;
                        Date_ end_date;
                        Handle_<Date::Increment_> tenor;
                        Holidays_ holidays("");
                        DateGeneration_ gen_rule("Forward");
                        BizDayConvention_ biz_rule("Unadjusted");

                        for(;i < tokens.size() - 2; ++i) {
                            REQUIRE2(tokens[i + 1] == ":", "schedule parameter name not followed by `:`", ScriptError_);

                            if (tokens[i] == "START") {
                                String_ dt_str = std::accumulate(tokens.begin() + i + 2, tokens.begin() + i + 7, String_(""), [](const String_& x, const String_& y) { return x + y; });
                                start_date = Date::FromString(dt_str);
                                i += 6;
                            } else if (tokens[i] == "END") {
                                String_ dt_str = std::accumulate(tokens.begin() + i + 2, tokens.begin() + i + 7, String_(""), [](const String_& x, const String_& y) { return x + y; });
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
                            }
                            else
                                THROW2("unknown token", ScriptError_);
                        }

                        Vector_<Date_> schedule = Dal::MakeSchedule(start_date,
                                                                    Cell_(end_date),
                                                                    holidays,
                                                                    tenor,
                                                                    gen_rule,
                                                                    biz_rule);
                        for (const auto& d : schedule) {
                            if (d >= evaluationDate) {
                                // TODO: to add logic replace place holder for specific date, e.g `PeriodBegin`, `PeriodEnd`
                                String_ replaced = evtIt->second;
                                for (const auto& macro : macros)
                                    replaced = std::regex_replace(
                                        replaced, std::regex(macro.first, std::regex_constants::icase), macro.second);
                                processed_events[d] = replaced;
                            }
                        }
                    } else {
                        REQUIRE2(macros.find(desc) == macros.end(), "macro name has already registered", ScriptError_);
                        REQUIRE2(processed_events.empty(), "macros should always at the front", ScriptError_);
                        macros[Cell::ToString(cell)] = evtIt->second;
                    }
                } else if(Cell::IsDate(cell) && Cell::ToDate(cell) >= evaluationDate) {
                    String_ replaced = evtIt->second;
                    for (const auto& macro : macros)
                        replaced = std::regex_replace(replaced, std::regex(macro.first, std::regex_constants::icase), macro.second);
                    processed_events[Cell::ToDate(cell)] = replaced;
                }
            }

            for (const auto& processed_event : processed_events) {
                eventDates_.push_back(processed_event.first);
                events_.push_back(Parse(processed_event.second));
            }
        }

        template <class V_> void Visit(Visitor_<V_>& v) {
            for (auto& evt : events_) {
                for (auto& stat : evt)
                    stat->Accept(static_cast<V_&>(v));
            }
        }

        template <class V_> void Visit(ConstVisitor_<V_>& v) const {
            for (const auto& evt : events_) {
                for (const auto& stat : evt)
                    stat->Accept(static_cast<V_&>(v));
            }
        }

        template <class T_, class E_> void Evaluate(const Scenario_<T_>& scenario, E_& eval) const {
            eval.SetScenario(&scenario);
            eval.Init();
            for (size_t i = 0; i < events_.size(); ++i) {
                eval.SetCurEvt(i);
                for (auto& statIt : events_[i])
                    statIt->Accept(eval);
            }
        }

        template <class T_> void EvaluateCompiled(const Scenario_<T_>& scenario, EvalState_<T_>& state) const {
            //	Initialize state
            state.Init();

            //	Loop over events
            for (size_t i = 0; i < events_.size(); ++i)
                //	Evaluate the compiled events
                EvalCompiled(nodeStreams_[i], constStreams_[i], dataStreams_[i], scenario[i], state);
        }

        void IndexVariables();
        size_t IFProcess();
        void DomainProcess(bool fuzzy);
        void ConstProcess();
        void ConstCondProcess();

        size_t PreProcess(bool fuzzy, bool skip_domain);
        void Debug(std::ostream& ost = std::cout) const;
        void Compile();
    };

    class ScriptProductData_ : public Storable_ {
        Vector_<Cell_> eventDates_;
        Vector_<String_> eventDesc_;
        mutable ScriptProduct_ product_;

    public:
        ScriptProductData_(const String_& name, const Vector_<Cell_>& dates, const Vector_<String_>& events)
            : Storable_("ScriptProduct", name), eventDates_(dates), eventDesc_(events),
              product_(eventDates_, eventDesc_) {}
        void Write(Archive::Store_& dst) const override;

        ScriptProduct_& Product() const { return product_; }
    };
} // namespace Dal::Script

//
// Created by wegam on 2026/5/30.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/script/preprocessor.hpp>
#include <dal/script/lexer.hpp>
#include <dal/script/event/schedule.hpp>
#include <dal/utilities/exceptions.hpp>
#include <regex>

namespace Dal::Script {
    bool Preprocessor_::IsSchedule(const String_& desc) const {
        return desc.find(":") != String_::npos;
    }

    bool Preprocessor_::IsConstVariable(const String_& value) const {
        return String::IsNumber(value);
    }

    String_ Preprocessor_::ExpandMacros(const String_& statement,
                                        const std::map<String_, String_>& macros) const {
        String_ replaced = statement;
        for (const auto& macro : macros)
            replaced = std::regex_replace(replaced,
                                          std::regex(macro.first, std::regex_constants::icase),
                                          macro.second);
        return replaced;
    }

    String_ Preprocessor_::ExpandSchedulePlaceholders(const String_& statement,
                                                      const Date_& begin,
                                                      const Date_& end) const {
        String_ replaced = std::regex_replace(statement,
                                              std::regex("PeriodBegin", std::regex_constants::icase),
                                              Date::ToString(begin));
        replaced = std::regex_replace(replaced,
                                      std::regex("PeriodEnd", std::regex_constants::icase),
                                      Date::ToString(end));
        return replaced;
    }

    PreprocessedEvents_ Preprocessor_::Process(const Vector_<std::pair<Cell_, String_>>& events) const {
        std::map<String_, String_> macros;
        PreprocessedEvents_ result;
        auto& constVariables = result.constVariables_;
        auto& processedEvents = result.events_;

        for (const auto& event : events) {
            const Cell_& cell = event.first;
            if (!Cell::IsDate(cell)) {
                // distinguish macro/const-variable definitions from schedules
                auto desc = Cell::ToString(cell);
                if (IsSchedule(desc)) {
                    // find a schedule
                    auto schedule = ParseSchedule(Tokenize(desc));
                    String_ replaced = ExpandMacros(event.second, macros);

                    for (const auto& s : schedule) {
                        auto startDate = std::get<0>(s);
                        auto endDate = std::get<1>(s);
                        auto fixDate = std::get<2>(s);
                        // Replace placeholders of `PeriodBegin` and `PeriodEnd`
                        auto final_statement = ExpandSchedulePlaceholders(replaced, startDate, endDate);

                        if (processedEvents.find(fixDate) != processedEvents.end())
                            processedEvents[fixDate] += "\n" + final_statement;
                        else
                            processedEvents[fixDate] = final_statement;
                    }
                } else {
                    REQUIRE2(macros.find(desc) == macros.end(), "macro name has already registered", ScriptError_);
                    REQUIRE2(constVariables.find(desc) == constVariables.end(),
                             "const macro name has already registered", ScriptError_);
                    REQUIRE2(processedEvents.empty(), "macros should always at the front", ScriptError_);

                    if (IsConstVariable(event.second))
                        constVariables[desc] = String::ToDouble(event.second);
                    else
                        macros[desc] = event.second;
                }
            } else {
                String_ replaced = ExpandMacros(event.second, macros);
                auto d = Cell::ToDate(cell);
                if (processedEvents.find(d) != processedEvents.end())
                    processedEvents[d] += "\n" + replaced;
                else
                    processedEvents[d] = replaced;
            }
        }
        return result;
    }
}

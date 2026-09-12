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
    namespace {
        template <class F_> String_ ExpandWithSource(F_ expand, size_t row, std::optional<Date_> date) {
            try {
                return expand();
            } catch (const ScriptError_& error) {
                String_ context("; row=" + std::to_string(row));
                if (date)
                    context += ", event=" + Date::ToString(*date);
                THROW2(String_(error.what()) + context, ScriptError_);
            }
        }

        void AppendEvent(PreprocessedEvents_* result, const Date_& date, const String_& statement, size_t row) {
            auto found = result->events_.find(date);
            const size_t offset = found == result->events_.end() ? 0 : found->second.size() + 1;
            result->sources_[date].push_back({offset, row, date});
            if (found == result->events_.end())
                result->events_[date] = statement;
            else
                found->second += "\n" + statement;
        }

        String_ ReplaceOutsideIndices(const String_& statement, const String_& pattern, const String_& replacement) {
            const std::regex expression(pattern, std::regex_constants::icase);
            String_ result;
            size_t start = 0;
            for (const auto& range : IndexLiteralRanges(statement)) {
                result += std::regex_replace(String_(statement.substr(start, range.first - start)), expression, replacement);
                result += statement.substr(range.first, range.second - range.first);
                start = range.second;
            }
            return result + std::regex_replace(String_(statement.substr(start)), expression, replacement);
        }
    } // namespace

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
            replaced = ReplaceOutsideIndices(replaced, macro.first, macro.second);
        return replaced;
    }

    String_ Preprocessor_::ExpandSchedulePlaceholders(const String_& statement,
                                                      const Date_& begin,
                                                      const Date_& end) const {
        String_ replaced = ReplaceOutsideIndices(statement, "PeriodBegin", Date::ToString(begin));
        replaced = ReplaceOutsideIndices(replaced, "PeriodEnd", Date::ToString(end));
        return replaced;
    }

    PreprocessedEvents_ Preprocessor_::Process(const Vector_<std::pair<Cell_, String_>>& events) const {
        std::map<String_, String_> macros;
        PreprocessedEvents_ result;
        auto& constVariables = result.constVariables_;
        auto& processedEvents = result.events_;

        size_t row = 0;
        for (const auto& event : events) {
            ++row;
            const Cell_& cell = event.first;
            if (!Cell::IsDate(cell)) {
                // distinguish macro/const-variable definitions from schedules
                auto desc = Cell::ToString(cell);
                if (IsSchedule(desc)) {
                    // find a schedule
                    auto schedule = ParseSchedule(Tokenize(desc));
                    String_ replaced = ExpandWithSource([&] { return ExpandMacros(event.second, macros); }, row, std::nullopt);

                    for (const auto& s : schedule) {
                        auto startDate = std::get<0>(s);
                        auto endDate = std::get<1>(s);
                        auto fixDate = std::get<2>(s);
                        // Replace placeholders of `PeriodBegin` and `PeriodEnd`
                        auto final_statement =
                            ExpandWithSource([&] { return ExpandSchedulePlaceholders(replaced, startDate, endDate); }, row, fixDate);

                        AppendEvent(&result, fixDate, final_statement, row);
                    }
                } else {
                    REQUIRE2(desc != "FIX", String_("ReservedIdentifier: FIX is a function; rename the definition; row=" + std::to_string(row)),
                             ScriptError_);
                    REQUIRE2(macros.find(desc) == macros.end(), "macro name has already registered", ScriptError_);
                    REQUIRE2(constVariables.find(desc) == constVariables.end(), "const macro name has already registered", ScriptError_);
                    REQUIRE2(processedEvents.empty(), "macros should always at the front", ScriptError_);

                    if (IsConstVariable(event.second))
                        constVariables[desc] = String::ToDouble(event.second);
                    else
                        macros[desc] = event.second;
                }
            } else {
                auto d = Cell::ToDate(cell);
                String_ replaced = ExpandWithSource([&] { return ExpandMacros(event.second, macros); }, row, d);
                AppendEvent(&result, d, replaced, row);
            }
        }
        return result;
    }
} // namespace Dal::Script

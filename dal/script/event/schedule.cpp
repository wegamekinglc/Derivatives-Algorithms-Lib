//
// Created by wegam on 2024/8/11.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/script/event/schedule.hpp>
#include <dal/time/dateutils.hpp>
#include <dal/time/schedules.hpp>
#include <dal/time/dateincrement.hpp>
#include <dal/time/holidays.hpp>
#include <dal/math/cell.hpp>


namespace Dal {
    Vector_<std::tuple<Date_, Date_, Date_>> ParseSchedule(const Vector_<String_>& tokens) {

            int i = 0;
            Date_ startDate;
            Date_ endDate;
            Handle_<Date::Increment_> tenor;
            Holidays_ holidays("");
            DateGeneration_ genRule("Forward");
            BizDayConvention_ bizRule("Unadjusted");
            bool fixAtEnd = true;

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
                } else if (tokens[i] == "FIXING") {
                    if (tokens[i + 2] == "BEGIN")
                        fixAtEnd = false;
                    else if (tokens[i + 2] == "END")
                        fixAtEnd = true;
                    else
                        THROW("unknown token for fixing");
                } else
                    THROW2("unknown token", ScriptError_);
            }

            Vector_<Date_> schedule = MakeSchedule(startDate,
                                                   Cell_(endDate),
                                                   holidays,
                                                   tenor,
                                                   genRule,
                                                   bizRule);

            Vector_<std::tuple<Date_, Date_, Date_>> rtn;
            for(auto k = 1; k < schedule.size(); ++k) {
                if (fixAtEnd)
                    rtn.emplace_back(std::make_tuple(schedule[k - 1], schedule[k], schedule[k]));
                else
                    rtn.emplace_back(std::make_tuple(schedule[k - 1], schedule[k], schedule[k - 1]));
            }
            return rtn;
        }
}
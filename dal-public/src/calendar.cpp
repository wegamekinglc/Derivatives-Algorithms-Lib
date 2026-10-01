//
// Created by Codex on 2026/10/1.
//

#include <dal-public/src/calendar.hpp>

namespace Dal {
    bool IsBusinessDay(const Holidays_& holidays, const Date_& date) { return Holidays::IsBusinessDay(holidays, date); }
    Date_ NextBusinessDay(const Holidays_& holidays, const Date_& date) { return Holidays::NextBus(holidays, date); }
    Date_ PreviousBusinessDay(const Holidays_& holidays, const Date_& date) { return Holidays::PrevBus(holidays, date); }
    Date_ AdjustBusinessDate(const Holidays_& holidays, const Date_& date, const BizDayConvention_& convention) {
        return Holidays::Adjust(holidays, date, convention);
    }
    int CountBusinessDays(const Holidays_& holidays, const Date_& begin, const Date_& end) { return CountBusDays_(holidays)(begin, end); }
} // namespace Dal

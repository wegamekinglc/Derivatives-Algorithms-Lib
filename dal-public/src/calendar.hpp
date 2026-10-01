//
// Created by Codex on 2026/10/1.
//

#pragma once

#include <dal-public/src/types.hpp>
#include <dal/time/holidays.hpp>

namespace Dal {
#include <dal/auto/MG_BizDayConvention_enum.hpp>

    bool IsBusinessDay(const Holidays_& holidays, const Date_& date);
    Date_ NextBusinessDay(const Holidays_& holidays, const Date_& date);
    Date_ PreviousBusinessDay(const Holidays_& holidays, const Date_& date);
    Date_ AdjustBusinessDate(const Holidays_& holidays, const Date_& date, const BizDayConvention_& convention);
    int CountBusinessDays(const Holidays_& holidays, const Date_& begin, const Date_& end);
} // namespace Dal

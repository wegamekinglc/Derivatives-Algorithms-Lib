//
// Created by wegam on 2026/6/20.
//

#pragma once

#include "__platform.hpp"
#include <dal/time/date.hpp>
#include <dal/time/holidays.hpp>
namespace Dal {
#include <dal/auto/MG_BizDayConvention_enum.hpp>
} // namespace Dal

/*IF--------------------------------------------------------------------------
public Is_BizDay
    Check whether a date is a business day for a given holiday center
&inputs
center is string
    Holiday center name (e.g. "BEY" for Beijing, empty string for no holidays)
date is date
    The date to check
&outputs
result is boolean
    true if the date is a business day
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public Next_BizDay
    Find the next business day on or after the given date
&inputs
center is string
    Holiday center name (e.g. "BEY" for Beijing, empty string for no holidays)
date is date
    The starting date
&outputs
result is date
    The next business day
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public Prev_BizDay
    Find the previous business day on or before the given date
&inputs
center is string
    Holiday center name (e.g. "BEY" for Beijing, empty string for no holidays)
date is date
    The starting date
&outputs
result is date
    The previous business day
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public Adjust_Date
    Adjust a date according to a business day convention
&inputs
center is string
    Holiday center name (e.g. "BEY" for Beijing, empty string for no holidays)
date is date
    The date to adjust
convention is string
    Business day convention: "Unadjusted", "Following", or "ModifiedFollowing"
&outputs
result is date
    The adjusted date
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public Count_BusDays
    Count business days between two dates
&inputs
center is string
    Holiday center name (e.g. "BEY" for Beijing, empty string for no holidays)
begin is date
    The start date (inclusive)
end is date
    The end date (inclusive)
&outputs
result is integer
    The number of business days in the range [begin, end]
-IF-------------------------------------------------------------------------*/

namespace Dal {
    namespace {
        void Is_BizDay(const String_& center, const Date_& date, bool* result) {
            Holidays_ hols(center);
            *result = Holidays::IsBusinessDay(hols, date);
        }

        void Next_BizDay(const String_& center, const Date_& date, Date_* result) {
            Holidays_ hols(center);
            *result = Holidays::NextBus(hols, date);
        }

        void Prev_BizDay(const String_& center, const Date_& date, Date_* result) {
            Holidays_ hols(center);
            *result = Holidays::PrevBus(hols, date);
        }

        void Adjust_Date(const String_& center, const Date_& date, const String_& convention, Date_* result) {
            Holidays_ hols(center);
            BizDayConvention_ conv(convention);
            *result = Holidays::Adjust(hols, date, conv);
        }

        void Count_BusDays(const String_& center, const Date_& begin, const Date_& end, int* result) {
            Holidays_ hols(center);
            CountBusDays_ counter(hols);
            *result = counter(begin, end);
        }
    }
#ifdef _WIN32
#include <dal-excel/auto/MG_Is_BizDay_public.inc>
#include <dal-excel/auto/MG_Next_BizDay_public.inc>
#include <dal-excel/auto/MG_Prev_BizDay_public.inc>
#include <dal-excel/auto/MG_Adjust_Date_public.inc>
#include <dal-excel/auto/MG_Count_BusDays_public.inc>
#endif
}

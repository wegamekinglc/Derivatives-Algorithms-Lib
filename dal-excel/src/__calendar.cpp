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
public NextBizDay
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
public PrevBizDay
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
public Adjust
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
public CountBusDays
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

        void NextBizDay(const String_& center, const Date_& date, Date_* result) {
            Holidays_ hols(center);
            *result = Holidays::NextBus(hols, date);
        }

        void PrevBizDay(const String_& center, const Date_& date, Date_* result) {
            Holidays_ hols(center);
            *result = Holidays::PrevBus(hols, date);
        }

        void Adjust(const String_& center, const Date_& date, const String_& convention, Date_* result) {
            Holidays_ hols(center);
            BizDayConvention_ conv(convention);
            *result = Holidays::Adjust(hols, date, conv);
        }

        void CountBusDays(const String_& center, const Date_& begin, const Date_& end, int* result) {
            Holidays_ hols(center);
            CountBusDays_ counter(hols);
            *result = counter(begin, end);
        }
    }
#ifdef _WIN32
#include <dal-excel/auto/MG_Is_BizDay_public.inc>
#include <dal-excel/auto/MG_NextBizDay_public.inc>
#include <dal-excel/auto/MG_PrevBizDay_public.inc>
#include <dal-excel/auto/MG_Adjust_public.inc>
#include <dal-excel/auto/MG_CountBusDays_public.inc>
#endif
}

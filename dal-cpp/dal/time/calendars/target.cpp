//
// Created by Claude on 2026/6/16.
//

#include <algorithm>

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/time/calendars/target.hpp>

namespace Dal::Target {
    namespace {
        // Meeus/Jones/Butcher Gregorian computus -> Easter Sunday (March or April).
        Date_ EasterSunday(int year) {
            const int a = year % 19;
            const int b = year / 100;
            const int c = year % 100;
            const int d = b / 4;
            const int e = b % 4;
            const int f = (b + 8) / 25;
            const int g = (b - f + 1) / 3;
            const int h = (19 * a + b - d - g + 15) % 30;
            const int i = c / 4;
            const int k = c % 4;
            const int l = (32 + 2 * e + 2 * i - h - k) % 7;
            const int m = (a + 11 * h + 22 * l) / 451;
            const int month = (h + l - 7 * m + 114) / 31;
            const int day = ((h + l - 7 * m + 114) % 31) + 1;
            return Date_(year, month, day);
        }
    } // namespace

    Vector_<Date_> Holidays(int startYear, int endYear) {
        Vector_<Date_> retval;
        for (int y = startYear; y <= endYear; ++y) {
            const Date_ easter = EasterSunday(y);
            const Date_ candidates[] = {Date_(y, 1, 1),     // New Year's Day
                                        easter.AddDays(-2), // Good Friday
                                        easter.AddDays(1),  // Easter Monday
                                        Date_(y, 5, 1),     // Labour Day
                                        Date_(y, 12, 25),   // Christmas Day
                                        Date_(y, 12, 26)};  // Second Day of Christmas
            for (const auto& d : candidates)
                if (!Date::IsWeekEnd(d))
                    retval.push_back(d);
        }
        std::sort(retval.begin(), retval.end());
        retval.erase(std::unique(retval.begin(), retval.end()), retval.end());
        return retval;
    }
} // namespace Dal::Target

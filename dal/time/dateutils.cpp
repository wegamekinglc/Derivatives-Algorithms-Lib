//
// Created by wegam on 2020/10/25.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/time/dateutils.hpp>
#include <regex>

#include <dal/time/date.hpp>
#include <dal/utilities/exceptions.hpp>

namespace {
    const std::regex US_FORMAT("([0-9]+)/([0-9]+)/([0-9]+)");
    const std::regex YMD_FORMAT("([0-9]+)-([0-9]+)-([0-9]+)");
} // namespace

namespace Dal {
    bool Date::IsDateString(const String_& src) {
        return std::regex_match(src, US_FORMAT) || std::regex_match(src, YMD_FORMAT);
    }

    Date_ Date::FromString(const String_& src) {
        NOTE("Reading date from string");
        NOTICE(src);
        std::match_results<String_::const_iterator> match;
        if (std::regex_match(src, match, US_FORMAT)) {
            const int mm = String::ToInt(String_(match.str(1)));
            const int dd = String::ToInt(String_(match.str(2)));
            const int yy = String::ToInt(String_(match.str(3))) + (match.str(3).size() == 2 ? 2000 : 0);
            return Date_(yy, mm, dd);
        }
        if (std::regex_match(src, match, YMD_FORMAT)) {
            const int yyyy = String::ToInt(String_(match.str(1)));
            const int mm = String::ToInt(String_(match.str(2)));
            const int dd = String::ToInt(String_(match.str(3)));
            return Date_(yyyy, mm, dd);
        }
        THROW("Unrecognizable date source");
    }

    int Date::MonthFromFutureCode(char code) {
        static const Vector_<short> MONTHS = {0, 0, 0, 0, 0, 1, 2, 3, 0,  4, 5,  0, 6,
                                              7, 0, 0, 8, 0, 0, 0, 9, 10, 0, 11, 0, 12};
        REQUIRE(code >= 'A' && code <= 'Z', "Futures code must be an uppercase letter");
        const int ret_val = MONTHS[code - 'A'];
        REQUIRE(ret_val > 0, "Invalid futures code");
        return ret_val;
    }

    Date_ Date::Today() {
        int yy, mm, dd;
        Host::LocalTime(&yy, &mm, &dd);
        Date_ ret_val(yy, mm, dd);
        return ret_val;
    }

} // namespace Dal

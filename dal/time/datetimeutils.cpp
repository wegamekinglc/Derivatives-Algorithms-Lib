//
// Created by wegam on 2020/10/25.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/time/datetimeutils.hpp>

#include <dal/time/datetime.hpp>
#include <dal/time/dateutils.hpp>
#include <dal/utilities/algorithms.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal {
    bool DateTime::IsDateTimeString(const String_& src) {
        // I fear this is going to be a performance bottleneck
        auto space = src.find(' ');
        if (space == String_::npos)
            return false;
        if (!Date::IsDateString(String_(src.substr(0, space))))
            return false;
        // accept anything with a date, space, something, colon, something
        auto colon = src.find(':', space);
        return colon != String_::npos && colon > space + 1 && colon + 1 < src.size();
    }

    DateTime_ DateTime::FromString(const String_& src) {
        NOTICE(src);
        auto space = src.find(' ');
        const Date_ date = Date::FromString(String_(src.substr(0, space)));
        if (space == String_::npos)
            return DateTime_(date, 0);
        // split remainder on ':'
        Vector_<String_> tParts = String::Split(String_(src.substr(space + 1)), ':', true);
        REQUIRE(tParts.size() >= 2 && tParts.size() <= 3, "Expected hh:mm or hh:mm:ss time");
        Vector_<int> hms = Apply(String::ToInt, tParts);
        REQUIRE(*MinElement(hms) >= 0 && hms[0] < 24 && *MaxElement(hms) < 60, "Hour/minute/second out of bounds");
        return DateTime_(date, hms[0], hms[1], hms.size() > 2 ? hms[2] : 0);
    }
} // namespace Dal
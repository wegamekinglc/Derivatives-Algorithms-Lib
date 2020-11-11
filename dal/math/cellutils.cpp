//
// Created by wegam on 2020/10/25.
//

#include <dal/math/cellutils.hpp>
#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

#include <dal/time/dateutils.hpp>
#include <dal/time/datetimeutils.hpp>


namespace Dal {
    bool Cell::CanConvert(const Cell_& c, const Cell::types_t& allowed) {
        static const int MIN_DATE = Date::ToExcel(Date::Minimum());
        static const int MAX_DATE = Date::ToExcel(Date::Maximum());

        auto ok = [&](const Cell_::Type_& t) { return allowed[static_cast<int>(t)]; };
        switch (c.type_) {
            // look at the type we have, see if there is an ok conversion available
            case Cell_::Type_::NUMBER:
                if (ok(Cell_::Type_::DATETIME) || (ok(Cell_::Type_::DATE) && c.d_ == static_cast<int>(c.d_)))
                    if (c.d_ >= MIN_DATE && c.d_ <= MAX_DATE)
                        return true;
                break;
            case Cell_::Type_::STRING:
                if (ok(Cell_::Type_::DATETIME) && DateTime::IsDateTimeString(c.s_))
                    return true;
                if (ok(Cell_::Type_::DATE) && Date::IsDateString(c.s_))
                    return true;
                break;
            default:
                return false;
        }
        return false;
    }

    String_ Cell::CoerceToString(const Cell_& src) {
        if (Cell::IsString(src))
            return Cell::OwnString(src);
        if (Cell::IsBool(src))
            return Cell::ToBool(src) ? String_("true") : String_("false");
        if (Cell::IsInt(src))
            return String::FromInt(Cell::ToInt(src));
        if (Cell::IsDouble(src))
            return String::FromDouble(Cell::ToDouble(src));
        if (Cell::IsDate(src))
            return Date::ToString(Cell::ToDate(src));
        if (Cell::IsDateTime(src))
            return DateTime::ToString(Cell::ToDateTime(src));
        REQUIRE(Cell::IsEmpty(src), "Unreachable -- bad cell type");
        return String_();
    }

    Cell_ Cell::FromOptionalDouble(const boost::optional<double>& src) { return src ? FromDouble(src.get()) : Cell_(); }

    Cell_ Cell::ConvertString(const String_& src) {
        if (src.empty())
            return Cell_();
        if (String::IsNumber(src))
            return String::ToDouble(src);
        if (Date::IsDateString(src))
            return Date::FromString(src);
        if (DateTime::IsDateTimeString(src))
            return DateTime::FromString(src);
        // does this logic merit an Enumeration?
        if (src == "TRUE")
            return Cell_(true);
        if (src == "FALSE")
            return Cell_(false);
        // sometimes a string is just a string
        return src;
    }
}


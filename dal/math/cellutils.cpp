//
// Created by wegam on 2020/10/25.
//

#include <dal/platform/strict.hpp>
#include <dal/math/cellutils.hpp>
#include <dal/string/strings.hpp>
#include <dal/time/datetimeutils.hpp>
#include <dal/time/dateutils.hpp>
#include <dal/utilities/algorithms.hpp>

namespace Dal {
    String_ Cell::CoerceToString(const Cell_& src) {
        if (Cell::IsString(src))
            return Cell::ToString(src);
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
        return {};
    }

    Cell_ Cell::FromOptionalDouble(const std::optional<double>& src) { return src ? FromDouble(src.value()) : Cell_(); }

    Cell_ Cell::ConvertString(const String_& src) {
        if (src.empty())
            return {};
        if (String::IsNumber(src))
            return Cell_(String::ToDouble(src));
        if (Date::IsDateString(src))
            return Cell_(Date::FromString(src));
        if (DateTime::IsDateTimeString(src))
            return Cell_(DateTime::FromString(src));
        // does this logic merit an Enumeration?
        if (src == "TRUE")
            return Cell_(true);
        if (src == "FALSE")
            return Cell_(false);
        // sometimes a string is just a string
        return Cell_(src);
    }

    Vector_<String_> Cell::ToStringLines(const Matrix_<Cell_>& src) {
        Vector_<String_> ret_val;
        for (int i = 0; i < src.Rows(); ++i) {
            const auto row = src.Row(i);
            Vector_<String_> values = Apply(CoerceToString, row);
            ret_val.push_back(String::Accumulate(values, ",", false));
        }
        return ret_val;
    }
} // namespace Dal

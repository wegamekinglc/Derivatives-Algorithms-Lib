//
// Created by wegamekinglc on 2020/5/2.
//

#include <dal/math/cell.hpp>
#include <dal/utilities/exceptions.hpp>
#include <dal/string/stringutils.hpp>

namespace Dal {
    String_ Cell::OwnString(const Cell_& src) {
        switch(src.type_) {
        case Cell::Type_::STRING:
            return src.s_;
        }
        THROW("Cell must contain s string value");
    }

    double Cell::ToDouble(const Cell_& src) {
        switch (src.type_) {
        case Cell::Type_::NUMBER:
            return src.d_;
        case Cell::Type_::BOOLEAN:
            return src.b_ ? 1. : 0.;
        }
        THROW("Cell must contain s numeric value");
    }

    bool Cell::IsInt(const Cell_& src) {
        return Cell::IsDouble(src) && AsInt(src.d_) == src.d_;
    }

    int Cell::ToInt(const Cell_& src) {
        const auto d = ToDouble(src);
        const auto ret_val = AsInt(d);
        REQUIRE(ret_val == d, "Cell must contain an integer value");
        return ret_val;
    }

    bool Cell::ToBool(const Cell_& src) {
        switch (src.type_) {
        case Cell::Type_::BOOLEAN:
            return src.b_;
        }
        THROW("Cell must contain a boolean value");
    }

    Date_ Cell::ToDate(const Cell_& src) {
        switch (src.type_) {
        case Cell::Type_::DATE:
            return src.dt_.Date();
        case Cell::Type_::NUMBER:
            return Date::FromExcel(ToInt(src));
        }
        THROW("Cell must contain a date value");
    }

    DateTime_ Cell::ToDateTime(const Cell_& src) {
        switch (src.type_) {
        case Cell::Type_::DATETIME:
            return src.dt_;
        case Cell::Type_::NUMBER:
            NOTE("Interpreting number as datetime");
            const auto dt = AsInt(src.d_);
            return DateTime_(Date::FromExcel(dt), src.d_ - dt);
        }
        THROW("Cell must contain a datetime value");
    }

    Vector_<bool> Cell::ToBoolVector(const Cell_& src) {
        switch (src.type_)
        {
        case Cell::Type_::BOOLEAN:
            return Vector_<bool>(1, src.b_);
        case Cell::Type_::STRING:
            return String::ToBoolVector(src.s_);
        default:
            THROW("Cell is not convertible to vector of booleans");
        }
    }

    Cell_ Cell::FromBoolVector(const Vector_<bool>& src) {
        String_ temp;
        for (const auto& b : src)
            temp.push_back(b ? 'T' : 'F');
        return Cell_(temp);
    }

    bool operator==(const Cell_& lhs, const String_& rhs) {
        return lhs.type_ == Cell::Type_::STRING
               && lhs.s_ == rhs;
    }

}
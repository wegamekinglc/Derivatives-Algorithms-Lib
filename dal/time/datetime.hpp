//
// Created by Cheng Li on 2018/2/4.
//

// compact datetime -- stores dates as in class Date_, plus times with ~1 second resolution

#pragma once

#include <dal/math/vectors.hpp>
#include <dal/time/date.hpp>

namespace dal {

    class DateTime_ {
        Date_ date_;
        double frac_;
    public:
        DateTime_():frac_(0) {}
        explicit DateTime_(const Date_& date, double frac=0.0);
        explicit DateTime_(long long msec);
        DateTime_(const Date_& date, int hour, int minute=0, int second=0);
        Date_ Date() const { return date_;}
        double Frac() const { return frac_;}
        bool operator == (const DateTime_& rhs) const { return date_ == rhs.date_ && frac_ == rhs.frac_;}
        bool IsValid() const { return date_.IsValid() && frac_ < 1.;}
    };

    double operator - (const DateTime_& lhs, const DateTime_& rhs);
    bool operator < (const DateTime_& lhs, const DateTime_& rhs);
    inline bool operator > (const DateTime_& lhs, const DateTime_& rhs) { return rhs < lhs;}
    inline bool operator <= (const DateTime_& lhs, const DateTime_& rhs) { return !(rhs < lhs);}
    inline bool operator >= (const DateTime_& lhs, const DateTime_& rhs) { return !(lhs < rhs);}
    inline bool operator != (const DateTime_& lhs, const DateTime_& rhs) { return !(lhs == rhs);}

    namespace datetime {
        int Hour(const DateTime_& dt);
        int Minute(const DateTime_& dt);
        String_ TimeString(const DateTime_& dt);
        inline String_ ToString(const DateTime_& dt) {
            return string::Accumulate<Vector_<String_>>({date::ToString(dt.Date()), TimeString(dt)}, String_(" "));
        }
        DateTime_ Minimum();
        long long MSec(const DateTime_& dt);
    }

    inline double NumericValueOf(const DateTime_& src) { return NumericValueOf(src.Date()) + src.Frac();}
}
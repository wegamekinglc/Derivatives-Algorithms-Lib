//
// Created by Cheng Li on 2018/2/2.
//

#pragma once

#include <dal/string/strings.hpp>

namespace Dal {
    class Date_;

    namespace Date {
        short Year(const Date_& dt);

        short Month(const Date_& dt);

        short Day(const Date_& dt);

        short DayOfWeek(const Date_& dt);

        short DaysInMonth(int year, int month);

        inline bool IsWeekEnd(const Date_& dt) { return DayOfWeek(dt) % 6 == 0; }

        Date_ FromExcel(int serial);

        int ToExcel(const Date_& dt);

        String_ ToString(const Date_& dt);

        Date_ Minimum();

        Date_ Maximum();

        Date_ EndOfMonth(const Date_& dt);

        Date_ AddMonths(const Date_& dt, int n_months, bool preserve_eom = false);
    } // namespace Date

    class Date_ {
        uint16_t serial_;

        friend Date_ Date::FromExcel(int);

        friend int Date::ToExcel(const Date_&);

        friend bool operator==(const Date_& lhs, const Date_& rhs);

        friend bool operator<(const Date_& lhs, const Date_& rhs);

    public:
        Date_() : serial_(0) {}

        Date_(int yyyy, int mm, int dd);

        Date_(const Date_& src) = default;

        bool IsValid() const { return serial_ > 0; }

        Date_& operator=(const Date_& rhs) = default;

        Date_ AddDays(int days) const {
            Date_ ret_val(*this);
            ret_val.serial_ += static_cast<uint16_t>(days);
            return ret_val;
        }

        Date_& operator++() {
            ++serial_;
            return *this;
        }

        Date_& operator--() {
            --serial_;
            return *this;
        }
    };

    inline bool operator==(const Date_& lhs, const Date_& rhs) { return lhs.serial_ == rhs.serial_; }

    inline bool operator!=(const Date_& lhs, const Date_& rhs) { return !(lhs == rhs); }

    inline bool operator<(const Date_& lhs, const Date_& rhs) { return lhs.serial_ < rhs.serial_; }

    inline bool operator>(const Date_& lhs, const Date_& rhs) { return rhs < lhs; }

    inline bool operator<=(const Date_& lhs, const Date_& rhs) { return !(rhs < lhs); }

    inline bool operator>=(const Date_& lhs, const Date_& rhs) { return !(lhs < rhs); }

    int operator-(const Date_& lhs, const Date_& rhs);

    inline double NumericValueOf(const Date_& src) { return static_cast<double>(Date::ToExcel(src)); }
} // namespace Dal

//
// Created by Cheng Li on 2018/2/2.
//

#pragma once

#include <cstdint>
#include <limits>
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

        Date_ AddMonths(const Date_& dt, int nMonths, bool preserveEom = false);
    } // namespace Date

    // Valid dates span serials [1, MAX_SERIAL], i.e. 1970-01-01 to 2149-06-05; serial 0 is the invalid default
    class Date_ {
        uint16_t serial_;

        static constexpr long long MAX_SERIAL = std::numeric_limits<uint16_t>::max();

        // Out of line so the inline arithmetic stays small on hot paths
        [[noreturn]] static void ThrowOutOfRange();

        friend Date_ Date::FromExcel(int);

        friend int Date::ToExcel(const Date_&);
        friend bool operator==(const Date_& lhs, const Date_& rhs);
        friend bool operator<(const Date_& lhs, const Date_& rhs);

    public:
        Date_() : serial_(0) {}

        Date_(int yyyy, int mm, int dd);

        Date_(const Date_& src) = default;

        [[nodiscard]] bool IsValid() const { return serial_ > 0; }

        Date_& operator=(const Date_& rhs) = default;

        [[nodiscard]] Date_ AddDays(int days) const {
            const long long serial = static_cast<long long>(serial_) + days;
            if (serial < 1 || serial > MAX_SERIAL)
                ThrowOutOfRange();
            Date_ ret_val;
            ret_val.serial_ = static_cast<uint16_t>(serial);
            return ret_val;
        }

        Date_& operator++() {
            if (serial_ == MAX_SERIAL)
                ThrowOutOfRange();
            ++serial_;
            return *this;
        }

        Date_& operator--() {
            if (serial_ <= 1)
                ThrowOutOfRange();
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

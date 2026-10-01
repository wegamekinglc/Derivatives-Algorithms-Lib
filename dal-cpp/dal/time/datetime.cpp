//
// Created by Cheng Li on 2018/2/4.
//

#include <dal/platform/strict.hpp>
#include <dal/time/datetime.hpp>
#include <dal/utilities/algorithms.hpp>

namespace Dal {

    namespace {
        void FracToHMS(double frac, int* h, int* m, int* s) {
            ASSIGN(h, static_cast<int>(24 * frac));
            ASSIGN(m, static_cast<int>(1440 * frac) % 60);
            ASSIGN(s, static_cast<int>(86400 * frac) % 60);
        }
    } // namespace

    DateTime_::DateTime_(const Date_& date, double frac) : date_(date), frac_(frac) {
        REQUIRE(frac_ >= 0.0 && frac_ < 1.0, "DateTime fraction must be in [0, 1)");
    }

    DateTime_::DateTime_(const Date_& date, int h, int m, int s) : date_(date) {
        REQUIRE(h >= 0 && h < 24 && m >= 0 && m < 60 && s >= 0 && s < 60, "DateTime requires hour in [0, 24), minute and second in [0, 60)");
        const auto secs = 60 * (60 * h + m) + s;
        frac_ = secs / 86400.;
    }

    DateTime_::DateTime_(long long msec) {
        REQUIRE(msec >= 0, "DateTime milliseconds must be nonnegative");
        const auto whole = msec / 86400000;
        REQUIRE(whole <= Date::Maximum() - Date::Minimum(), "DateTime milliseconds exceed the supported date range");
        frac_ = static_cast<double>(msec - 86400000 * whole) / 86400000.;
        date_ = Date::Minimum().AddDays(static_cast<int>(whole));
    }

    DateTime_& DateTime_::operator+=(double frac) {
        REQUIRE(IsValid(), "DateTime addition requires a valid date and fraction");
        REQUIRE(std::isfinite(frac) && frac > 0.0, "DateTime increment must be finite and positive");
        const double total = frac_ + frac;
        REQUIRE(total < static_cast<double>(Date::Maximum() - date_) + 1.0, "DateTime increment exceeds the supported date range");
        const auto days = static_cast<int>(std::floor(total));
        const auto date = date_.AddDays(days);
        date_ = date;
        frac_ = total - days;
        return *this;
    }

    DateTime_ DateTime_::operator+(double frac) {
        DateTime_ new_dt(date_, frac_);
        new_dt += frac;
        return new_dt;
    }

    double operator-(const DateTime_& lhs, const DateTime_& rhs) {
        return lhs.Date() - rhs.Date() + lhs.Frac() - rhs.Frac();
    }

    bool operator<(const DateTime_& lhs, const DateTime_& rhs) {
        return lhs.Date() < rhs.Date() || (lhs.Date() == rhs.Date() && lhs.Frac() < rhs.Frac());
    }

    namespace DateTime {
        int Hour(const DateTime_& dt) {
            int h;
            FracToHMS(dt.Frac(), &h, nullptr, nullptr);
            return h;
        }

        int Minute(const DateTime_& dt) {
            int m;
            FracToHMS(dt.Frac(), nullptr, &m, nullptr);
            return m;
        }

        String_ TimeString(const DateTime_& dt) {
            int h, m, s;
            FracToHMS(dt.Frac(), &h, &m, &s);
            String_ ret_val("00:00:00");
            sprintf(&ret_val[0], "%02d:%02d:%02d", h, m, s);
            return ret_val;
        }

        DateTime_ Minimum() { return DateTime_(Date::Minimum(), 0.); }

        long long MSec(const DateTime_& dt) {
            long long days = dt.Date() - Date::Minimum();
            return 86400000 * days + static_cast<long long>(dt.Frac() * 86400000);
        }
    } // namespace DateTime
} // namespace Dal

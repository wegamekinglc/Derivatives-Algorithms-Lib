//
// Created by Cheng Li on 2018/2/4.
//

#include <dal/platform/strict.hpp>
#include <dal/time/datetime.hpp>
#include <dal/utilities/algorithms.hpp>

namespace dal {

    namespace {
        void FracToHMS(double frac, int* h, int* m, int* s) {
            ASSIGN(h, static_cast<int>(24 * frac));
            ASSIGN(m, static_cast<int>(1440 * frac) % 60);
            ASSIGN(s, static_cast<int>(86400  * frac) % 60);
        }
    }

    DateTime_::DateTime_(const Date_ &date, double frac)
    :date_(date), frac_(static_cast<uint16_t>(Min(65535., 65536 * frac + 0.5))) {}

    DateTime_::DateTime_(const Date_ &date, int h, int m, int s)
    :date_(date) {
        const auto secs = 60 * (60 * h + m) + s;
        REQUIRE(secs >= 0 && secs < 86400, "datetime fraction exceeds maximum seconds in one day");
        const auto frac = (secs + 0.65) / 86400.;
        frac_ = static_cast<uint16_t>(frac * 65536);
    }

    DateTime_::DateTime_(long long msec) {
        const auto whole = msec / 86400000;
        const auto frac = (msec + 500 - 86400000 * whole) * 64 / 84375;	// 86400000/2^16 = 84375/64
        REQUIRE(frac <= 0xFFFF, "datetime fraction exceeds maximum seconds in one day");
        frac_ = static_cast<uint16_t>(frac);
        date_ = date::Minimum().AddDays(static_cast<int>(whole));
    }

    double operator - (const DateTime_& lhs, const DateTime_& rhs) {
        return lhs.Date() - rhs.Date() + lhs.Frac() - rhs.Frac();
    }

    bool operator < (const DateTime_& lhs, const DateTime_& rhs) {
        return lhs.Date() < rhs.Date() || (lhs.Date() == rhs.Date() && lhs.Frac() < rhs.Frac());
    }

    namespace datetime {
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
            sprintf(&ret_val[0], "%0.2d:%0.2d:%0.2d", h, m, s);
            return ret_val;
        }

        DateTime_ Minimum() {
            return DateTime_(date::Minimum(), 0.);
        }

        long long MSec(const DateTime_& dt) {
            long long days = dt.Date() - date::Minimum();
            return 86400000 * days + static_cast<long long>(dt.Frac() * 86400000);
        }
    }
}


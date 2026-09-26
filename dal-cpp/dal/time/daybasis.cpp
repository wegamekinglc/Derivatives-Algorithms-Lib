//
// Created by wegam on 2020/10/25.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/time/daybasis.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal {

#include <dal/auto/MG_DayBasis_enum.inc>

    namespace {
        double DaysInYear(int yy) { return Date::DaysInMonth(yy, 2) == 29 ? 366.0 : 365.0; }

        // Constructs the next 1 January only when the period crosses it, so periods ending near Date::Maximum() stay representable;
        // reversed periods are antisymmetric, matching QuantLib's ActualActual::ISDA
        double ActActISDA(const Date_& from, const Date_& to) {
            if (to < from)
                return -ActActISDA(to, from);
            const short yStart = Date::Year(from);
            const double denom = DaysInYear(yStart);
            if (Date::Year(to) <= yStart)
                return (to - from) / denom;
            const Date_ nextYear(yStart + 1, 1, 1);
            return (nextYear - from) / denom + ActActISDA(nextYear, to);
        }

        // 366 iff a 29 February falls in (from, to]
        double AnnualDaysL(const Date_& from, const Date_& to) {
            for (int yy = Date::Year(from), yLast = Date::Year(to); yy <= yLast; ++yy) {
                if (Date::DaysInMonth(yy, 2) == 29) {
                    const Date_ leapDay(yy, 2, 29);
                    if (from < leapDay && leapDay <= to)
                        return 366.0;
                }
            }
            return 365.0;
        }

        double DaysL(const Date_& end) { return DaysInYear(Date::Year(end)); }

        double Act365L(const Date_& from, const Date_& to, bool is_annual, const Date_& end) {
            return (to - from) / (is_annual ? AnnualDaysL(from, end) : DaysL(end));
        }

        double Bond(const Date_& from, const Date_& to) {
            short y1 = Date::Year(from);
            short m1 = Date::Month(from);
            short d1 = Date::Day(from);
            short y2 = Date::Year(to);
            short m2 = Date::Month(to);
            short d2 = Date::Day(to);

            // 30/360 US: February end-of-month rules first, then the 31st rules
            if (m1 == 2 && d1 == Date::DaysInMonth(y1, m1)) {
                if (m2 == 2 && d2 == Date::DaysInMonth(y2, m2))
                    d2 = 30;
                d1 = 30;
            }
            if (d2 == 31 && d1 >= 30)
                d2 = 30;
            if (d1 == 31)
                d1 = 30;
            return (360 * (y2 - y1) + 30 * (m2 - m1) + (d2 - d1)) / 360.0;
        }
    } // namespace

    double DayBasis_::operator()(const Date_& from, const Date_& to, const DayBasis::Context_* info) const {
        switch (val_) {
        case Value_::ACT_360:
            return (to - from) / 360.0;
        case Value_::ACT_365F:
            return (to - from) / 365.0;
        case Value_::ACT_ACT:
            return ActActISDA(from, to);
        case Value_::ACT_365L:
            REQUIRE(info, "ACT/365L day-count requires nominal end date");
            return Act365L(from, to, info->couponMonths_ == 12, info->nominalEnd_);
        case Value_::BOND:
            return Bond(from, to);
        default:
            THROW("Unrecognized day basis");
        }
    }
} // namespace Dal

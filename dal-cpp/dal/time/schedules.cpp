//
// Created by wegam on 2022/10/2.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/time/schedules.hpp>
#include <dal/time/date.hpp>
#include <dal/math/cell.hpp>
#include <dal/math/cellutils.hpp>
#include <dal/time/holidays.hpp>
#include <dal/time/dateincrement.hpp>


namespace Dal {
#include <dal/auto/MG_DateGeneration_enum.inc>
#include <dal/auto/MG_BizDayConvention_enum.inc>

    namespace {
        Schedule_ GenerateForward(const Date_& start, const Date_& maturity, int tenorMonths, bool preserveEom) {
            Vector_<Date_> retVal;
            retVal.push_back(start);
            for (;;) {
                const Date_ next = Date::AddMonths(retVal.back(), tenorMonths, preserveEom);
                if (next > maturity)
                    break;
                retVal.push_back(next);
            }
            if (retVal.back() != maturity)
                retVal.push_back(maturity);
            return retVal;
        }

        Schedule_ GenerateBackward(const Date_& start, const Date_& maturity, int tenorMonths, bool preserveEom) {
            Vector_<Date_> retVal;
            retVal.push_back(maturity);
            for (;;) {
                const Date_ prev = Date::AddMonths(retVal.back(), -tenorMonths, preserveEom);
                if (prev < start)
                    break;
                retVal.push_back(prev);
            }
            if (retVal.back() != start)
                retVal.push_back(start);
            return Reverse(retVal);
        }

        Schedule_ DateGenerateByPeriod(const Date_& start,
                                       const Date_& maturity,
                                       const PeriodLength_& tenor,
                                       DateGeneration_ method,
                                       bool preserveEom) {
            const int tenorMonths = tenor.Months();
            REQUIRE(tenorMonths > 0, "Schedule tenor must be positive");
            if (method == DateGeneration_("Forward"))
                return GenerateForward(start, maturity, tenorMonths, preserveEom);
            if (method == DateGeneration_("Backward"))
                return GenerateBackward(start, maturity, tenorMonths, preserveEom);
            THROW("date generation rule is not recognized");
        }

        Date_ ApplyLag(const Date_& date, int lag, const Holidays_& hols, bool forward) {
            if (lag == 0)
                return date;
            const int steps = std::abs(lag);
            const bool moveForward = lag > 0 ? forward : !forward;
            return moveForward ? Date::NBusDays(steps, hols)->FwdFrom(date) : Date::NBusDays(steps, hols)->BackFrom(date);
        }
    } // namespace

    int CouponMonths(const Date_& start, const Date_& maturity) {
        int months = 12 * (Date::Year(maturity) - Date::Year(start)) + Date::Month(maturity) - Date::Month(start);
        const bool startIsEom = start == Date::EndOfMonth(start);
        const bool maturityIsEom = maturity == Date::EndOfMonth(maturity);
        if (Date::Day(maturity) < Date::Day(start) && !(startIsEom && maturityIsEom))
            --months;
        return std::max(months, 1);
    }

    Schedule_ DateGenerate(const Date_& start,
                           const Date_& maturity,
                           const Handle_<Date::Increment_>& tenor, DateGeneration_ method) {
        Vector_<Date_> ret_val;
        if (method == DateGeneration_("Forward")) {
            ret_val.push_back(start);
            for (int ip = 1;; ++ip) {
                const Date_ pin_date = tenor->FwdFrom(ret_val[ip - 1]);
                if (pin_date > maturity)
                    break;
                ret_val.push_back(pin_date);
            }
            if (maturity != ret_val.back())
                ret_val.push_back(maturity);
        } else if (method == DateGeneration_("Backward")) {
            ret_val.push_back(maturity);
            for (int ip = 1;; ++ip) {
                const Date_ pin_date = tenor->BackFrom(ret_val[ip - 1]);
                if (pin_date < start)
                    break;
                ret_val.push_back(pin_date);
            }
            if (start != ret_val.back())
                ret_val.push_back(start);
            ret_val = Reverse(ret_val);
        } else
            THROW("date generation rule is not recognized");
        return ret_val;
    }

    Schedule_ MakeSchedule(const Date_& start,
                           const Cell_& maturity,
                           const Holidays_& hols,
                           const Handle_<Date::Increment_>& tenor,
                           DateGeneration_ method,
                           BizDayConvention_ convention) {
        REQUIRE(Cell::TypeCheck_<Date_>()(maturity), "currently `end` must be a date");
        Date_ end = Cell::ToDate(maturity);
        Vector_<Date_> ret_val;
        Vector_<Date_> pin_dates = DateGenerate(start, end, tenor, method);
        for (const auto& pin_date : pin_dates) {
            ret_val.push_back(Holidays::Adjust(hols, pin_date, convention));
        }
        return Unique(ret_val);
    }

    Schedule_ MakeSchedule(const Date_& start,
                           const Date_& maturity,
                           const PeriodLength_& tenor,
                           const Holidays_& hols,
                           DateGeneration_ method,
                           BizDayConvention_ convention,
                           bool preserveEom) {
        Vector_<Date_> retVal;
        for (const auto& pinDate : DateGenerateByPeriod(start, maturity, tenor, method, preserveEom))
            retVal.push_back(Holidays::Adjust(hols, pinDate, convention));
        return Unique(retVal);
    }

    Vector_<SchedulePeriod_> MakeSchedulePeriods(const Date_& start,
                                                 const Date_& maturity,
                                                 const PeriodLength_& tenor,
                                                 const Holidays_& accrualHolidays,
                                                 int fixingLag,
                                                 const Holidays_& fixingHolidays,
                                                 int paymentLag,
                                                 const Holidays_& paymentHolidays,
                                                 DateGeneration_ method,
                                                 BizDayConvention_ accrualConvention,
                                                 BizDayConvention_ paymentConvention,
                                                 bool preserveEom) {
        const Vector_<Date_> unadjusted = DateGenerateByPeriod(start, maturity, tenor, method, preserveEom);
        REQUIRE(unadjusted.size() >= 2, "Schedule periods require at least one accrual interval");

        Vector_<SchedulePeriod_> retVal;
        retVal.reserve(unadjusted.size() - 1);
        for (int i = 1; i < static_cast<int>(unadjusted.size()); ++i) {
            SchedulePeriod_ period;
            period.unadjustedStart_ = unadjusted[i - 1];
            period.unadjustedEnd_ = unadjusted[i];
            period.accrualStart_ = Holidays::Adjust(accrualHolidays, period.unadjustedStart_, accrualConvention);
            period.accrualEnd_ = Holidays::Adjust(accrualHolidays, period.unadjustedEnd_, accrualConvention);
            period.fixingDate_ = ApplyLag(period.accrualStart_, fixingLag, fixingHolidays, false);
            period.paymentDate_ = Holidays::Adjust(paymentHolidays,
                                                   ApplyLag(period.accrualEnd_, paymentLag, paymentHolidays, true),
                                                   paymentConvention);
            const int couponMonths = CouponMonths(period.unadjustedStart_, period.unadjustedEnd_);
            period.isStub_ = couponMonths != tenor.Months();
            period.dayCountContext_.reset(
                new DayBasis::Context_(i == static_cast<int>(unadjusted.size()) - 1, period.unadjustedStart_, period.unadjustedEnd_, couponMonths));
            retVal.push_back(period);
        }
        return retVal;
    }
}

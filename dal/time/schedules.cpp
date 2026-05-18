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
        Schedule_ DateGenerateByPeriod(const Date_& start,
                                       const Date_& maturity,
                                       const PeriodLength_& tenor,
                                       DateGeneration_ method,
                                       bool preserveEom) {
            const int tenorMonths = tenor.Months();
            REQUIRE(tenorMonths > 0, "Schedule tenor must be positive");
            Vector_<Date_> retVal;
            if (method == DateGeneration_("Forward")) {
                retVal.push_back(start);
                for (;;) {
                    const Date_ next = Date::AddMonths(retVal.back(), tenorMonths, preserveEom);
                    if (next > maturity)
                        break;
                    retVal.push_back(next);
                }
                if (retVal.back() != maturity)
                    retVal.push_back(maturity);
            } else if (method == DateGeneration_("Backward")) {
                retVal.push_back(maturity);
                for (;;) {
                    const Date_ prev = Date::AddMonths(retVal.back(), -tenorMonths, preserveEom);
                    if (prev < start)
                        break;
                    retVal.push_back(prev);
                }
                if (retVal.back() != start)
                    retVal.push_back(start);
                retVal = Reverse(retVal);
            } else {
                THROW("date generation rule is not recognized");
            }
            return retVal;
        }

        Date_ ApplyLag(const Date_& date, int lag, const Holidays_& hols, bool forward) {
            if (lag == 0)
                return date;
            const int steps = std::abs(lag);
            const bool moveForward = lag > 0 ? forward : !forward;
            return moveForward ? Date::NBusDays(steps, hols)->FwdFrom(date) : Date::NBusDays(steps, hols)->BackFrom(date);
        }
    } // namespace

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
                                                 const DayBasis_&,
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
            period.isStub_ = Date::AddMonths(period.unadjustedStart_, tenor.Months(), preserveEom) != period.unadjustedEnd_;
            period.dayCountContext_.reset(
                new DayBasis::Context_(i == static_cast<int>(unadjusted.size()) - 1, period.unadjustedStart_, period.unadjustedEnd_, tenor.Months()));
            retVal.push_back(period);
        }
        return retVal;
    }
}

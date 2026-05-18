//
// Created by wegam on 2022/10/2.
//

#pragma once
#include <dal/math/vectors.hpp>
#include <dal/string/strings.hpp>
#include <dal/time/daybasis.hpp>
#include <dal/time/holidays.hpp>
#include <dal/time/periodlength.hpp>
#include <dal/utilities/exceptions.hpp>

/*IF---------------------------------------------------------
enumeration DateGeneration
    These conventions specify the rule used to generate dates in a schedule
switchable
alternative Backward
    Generate dates backward
alternative Forward
    Generate dates forward
-IF---------------------------------------------------------*/


/*IF---------------------------------------------------------
enumeration BizDayConvention
    These conventions specify the algorithm used to adjust a date in case it is not a valid business day
switchable
alternative Unadjusted
    Do not modify the date according to the holidays
alternative Following
    Choose the first business day after the given holiday
alternative ModifiedFollowing
    Choose the first business day after the given holiday unless it belongs
    to a different month, in which case choose the first business day before
    the holiday
-IF---------------------------------------------------------*/


namespace Dal {
#include <dal/auto/MG_DateGeneration_enum.hpp>
#include <dal/auto/MG_BizDayConvention_enum.hpp>

    namespace Date {
        class Increment_;
    }

    struct Cell_;
    class Date_;
    using Schedule_ = Vector_<Date_>;
    struct SchedulePeriod_ {
        Date_ unadjustedStart_;
        Date_ unadjustedEnd_;
        Date_ accrualStart_;
        Date_ accrualEnd_;
        Date_ fixingDate_;
        Date_ paymentDate_;
        bool isStub_ = false;
        Handle_<DayBasis::Context_> dayCountContext_;
    };

    Schedule_ DateGenerate(const Date_& start,
                           const Date_& maturity,
                           const Handle_<Date::Increment_>& tenor,
                           DateGeneration_ method = DateGeneration_("Forward"));

    Schedule_ MakeSchedule(const Date_& start,
                           const Cell_& maturity,
                           const Holidays_& hols,
                           const Handle_<Date::Increment_>& tenor,
                           DateGeneration_ method = DateGeneration_("Forward"),
                           BizDayConvention_ convention = BizDayConvention_("Following"));
    Schedule_ MakeSchedule(const Date_& start,
                           const Date_& maturity,
                           const PeriodLength_& tenor,
                           const Holidays_& hols,
                           DateGeneration_ method = DateGeneration_("Forward"),
                           BizDayConvention_ convention = BizDayConvention_("Following"),
                           bool preserveEom = false);
    Vector_<SchedulePeriod_> MakeSchedulePeriods(const Date_& start,
                                                 const Date_& maturity,
                                                 const PeriodLength_& tenor,
                                                 const Holidays_& accrualHolidays,
                                                 int fixingLag = 0,
                                                 const Holidays_& fixingHolidays = Holidays_(""),
                                                 int paymentLag = 0,
                                                 const Holidays_& paymentHolidays = Holidays_(""),
                                                 DateGeneration_ method = DateGeneration_("Forward"),
                                                 BizDayConvention_ accrualConvention = BizDayConvention_("Following"),
                                                 BizDayConvention_ paymentConvention = BizDayConvention_("Following"),
                                                 bool preserveEom = false);
}

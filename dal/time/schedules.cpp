//
// Created by wegam on 2022/10/2.
//

#include <dal/platform/platform.hpp>
#include <dal/time/schedules.hpp>
#include <dal/time/date.hpp>
#include <dal/math/cell.hpp>
#include <dal/math/cellutils.hpp>
#include <dal/time/holidays.hpp>
#include <dal/time/dateincrement.hpp>


namespace Dal {
    Schedule_ MakeSchedule(const Date_& start,
                           const Cell_& maturity,
                           const Holidays_& hols,
                           const Handle_<Date::Increment_>& tenor) {
        // we only support forward generated schedule
        REQUIRE(Cell::TypeCheck_<Date_>()(maturity), "currently `end` must be a date");
        Date_ end = Cell::ToDate(maturity);
        Vector_<Date_> ret_val = Vector::V1(Holidays::NextBus(hols, start));
        Vector_<Date_> pin_dates = Vector::V1(start);
        for (int ip = 1;; ++ip) {
            const Date_ pin_date = tenor->FwdFrom(pin_dates[ip-1]);
            if (pin_date > end) break;
            const Date_ adjust_date = Holidays::NextBus(hols, pin_date);
            pin_dates.push_back(pin_date);
            if (adjust_date != ret_val[ip-1])
                ret_val.push_back(adjust_date);
        }

        if (end > ret_val.back())
            ret_val.push_back(end);
        return ret_val;
    }
}

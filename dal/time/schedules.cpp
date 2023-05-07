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
#include <dal/auto/MG_DateGeneration_enum.inc>
#include <dal/auto/MG_BizDayConvention_enum.inc>

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
            if (convention == BizDayConvention_("Following"))
                ret_val.push_back(Holidays::NextBus(hols, pin_date));
            else if (convention == BizDayConvention_("Unadjusted"))
                ret_val.push_back(pin_date);
            else
                THROW("business date rule is not recognized");
        }
        return Unique(ret_val);
    }
}

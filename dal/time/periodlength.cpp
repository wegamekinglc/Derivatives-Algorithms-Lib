//
// Created by wegam on 2022/1/29.
//

#include <dal/platform/platform.hpp>
#include <dal/time/date.hpp>
#include <dal/time/periodlength.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal {
#include <dal/auto/MG_PeriodLength_enum.inc>

    int PeriodLength_::Months() const {
        switch (val_) {
        case Value_::ANNUAL:
            return 12;
        case Value_::SEMIANNUAL:
            return 6;
        case Value_::QUARTERLY:
            return 3;
        case Value_::MONTHLY:
            return 1;
        default:
            THROW("unnatural period length");
        }
    }

    Date_ Date::NominalMaturity(const Date_& start, const PeriodLength_& step, const Ccy_& ccy) {
        int yy = Year(start);
        int mm = Month(start) + step.Months();
        while (mm > 12)
            ++yy, mm -= 12;
        int dd = Min(Day(start), DaysInMonth(yy, mm));
        return Date_(yy, mm, dd);
    }
} // namespace Dal
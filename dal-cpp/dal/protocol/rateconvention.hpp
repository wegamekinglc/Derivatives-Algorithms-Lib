//
// Created by GitHub Copilot on 2026/5/18.
//

#pragma once

#include <dal/protocol/collateraltype.hpp>
#include <dal/time/daybasis.hpp>
#include <dal/time/holidays.hpp>
#include <dal/time/periodlength.hpp>
#include <dal/time/schedules.hpp>

namespace Dal {
    struct RateIndexConvention_ {
        int spotLag_ = 0;
        int fixingLag_ = 0;
        bool useProjectionCurve_ = false;
        PeriodLength_ forecastTenor_;
        DayBasis_ dayBasis_ = DayBasis_("ACT_365F");
        BizDayConvention_ businessDayConvention_ = BizDayConvention_("Following");
        Holidays_ fixingHolidays_ = Holidays_("");
        Holidays_ accrualHolidays_ = Holidays_("");
        bool endOfMonth_ = false;
        CollateralType_ collateral_ = CollateralType_(CollateralType_::Value_::OIS);
    };

    struct RateLegConvention_ {
        int paymentLag_ = 0;
        PeriodLength_ paymentFrequency_;
        DayBasis_ dayBasis_ = DayBasis_("ACT_365F");
        BizDayConvention_ businessDayConvention_ = BizDayConvention_("Following");
        BizDayConvention_ paymentConvention_ = BizDayConvention_("Following");
        Holidays_ accrualHolidays_ = Holidays_("");
        Holidays_ paymentHolidays_ = Holidays_("");
        bool endOfMonth_ = false;
    };
} // namespace Dal

//
// Created by wegam on 2022/12/9.
//

#pragma once

#include <dal/time/date.hpp>
#include <dal/time/daybasis.hpp>

namespace Dal {
    struct AccrualPeriod_ {
        Date_ startDate_;
        Date_ endDate_;
        double notional_;
        DayBasis_ couponBasis_;
        double dcf_;
        Handle_<DayBasis::Context_> context_;
        bool isStub_;

        AccrualPeriod_() = default;
        AccrualPeriod_(const Date_& start, const Date_& end, double notional, const DayBasis_& day_count);
        AccrualPeriod_(const Date_& start, const Date_& end, double notional, const DayBasis_& day_count, const Handle_<DayBasis::Context_>& context, bool is_stub);
    };
}

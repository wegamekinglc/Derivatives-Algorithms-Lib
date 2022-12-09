//
// Created by wegam on 2022/12/9.
//

#include <dal/protocol/accrualperiod.hpp>
#include <dal/platform/strict.hpp>

namespace Dal {
    AccrualPeriod_::AccrualPeriod_(const Date_ &start, const Date_ &end, double notional, const DayBasis_ &day_count)
    : startDate_(start), endDate_(end), notional_(notional), couponBasis_(day_count), dcf_(day_count(start, end, nullptr)), isStub_(false) {}

    AccrualPeriod_::AccrualPeriod_(const Date_ &start,
                                   const Date_ &end,
                                   double notional,
                                   const DayBasis_ &day_count,
                                   const Handle_<DayBasis::Context_> &context,
                                   bool is_stub)
            : startDate_(start),
            endDate_(end),
            notional_(notional),
            couponBasis_(day_count),
            dcf_(day_count(start, end, context.get())),
            context_(context),
            isStub_(is_stub) {}
}
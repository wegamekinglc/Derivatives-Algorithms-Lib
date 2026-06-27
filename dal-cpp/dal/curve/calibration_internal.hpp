//
// Created by dal-implementer on 2026/6/27.
//

#pragma once

#include <algorithm>
#include <dal/curve/ycinstrument.hpp>
#include <dal/math/vectors.hpp>
#include <dal/platform/platform.hpp>
#include <dal/protocol/accrualperiod.hpp>
#include <dal/protocol/rateconvention.hpp>
#include <dal/time/schedules.hpp>

namespace Dal {

    // Shared internal helpers for single-curve and joint-curve calibration and leg-period
    // construction. Hoisted from calibration.cpp / jointcalibration.cpp / ycinstrument.cpp /
    // xccyinstrument.cpp to eliminate byte-identical duplication. Marked inline so the header-only
    // definitions do not violate the ODR across translation units.

    // Stable ordering of curve-calibration instruments: by maturity, then by start, then by name.
    inline Vector_<Handle_<YCInstrument_>> OrderInstruments(const Vector_<Handle_<YCInstrument_>>& instruments) {
        auto ordered = instruments;
        std::sort(ordered.begin(), ordered.end(), [](const Handle_<YCInstrument_>& lhs, const Handle_<YCInstrument_>& rhs) {
            const auto lhsSpan = lhs->TimeSpan();
            const auto rhsSpan = rhs->TimeSpan();
            if (lhsSpan.second != rhsSpan.second)
                return lhsSpan.second < rhsSpan.second;
            if (lhsSpan.first != rhsSpan.first)
                return lhsSpan.first < rhsSpan.first;
            return lhs->Name() < rhs->Name();
        });
        return ordered;
    }

    // Resolve the float index convention of a curve instrument, or nullptr if it has none.
    // Used by Phase-A analytic-Jacobian eligibility checks (single-curve) and projection-curve
    // routing (joint). Returns a borrowed pointer; the instrument outlives the call.
    inline const RateIndexConvention_* FloatConventionOf(const YCInstrument_& inst) {
        if (const auto* deposit = dynamic_cast<const Deposit_*>(&inst))
            return &deposit->FloatConvention();
        if (const auto* fra = dynamic_cast<const FRA_*>(&inst))
            return &fra->FloatConvention();
        if (const auto* future = dynamic_cast<const Future_*>(&inst))
            return &future->FloatConvention();
        if (const auto* swap = dynamic_cast<const Swap_*>(&inst))
            return &swap->FloatConvention();
        return nullptr;
    }

    // Wrap a schedule period as an accrual period with unit notional under the given day basis.
    inline AccrualPeriod_ MakeAccrualPeriod(const SchedulePeriod_& period, const DayBasis_& basis) {
        return AccrualPeriod_(period.accrualStart_, period.accrualEnd_, 1.0, basis, period.dayCountContext_, period.isStub_);
    }

    // Build a leg's coupon periods. PeriodT must be an aggregate initializable as
    // {SchedulePeriod_, AccrualPeriod_} (e.g. CouponPeriod_, XccyCouponPeriod_).
    template <class PeriodT_>
    Vector_<PeriodT_> BuildLegPeriods(
        const Date_& start, const Date_& maturity, const RateLegConvention_& legConvention, int fixingLag, const Holidays_& fixingHolidays) {
        Vector_<PeriodT_> retval;
        for (const auto& period :
             MakeSchedulePeriods(start, maturity, legConvention.paymentFrequency_, legConvention.accrualHolidays_, fixingLag, fixingHolidays,
                                 legConvention.paymentLag_, legConvention.paymentHolidays_, DateGeneration_("Forward"),
                                 legConvention.businessDayConvention_, legConvention.paymentConvention_, legConvention.endOfMonth_)) {
            retval.push_back({period, MakeAccrualPeriod(period, legConvention.dayBasis_)});
        }
        return retval;
    }

} // namespace Dal

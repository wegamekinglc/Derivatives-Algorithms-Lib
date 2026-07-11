//
// Created by dal-implementer on 2026/6/27.
//

#pragma once

#include <algorithm>
#include <cmath>
#include <type_traits>
#include <utility>

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

    // 4-way dispatch over {Deposit_, FRA_, Future_, Swap_}. OISSwap_ reaches the Swap_ arm via
    // inheritance. Each caller supplies its per-type lambda; on no match the common return type is
    // default-constructed (nullptr for pointers, empty Handle_<>, false for bool).
    template <class D_, class F_, class Fu_, class S_>
    auto VisitRate(const YCInstrument_& inst, D_&& depositFn, F_&& fraFn, Fu_&& futureFn, S_&& swapFn)
        -> std::common_type_t<std::invoke_result_t<D_, const Deposit_&>,
                              std::invoke_result_t<F_, const FRA_&>,
                              std::invoke_result_t<Fu_, const Future_&>,
                              std::invoke_result_t<S_, const Swap_&>> {
        using R_ = std::common_type_t<std::invoke_result_t<D_, const Deposit_&>, std::invoke_result_t<F_, const FRA_&>,
                                      std::invoke_result_t<Fu_, const Future_&>, std::invoke_result_t<S_, const Swap_&>>;
        if (const auto* deposit = dynamic_cast<const Deposit_*>(&inst))
            return static_cast<R_>(std::forward<D_>(depositFn)(*deposit));
        if (const auto* fra = dynamic_cast<const FRA_*>(&inst))
            return static_cast<R_>(std::forward<F_>(fraFn)(*fra));
        if (const auto* future = dynamic_cast<const Future_*>(&inst))
            return static_cast<R_>(std::forward<Fu_>(futureFn)(*future));
        if (const auto* swap = dynamic_cast<const Swap_*>(&inst))
            return static_cast<R_>(std::forward<S_>(swapFn)(*swap));
        return R_{};
    }

    // Resolve the float index convention of a curve instrument, or nullptr if it has none.
    // Used by Phase-A analytic-Jacobian eligibility checks (single-curve) and projection-curve
    // routing (joint). Returns a borrowed pointer; the instrument outlives the call.
    inline const RateIndexConvention_* FloatConventionOf(const YCInstrument_& inst) {
        return VisitRate(
            inst, [](const Deposit_& d) -> const RateIndexConvention_* { return &d.FloatConvention(); },
            [](const FRA_& f) -> const RateIndexConvention_* { return &f.FloatConvention(); },
            [](const Future_& fu) -> const RateIndexConvention_* { return &fu.FloatConvention(); },
            [](const Swap_& s) -> const RateIndexConvention_* { return &s.FloatConvention(); });
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

    // maxAbs and RMS of a residual sequence. Single-pass, same accumulation order as the inline
    // loops it replaces, so floating-point output is byte-identical to the originals.
    struct ResidualStats_ {
        double maxAbsResidual_ = 0.0;
        double rmsResidual_ = 0.0;
    };

    template <class E_> ResidualStats_ ResidualStats(const Vector_<E_>& residuals) {
        double maxAbs = 0.0;
        double sq = 0.0;
        for (const E_ r : residuals) {
            maxAbs = std::max(maxAbs, std::fabs(r));
            sq += r * r;
        }
        return ResidualStats_{maxAbs, residuals.empty() ? 0.0 : std::sqrt(sq / residuals.size())};
    }

    // Resolve the coupon-months count for a single-period instrument's day-count context.
    // Forecasts off a projection curve use the curve's tenor; everything else uses CouponMonths.
    inline int SinglePeriodCouponMonths(const RateIndexConvention_& convention, const Date_& start, const Date_& maturity) {
        return convention.useProjectionCurve_ ? convention.forecastTenor_.Months() : CouponMonths(start, maturity);
    }

    // Build the SchedulePeriod_ shared by every single-period rate (Deposit, FRA, Future, and their
    // projection variants). The caller supplies the resolved couponMonths so both the
    // CouponMonths(...) and useProjectionCurve_ ternary call-sites collapse to one schedule build.
    inline SchedulePeriod_
    BuildSinglePeriodSchedule(const Date_& start, const Date_& maturity, const RateIndexConvention_& convention, int couponMonths) {
        SchedulePeriod_ period;
        period.unadjustedStart_ = start;
        period.unadjustedEnd_ = maturity;
        period.accrualStart_ = Holidays::Adjust(convention.accrualHolidays_, start, convention.businessDayConvention_);
        period.accrualEnd_ = Holidays::Adjust(convention.accrualHolidays_, maturity, convention.businessDayConvention_);
        period.dayCountContext_ = Handle_<DayBasis::Context_>(new DayBasis::Context_(true, start, maturity, couponMonths));
        return period;
    }

} // namespace Dal

//
// Created by dal-implementer on 2026/6/15.
//

#pragma once

#include <dal/curve/discount.hpp>
#include <dal/platform/platform.hpp>

namespace Dal {
    // Phase A templated yield-curve context. Holds a single DiscountCurveT_<T_> reference -- the
    // calibrated target curve that the templated rates read DFs from. This is NOT a YieldCurve_
    // subclass; it exists only for the AAD-tape residual evaluation in calibration.cpp, where
    // forecast == discount (Phase A eligibility) so the rate never needs a separate forecast slot.
    // Multi-curve / FORECAST-target calibrations are out of Phase A scope and never construct a
    // YCCtxT_<Number_>.
    template <class T_>
    struct YCCtxT_ {
        const DiscountCurveT_<T_>& curve_;
        explicit YCCtxT_(const DiscountCurveT_<T_>& curve) : curve_(curve) {}
    };
} // namespace Dal

#pragma once

#include <dal/curve/discount.hpp>

namespace Dal {
    struct PiecewiseConstant_;

    DiscountCurve_* NewDiscountPWC(const String_& name,
                                   const String_& ccy,
                                   const PiecewiseConstant_& fwds,
                                   const Handle_<DiscountCurve_>& base = Handle_<DiscountCurve_>());
} // namespace Dal

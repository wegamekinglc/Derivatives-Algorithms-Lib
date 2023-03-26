//
// Created by wegam on 2023/3/26.
//

#pragma once

#include <dal/curve/piecewiselinear.hpp>
#include <dal/curve/yc.hpp>

namespace Dal {

    DiscountCurve_* NewDiscountPWLF(const String_ &name,
                                    const PiecewiseLinear_& fwds,
                                    const Handle_ <DiscountCurve_>& base = Handle_<DiscountCurve_>());
}

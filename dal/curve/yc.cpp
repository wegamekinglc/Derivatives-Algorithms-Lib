//
// Created by wegam on 2023/3/26.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/curve/yc.hpp>

namespace Dal {
    YieldCurve_::YieldCurve_(const String_ &name, const String_ &ccy)
    : Storable_("YieldCurve", name), ccy_(ccy) {}
}
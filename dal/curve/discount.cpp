//
// Created by wegam on 2023/3/26.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/curve/discount.hpp>

namespace Dal {
    DiscountCurve_::DiscountCurve_(const String_ &name): YCComponent_("DiscountCurve", name) {}
}
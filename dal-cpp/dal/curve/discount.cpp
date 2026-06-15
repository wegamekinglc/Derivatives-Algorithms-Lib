//
// Created by wegam on 2023/3/26.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/curve/discount.hpp>

namespace Dal {
    // Tape::DiscountCurve_<T_> ctor is defined inline in the header (Phase A templatization). The
    // double alias DiscountCurve_ = Tape::DiscountCurve_<double> inherits it, so there is no
    // out-of-line ctor to define here. The translation unit stays so existing link expectations
    // (the symbol was historically part of the lib) do not regress.
} // namespace Dal

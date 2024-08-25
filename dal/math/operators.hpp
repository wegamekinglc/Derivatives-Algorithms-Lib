//
// Created by wegam on 2021/8/8.
//

#pragma once

#include <cmath>
#include <dal/math/aad/aad.hpp>

namespace Dal {
    using std::sqrt;
    using std::exp;
    using std::fabs;
    using std::max;
    using std::min;
    using std::pow;
    using std::erfc;
    using std::log;
    using AAD::Number_;
    FORCE_INLINE constexpr double value(double d) { return d; }

    using AAD::operator*;
    using AAD::operator+;
    using AAD::operator/;
    using AAD::operator-;
    using AAD::operator!=;
    using AAD::operator<;
    using AAD::operator<=;
    using AAD::operator>;
    using AAD::operator>=;
    using AAD::operator==;
    using AAD::exp;
    using AAD::fabs;
    using AAD::log;
    using AAD::max;
    using AAD::min;
    using AAD::pow;
    using AAD::NCDF;
    using AAD::NPDF;
    using AAD::erfc;
    using AAD::sqrt;
    FORCE_INLINE double value(const Number_& d) { return d.value(); }

} // namespace Dal

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
    FORCE_INLINE double value(double d) { return d; }

#ifdef USE_CODI
    FORCE_INLINE double value(const Number_& d) { return d.value(); }
    template <class T_>
    FORCE_INLINE T_ NCDF(const T_& z) { return 0.5 * erfc(-z / M_SQRT_2); }
    template <class T_>
    FORCE_INLINE T_ NPDF(const T_& z) { return exp(-0.5 * z * z) / M_SQRT_2_PI; }
#endif

#ifdef USE_AADET
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
#endif
} // namespace Dal

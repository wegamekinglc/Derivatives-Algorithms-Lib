//
// Created by wegam on 2021/8/8.
//

#pragma once
#include <dal/math/aad/aad.hpp>
#include <dal/platform/platform.hpp>
#include <cmath>

namespace Dal {
    FORCE_INLINE double Sqrt(double t) { return std::sqrt(t); }
    FORCE_INLINE double Exp(double t) { return std::exp(t); }
    FORCE_INLINE double Fabs(double t) { return std::fabs(t); }
    FORCE_INLINE double Log(double t) { return std::log(t); }
    FORCE_INLINE double Pow(double t, int u) { return std::pow(t, u); }
    FORCE_INLINE double Pow(double t, double u) { return std::pow(t, u); }
    FORCE_INLINE double Plus(double t1, double t2) { return t1 + t2; }

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
    using AAD::Exp;
    using AAD::Fabs;
    using AAD::Log;
    using AAD::Max;
    using AAD::Min;
    using AAD::Pow;
    using AAD::NCDF;
    using AAD::NPDF;
    using AAD::Sqrt;
} // namespace Dal

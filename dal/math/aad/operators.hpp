//
// Created by wegam on 2021/8/8.
//

#pragma once
#include <dal/platform/platform.hpp>
#include <dal/math/aad/aad.hpp>
#include <cmath>

namespace Dal::AAD {
    using Dal::Square;
    using Dal::Max;
    using Dal::Min;
    using Dal::NCDF;
    using Dal::NPDF;
    FORCE_INLINE double Sqrt(double t) { return std::sqrt(t); }
    FORCE_INLINE double Exp(double t) { return std::exp(t); }
    FORCE_INLINE double Fabs(double t) { return std::fabs(t); }
    FORCE_INLINE double Log(double t) { return std::log(t); }
    FORCE_INLINE double Pow(double t, int u) { return std::pow(t, u); }
    FORCE_INLINE double Pow(double t, double u) { return std::pow(t, u); }
    FORCE_INLINE double Plus(double t1, double t2) { return t1 + t2; }
} // namespace Dal

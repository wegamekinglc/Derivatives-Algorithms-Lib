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
    inline double Sqrt(double t) { return std::sqrt(t); }
    inline double Exp(double t) { return std::exp(t); }
    inline double Fabs(double t) { return std::fabs(t); }
    inline double Log(double t) { return std::log(t); }
    inline double Pow(double t, int u) { return std::pow(t, u); }
    inline double Pow(double t, double u) { return std::pow(t, u); }
    inline double Plus(double t1, double t2) { return t1 + t2; }
} // namespace Dal

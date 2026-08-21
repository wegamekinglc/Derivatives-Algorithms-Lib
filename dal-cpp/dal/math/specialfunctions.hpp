//
// Created by wegam on 2020/12/16.
//

#pragma once

#include <cmath>
#include <dal/platform/consts.hpp>

namespace Dal {
    inline double NPDF(double z) { return z < -10.0 || 10.0 < z ? 0.0 : std::exp(-0.5 * z * z) / M_SQRT_2_PI; }
    double NCDF(double z, bool precise = true);
    double InverseNCDF(double x, bool precise = true, bool polish = true);
} // namespace Dal

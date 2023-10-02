//
// Created by wegam on 2022/4/3.
//

#pragma once

#include <dal/math/vectors.hpp>

namespace Dal {
    // raw function to compute z-vals used in smoothing spline
    // assumes x are sorted and distinct
    Vector_<> SmoothedVals(const Vector_<>& x,
                           const Vector_<>& y,
                           const Vector_<>& weight = Vector_<>(),
                           double lambda = 0.0);
}

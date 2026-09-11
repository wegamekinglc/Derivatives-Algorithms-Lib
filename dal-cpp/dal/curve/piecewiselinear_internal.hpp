#pragma once

#include <dal/math/vectors.hpp>
#include <dal/time/date.hpp>

namespace Dal::PiecewiseLinearInternal {
    // Shared arithmetic for the standalone interpolant and cached double discount curve.
    // Keep the expression order stable: calibration depends on the historical double path.
    Vector_<> Sofar(const Vector_<Date_>& knots, const Vector_<>& left, const Vector_<>& right);
    double IntegralTo(const Vector_<Date_>& knots, const Vector_<>& left, const Vector_<>& right, const Vector_<>& sofar, const Date_& date);
} // namespace Dal::PiecewiseLinearInternal

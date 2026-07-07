//
// Created by dal-implementer on 2026/7/8.
//

#pragma once

#include <dal/math/pde/pde.hpp>

namespace Dal::PDE {
    Vector_<> GridLocations(const CoordinateVector_& points);

    CoordinateVector_ MakeUniformGrid(double xLow, double xHigh, int n);
    CoordinateVector_ MakeConcentratingGrid(double xLow, double xHigh, int n, double cPoint, double density);
} // namespace Dal::PDE

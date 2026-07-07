//
// Created by dal-implementer on 2026/7/8.
//

#include <dal/math/pde/pdegrid.hpp>
#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal::PDE {
    namespace {
        void RequireGridShape(const CoordinateVector_& points) {
            REQUIRE(points.n_ >= 3, "grid requires at least 3 points");
            REQUIRE(points.yHigh_ > points.yLow_, "grid requires yHigh > yLow");
            REQUIRE(!points.yToX_.IsEmpty(), "grid requires a coordinate map (yToX_ is empty)");
        }
    } // namespace

    Vector_<> GridLocations(const CoordinateVector_& points) {
        RequireGridShape(points);

        Vector_<> locations(points.n_);
        const double dy = (points.yHigh_ - points.yLow_) / (points.n_ - 1);
        for (int i = 0; i < points.n_; ++i) {
            double y = points.yLow_ + i * dy;
            if (i == 0)
                y = points.yLow_;
            else if (i == points.n_ - 1)
                y = points.yHigh_;
            locations[i] = (*points.yToX_)(y, nullptr, nullptr);
        }

        for (int i = 1; i < points.n_; ++i)
            REQUIRE(locations[i - 1] < locations[i], "grid locations must be strictly increasing");
        return locations;
    }

    CoordinateVector_ MakeUniformGrid(double xLow, double xHigh, int n) {
        CoordinateVector_ points{xLow, xHigh, n, Handle_<CoordinateMap_>(NewIdentityMap())};
        RequireGridShape(points);
        return points;
    }

    CoordinateVector_ MakeConcentratingGrid(double xLow, double xHigh, int n, double cPoint, double density) {
        CoordinateVector_ points{0.0, 1.0, n, Handle_<CoordinateMap_>(NewConcentratingMap(xLow, xHigh, cPoint, density))};
        RequireGridShape(points);
        return points;
    }
} // namespace Dal::PDE

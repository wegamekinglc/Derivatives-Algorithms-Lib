//
// Created by dal-implementer on 2026/7/8.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/math/pde/pdegrid.hpp>
#include <dal/utilities/exceptions.hpp>

using namespace Dal;
using namespace Dal::PDE;

TEST(PdeGridTest, TestUniformGridPinsEndpointsAndSamplesInterior) {
    const CoordinateVector_ grid = MakeUniformGrid(0.0, 1.0, 6);
    ASSERT_EQ(grid.yLow_, 0.0);
    ASSERT_EQ(grid.yHigh_, 1.0);
    ASSERT_EQ(grid.n_, 6);

    const Vector_<> x = GridLocations(grid);
    ASSERT_EQ(x.size(), 6);
    ASSERT_EQ(x.front(), 0.0);
    ASSERT_EQ(x.back(), 1.0);

    const double dy = (grid.yHigh_ - grid.yLow_) / (grid.n_ - 1);
    for (int i = 1; i < grid.n_ - 1; ++i)
        ASSERT_EQ(x[i], grid.yLow_ + i * dy);
}

TEST(PdeGridTest, TestConcentratingGridPinsEndpointsAndClustersAroundPoint) {
    const double xLow = 0.0;
    const double xHigh = 5.0;
    const double cPoint = 2.5;
    const CoordinateVector_ grid = MakeConcentratingGrid(xLow, xHigh, 51, cPoint, 0.1);
    const Vector_<> x = GridLocations(grid);

    ASSERT_EQ(x.front(), xLow);
    ASSERT_EQ(x.back(), xHigh);
    for (int i = 1; i < static_cast<int>(x.size()); ++i)
        ASSERT_LT(x[i - 1], x[i]);

    int smallestSpacing = 0;
    double minDx = x[1] - x[0];
    for (int i = 1; i < static_cast<int>(x.size()) - 1; ++i) {
        const double dx = x[i + 1] - x[i];
        if (dx < minDx) {
            minDx = dx;
            smallestSpacing = i;
        }
    }
    const double intervalMid = 0.5 * (x[smallestSpacing] + x[smallestSpacing + 1]);
    ASSERT_NEAR(intervalMid, cPoint, 0.25);
}

TEST(PdeGridTest, TestGridValidation) {
    ASSERT_THROW(MakeUniformGrid(1.0, 0.0, 5), Exception_);
    ASSERT_THROW(MakeUniformGrid(0.0, 1.0, 2), Exception_);
    ASSERT_THROW(MakeConcentratingGrid(0.0, 1.0, 2, 0.5, 0.1), Exception_);

    CoordinateVector_ noMap{0.0, 1.0, 5, Handle_<CoordinateMap_>()};
    ASSERT_THROW(GridLocations(noMap), Exception_);

    CoordinateVector_ reversed{1.0, 0.0, 5, Handle_<CoordinateMap_>(NewIdentityMap())};
    ASSERT_THROW(GridLocations(reversed), Exception_);
}

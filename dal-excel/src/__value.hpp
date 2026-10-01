//
// Created by wegam on 2026/7/11.
//

#pragma once

#include <cmath>
#include <limits>

#include <dal/utilities/exceptions.hpp>

namespace Dal {
    namespace Excel {
        inline int CheckedPathCount(double nPaths, int minimum) {
            REQUIRE(std::isfinite(nPaths), "number of paths must be finite");
            REQUIRE(std::trunc(nPaths) == nPaths, "number of paths must be exactly integral");
            REQUIRE(nPaths >= minimum && nPaths <= static_cast<double>(std::numeric_limits<int>::max()),
                    "number of paths is outside the supported integer range");
            return static_cast<int>(nPaths);
        }

        inline int CheckedMonteCarloPathCount(double nPaths) { return CheckedPathCount(nPaths, 1); }
    } // namespace Excel
} // namespace Dal

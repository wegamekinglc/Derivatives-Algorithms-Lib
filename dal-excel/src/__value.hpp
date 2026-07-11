//
// Created by wegam on 2026/7/11.
//

#pragma once

#include <dal/utilities/exceptions.hpp>

#include <cmath>
#include <limits>

namespace Dal {
    namespace Excel {
        inline int CheckedMonteCarloPathCount(double nPaths) {
            REQUIRE(std::isfinite(nPaths), "number of Monte Carlo paths must be finite");
            REQUIRE(std::trunc(nPaths) == nPaths, "number of Monte Carlo paths must be exactly integral");
            REQUIRE(nPaths >= 1.0 && nPaths <= static_cast<double>(std::numeric_limits<int>::max()),
                    "number of Monte Carlo paths must be between 1 and INT_MAX");
            return static_cast<int>(nPaths);
        }
    } // namespace Excel
} // namespace Dal

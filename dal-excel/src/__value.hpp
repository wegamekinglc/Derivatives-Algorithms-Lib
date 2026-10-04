//
// Created by wegam on 2026/7/11.
//

#pragma once

#include <cmath>
#include <limits>
#include <map>

#include <dal-public/src/types.hpp>

namespace Dal {
    namespace Excel {
        inline Matrix_<Cell_> MonteCarloPriceTable(const std::map<String_, double>& prices) {
            Matrix_<Cell_> values(static_cast<int>(prices.size()), 2);
            int row = 0;
            for (const auto& price : prices) {
                values(row, 0) = price.first;
                values(row, 1) = price.second;
                ++row;
            }
            return values;
        }

        inline int CheckedPathCount(double nPaths, int minimum) {
            REQUIRE(std::isfinite(nPaths), "number of paths must be finite");
            REQUIRE(std::trunc(nPaths) == nPaths, "number of paths must be exactly integral");
            REQUIRE(nPaths >= minimum && nPaths <= static_cast<double>(std::numeric_limits<int>::max()),
                    "number of paths is outside the supported integer range");
            return static_cast<int>(nPaths);
        }

        inline int CheckedMonteCarloPathCount(double nPaths) { return CheckedPathCount(nPaths, 1); }

        inline int CheckedMonteCarloPathCount(double nPaths, const String_& function) {
            try {
                return CheckedMonteCarloPathCount(nPaths);
            } catch (const Exception_& error) {
                THROW("InvalidPathCount: " + function + "; n_paths; " + String_(error.what()));
            }
        }
    } // namespace Excel
} // namespace Dal

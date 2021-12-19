//
// Created by wegam on 2020/11/16.
//

#include <dal/platform/platform.hpp>
#include <dal/utilities/numerics.hpp>
#include <dal/utilities/exceptions.hpp>
#include <cmath>

namespace Dal {
    int AsInt(double src) {
        REQUIRE(std::fabs(src) < 2147483647., "Number is too large to be an integer");
        return static_cast<int>(src + (src > 0 ? 1e-9 : -1e-9));
    }

    int NearestInt(double src) {
        REQUIRE(std::fabs(src) < 2147483647, "Number is too large to be an integer");
        return static_cast<int>(src + (src > 0 ? 0.5 : -0.5));
    }

    int AsInt(std::ptrdiff_t src) {
        REQUIRE(std::abs(src) < 2147483647, "32-bit integer overflow");
        return static_cast<int>(src);
    }
}

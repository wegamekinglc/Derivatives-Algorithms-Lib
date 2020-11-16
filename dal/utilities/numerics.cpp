//
// Created by wegam on 2020/11/16.
//

#include <dal/platform/platform.hpp>
#include <dal/utilities/numerics.hpp>
#include <dal/utilities/exceptions.hpp>
#include <dal/utilities/algorithms.hpp>
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

    namespace Vector {
        Vector_<> L1Normalized(const Vector_<>& base) {
            using val_type = typename Vector_<>::value_type;
            auto func = [](val_type x, val_type y) {
                return x + std::fabs(y);
            };
            auto l1 = Accumulate(base, func);
            auto func2 = [&l1](val_type  x){ return x / (l1 + 1e-9);};
            return Apply(func2, base);
        }

        Vector_<> L2Normalized(const Vector_<>& base) {
            using val_type = typename Vector_<>::value_type;
            auto func = [](val_type x, val_type y) {
              return x + y * y;
            };
            auto l2 = std::sqrt(Accumulate(base, func));
            auto func2 = [&l2](val_type  x){ return x / (l2 + 1e-9);};
            return Apply(func2, base);
        }
    }
}
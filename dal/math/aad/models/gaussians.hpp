//
// Created by wegam on 2022/9/11.
//

#pragma once

#include <dal/platform/platform.hpp>
#include <dal/math/aad/operators.hpp>

namespace Dal::AAD {

    template <class T_>
    inline T_ NormalPDF(const T_& x) {
        return x < -10.0 || 10.0 < x ? T_(0.0) : Exp(-0.5 * x * x) / 2.506628274631;
    }

    template <class T_>
    inline T_ NormalCDF(const T_& x) {
        if (x < -10.0)
            return T_(0.0);
        if (x > 10.0)
            return T_(1.0);
        if (x < 0.0)
            return T_(1.0 - NormalCDF<T_>(-x));

        static constexpr double p = 0.2316419;
        static constexpr double b1 = 0.319381530;
        static constexpr double b2 = -0.356563782;
        static constexpr double b3 = 1.781477937;
        static constexpr double b4 = -1.821255978;
        static constexpr double b5 = 1.330274429;

        const auto t = 1.0 / (1.0 + p * x);

        const auto pol = t * (b1 + t * (b2 + t * (b3 + t * (b4 + t * b5))));

        const auto pdf = NormalPDF<T_>(x);

        return T_(1.0 - pdf * pol);
    }
} // namespace Dal::AAD
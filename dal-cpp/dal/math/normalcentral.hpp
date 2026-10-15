//
// Created by Codex on 2026/10/16.
//

#pragma once

#include <cstddef>
#include <dal/math/specialfunctions.hpp>

namespace Dal::Math::Detail {
    inline constexpr double NORMAL_A[] = {-3.969683028665376e+01, 2.209460984245205e+02,  -2.759285104469687e+02,
                                          1.383577518672690e+02,  -3.066479806614716e+01, 2.506628277459239e+00};
    inline constexpr double NORMAL_B[] = {-5.447609879822406e+01, 1.615858368580409e+02,  -1.556989798598866e+02,
                                          6.680131188771972e+01,  -1.328068155288572e+01, 1.0};

    template <class O_> typename O_::Vector_ CentralInverse(typename O_::Vector_ x) {
        const auto z = O_::Subtract(x, O_::Broadcast(0.5));
        const auto r = O_::Multiply(z, z);
        auto numerator = O_::Broadcast(NORMAL_A[0]);
        auto denominator = O_::Broadcast(NORMAL_B[0]);
        for (size_t i = 1; i < 6; ++i) {
            numerator = O_::Add(O_::Multiply(numerator, r), O_::Broadcast(NORMAL_A[i]));
            denominator = O_::Add(O_::Multiply(denominator, r), O_::Broadcast(NORMAL_B[i]));
        }
        return O_::Divide(O_::Multiply(numerator, z), denominator);
    }

    template <class O_> void InverseNormalVector(double* values, size_t n) {
        size_t i = 0;
        for (; i + O_::WIDTH <= n; i += O_::WIDTH) {
            const auto x = O_::Load(values + i);
            if (O_::Central(x))
                O_::Store(values + i, CentralInverse<O_>(x));
            else
                for (size_t lane = 0; lane < O_::WIDTH; ++lane)
                    values[i + lane] = InverseNCDF(values[i + lane], false, false);
        }
        for (; i < n; ++i)
            values[i] = InverseNCDF(values[i], false, false);
    }

    void InverseNormalAvx2(double* values, size_t n);
} // namespace Dal::Math::Detail

//
// Created by dal-implementer on 2026/8/15.
//

#pragma once

#include <cmath>
#include <type_traits>

#include <dal/curve/discount.hpp>
#include <dal/math/aad/aad.hpp>
#include <dal/time/date.hpp>

namespace Dal {
    namespace Tape {
        // Only <double, DiscountCurve_<double>> tape curves can be archived.
        template <class T_, class B_> constexpr bool IsDoubleSerializable() {
            return std::is_same_v<T_, double> && std::is_same_v<B_, DiscountCurve_<double>>;
        }

        // exp(logDf) folded with the optional base-curve multiplier: std::exp on the double path,
        // AAD::exp on the tape path.
        template <class T_, class B_> T_ DiscountFromLogDf(const T_& logDf, const Handle_<B_>& base, const Date_& from, const Date_& to) {
            if constexpr (std::is_same_v<T_, double>) {
                return std::exp(logDf) * (base ? (*base)(from, to) : 1.0);
            } else {
                if (base)
                    return AAD::exp(logDf) * (*base)(from, to);
                return AAD::exp(logDf);
            }
        }
    } // namespace Tape
} // namespace Dal

#pragma once

#include <type_traits>

#include <dal/math/aad/aad.hpp>

namespace Dal::CompoundedOisInternal {
    struct LogOnePlus_ {
        static double Eval(double value, double) { return std::log1p(value); }
        static double Derivative(double value, double, double) { return 1. / (1. + value); }
    };
    struct ExpMinusOne_ {
        static double Eval(double value, double) { return std::expm1(value); }
        static double Derivative(double value, double, double) { return std::exp(value); }
    };
    template <class T_> T_ LogOnePlus(const T_& value) {
        if constexpr (std::is_same_v<T_, double>)
            return std::log1p(value);
        else
            return AAD::UnaryExpression_<T_, LogOnePlus_>(value);
    }
    template <class T_> T_ ExpMinusOne(const T_& value) {
        if constexpr (std::is_same_v<T_, double>)
            return std::expm1(value);
        else
            return AAD::UnaryExpression_<T_, ExpMinusOne_>(value);
    }
} // namespace Dal::CompoundedOisInternal

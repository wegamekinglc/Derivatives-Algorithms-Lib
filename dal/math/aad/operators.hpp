//
// Created by wegam on 2021/8/8.
//

#pragma once
#include <cmath>
#include <dal/math/aad/number.hpp>

namespace Dal {
    template <class T_>
    inline T_ Sqrt(const T_& t) {
        return std::sqrt(t);
    }
    template <class T_>
    inline T_ Exp(const T_& t) {
        return std::exp(t);
    }

    template <class T_>
    inline T_ Log(const T_& t) {
        return std::log(t);
    }

    template <class T_, class U_>
    inline T_ Pow(const T_& t, const U_& u) {
        return std::pow(t, u);
    }
}
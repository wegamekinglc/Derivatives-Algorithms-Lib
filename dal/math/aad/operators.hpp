//
// Created by wegam on 2021/8/8.
//

#pragma once
#include "dal/math/aad/number.hpp"
#include <cmath>

namespace Dal::AAD {
    using Dal::Square;
    using Dal::Max;
    using Dal::Min;
    template <class T_> inline T_ Sqrt(const T_& t) { return std::sqrt(t); }
    template <class T_> inline T_ Exp(const T_& t) { return std::exp(t); }
    template <class T_> inline T_ Fabs(const T_& t) { return std::fabs(t); }
    template <class T_> inline T_ Log(const T_& t) { return std::log(t); }
    template <class T_, class U_> inline T_ Pow(const T_& t, const U_& u) { return std::pow(t, u); }
    template <class T_> inline T_ Plus(const T_& t1, const T_& t2) { return t1 + t2; }
} // namespace Dal

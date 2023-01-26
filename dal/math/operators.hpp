//
// Created by wegam on 2021/8/8.
//

#pragma once

#include <dal/platform/platform.hpp>
#include <dal/math/aad/aad.hpp>
#include <cmath>

namespace Dal {
    using std::sqrt;
    using std::exp;
    using std::fabs;
    using std::max;
    using std::min;
    using std::pow;
    using std::erfc;
    using std::log;

#ifdef USE_ADEPT
    using adept::operator*;
    using adept::operator+;
    using adept::operator/;
    using adept::operator-;
    using adept::operator!=;
    using adept::operator<;
    using adept::operator<=;
    using adept::operator>;
    using adept::operator>=;
    using adept::operator==;
    using adept::exp;
    using adept::fabs;
    using adept::log;
    using adept::max;
    using adept::min;
    using adept::pow;
    using adept::erfc;
    using adept::sqrt;
#else
    using AAD::operator*;
    using AAD::operator+;
    using AAD::operator/;
    using AAD::operator-;
    using AAD::operator!=;
    using AAD::operator<;
    using AAD::operator<=;
    using AAD::operator>;
    using AAD::operator>=;
    using AAD::operator==;
    using AAD::exp;
    using AAD::fabs;
    using AAD::log;
    using AAD::max;
    using AAD::min;
    using AAD::pow;
    using AAD::NCDF;
    using AAD::NPDF;
    using AAD::sqrt;
#endif
} // namespace Dal

//
// Created by wegam on 2022/5/9.
//

#pragma once

#include <dal/platform/platform.hpp>
#include <dal/math/vectors.hpp>
#include <dal/string/strings.hpp>

namespace Dal {
    class Interp1_;
    Handle_<Interp1_> Interp1NewLinear(const String_& name, const Vector_<>& x, const Vector_<>& y);
}
//
// Created by wegam on 2026/4/10.
//

#pragma once

#include <dal/math/interp/interp.hpp>

namespace Dal {
    namespace Interp {
        Interp1_* NewLogLinear(const String_& name, const Vector_<>& x, const Vector_<>& f);
    } // namespace Interp
} // namespace Dal

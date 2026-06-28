//
// Created by wegam on 2020/10/25.
//

#pragma once

#include <dal/math/interp/interp.hpp>


namespace Dal {

    namespace Interp {
        Interp1_* NewLinear(const String_& name, const Vector_<>& x, const Vector_<>& f);
    } // namespace Interp
} // namespace Dal

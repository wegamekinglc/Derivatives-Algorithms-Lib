//
// Created by wegam on 2022/5/9.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <public/src/interp.hpp>
#include <dal/math/interp/interp.hpp>

namespace Dal {
    Handle_<Interp1_> Interp1NewLinear(const String_& name,
                                       const Vector_<>& x,
                                       const Vector_<>& y) {
        return Handle_<Interp1_>(Interp::NewLinear(name, x, y));
    }
}
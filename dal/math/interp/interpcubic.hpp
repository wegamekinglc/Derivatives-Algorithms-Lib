//
// Created by wegam on 2020/12/16.
//

#pragma once
#include <dal/math/interp/interp.hpp>

namespace Dal {
    class String_;
    namespace Interp {
        struct Boundary_ {
            int order_;
            double value_;
            Boundary_(int o, double v)
                :order_(o), value_(v) {}
        };

        Interp1_* NewCubic(const String_& name,
                           const Vector_<>& x,
                           const Vector_<>& f,
                           const Boundary_& lhs,
                           const Boundary_& rhs);
    }
}
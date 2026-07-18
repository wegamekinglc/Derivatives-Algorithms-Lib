//
// Created by wegam on 2020/10/25.
//

#pragma once

#include <dal/math/interp/interp.hpp>


namespace Dal {

    namespace Interp {
        std::unique_ptr<Interp1_> NewLinear(const String_& name, const Vector_<>& x, const Vector_<>& f);
    } // namespace Interp
} // namespace Dal

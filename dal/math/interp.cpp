//
// Created by wegam on 2020/11/6.
//

#include <dal/platform/platform.hpp>
#include <dal/math/interp.hpp>
#include <dal/platform/strict.hpp>

namespace Dal {
    Interp1_::Interp1_(const String_& name) :Storable_("Interp1", name) {}

    Interp1Linear_::Interp1Linear_(const String_& name, const Vector_<>& x, const Vector_<>& f)
    :Interp1_(name), x_(x), f_(f) {
        REQUIRE(x_.size() == f_.size(), "x_ size must be equal to f_ size");
        REQUIRE(IsMonotonic(x, std::less_equal()), "x_ array should be monotonic");
    }
}
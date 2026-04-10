//
// Created by wegam on 2020/10/25.
//

#pragma once

#include <dal/math/interp/interp.hpp>

/*IF--------------------------------------------------------------------------
storable Interp1Linear
        Linear interpolator on known values in one dimension
version 1
&members
name is ?string
x is number[]
f is number[]
-IF-------------------------------------------------------------------------*/

namespace Dal {

    class BASE_EXPORT Interp1Linear_ : public Interp1_ {
        Vector_<> x_;
        Vector_<> f_;

    public:
        Interp1Linear_(const String_& name, const Vector_<>& x, const Vector_<>& f);
        Interp1Linear_(const String_& name, const std::map<double, double>& f);
        void Write(Archive::Store_& dst) const override;
        double operator()(double x) const override;
        [[nodiscard]] const Vector_<>& x() const { return x_; }
        [[nodiscard]] const Vector_<>& f() const { return f_; }
    };

    namespace Interp {
        Interp1_* NewLinear(const String_& name, const Vector_<>& x, const Vector_<>& f);
    }
} // namespace Dal

//
// Created by wegam on 2022/5/2.
//

#pragma once

#include <dal/storage/archive.hpp>
#include <dal/math/interp/interp.hpp>

/*IF--------------------------------------------------------------------------
storable Interp2Linear
        Linear interpolator on known values in two dimensions
version 1
&members
name is ?string
x is number[]
y is number[]
f is number[][]
-IF-------------------------------------------------------------------------*/

namespace Dal {
    class BASE_EXPORT Interp2_: public Storable_ {
    public:
        explicit Interp2_(const String_& name);
        virtual double operator()(double x, double y) const = 0;
        virtual bool IsInBounds(double x, double y) const { return true; }
    };

    template <class T_ = double>
    inline T_ Interp2DLinearImplX(const Vector_<>& x, const Vector_<>& y, const Matrix_<T_>& f, double x0, double y0) {
        const size_t n = x.size();

        auto it = UpperBound(x, x0);
        const size_t n2 = std::distance(x.begin(), it);

        // extrapolation
        if (n2 == n)
            return InterpLinearImplX<T_>(y, f.Row(n2 - 1), y0);
        if (n2 == 0)
            return InterpLinearImplX<T_>(y, f.Row(0), y0);

        // interpolation
        const size_t n1 = n2 - 1;
        auto x1 = x[n1];
        auto x2 = x[n2];
        auto z1 = InterpLinearImplX<T_>(y, f.Row(n1), y0);
        auto z2 = InterpLinearImplX<T_>(y, f.Row(n2), y0);

        auto t = (x0 - x1) / (x2 - x1);
        return z1 + (z2 - z1) * t;
    }

    class BASE_EXPORT Interp2Linear_ : public Interp2_ {
        Vector_<> x_;
        Vector_<> y_;
        Matrix_<> f_;

    public:
        Interp2Linear_(const String_& name, const Vector_<>& x, const Vector_<>& y, const Matrix_<>& f);
        void Write(Archive::Store_& dst) const override;
        double operator()(double x, double y) const override;
        const Vector_<>& x() const { return x_; }
        const Vector_<>& y() const { return y_; }
    };

    namespace Interp {
        Interp2_* NewLinear2(const String_& name, const Vector_<>& x, const Vector_<>& y, const Matrix_<>& f);
    }
}

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
    template <class T_ = double>
    class BASE_EXPORT Interp2X_: public Storable_ {
    public:
        explicit Interp2X_(const String_& name);
        virtual T_ operator()(double x, double y) const = 0;
        virtual bool IsInBounds(double x, double y) const { return true; }
    };

    template <class T_>
    Interp2X_<T_>::Interp2X_(const String_& name) : Storable_("Interp2", name) {}

    using Interp2_ = Interp2X_<>;

    template <class T_>
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

    template <class T_ = double>
    class BASE_EXPORT Interp2XLinear_ : public Interp2X_<T_> {
        Vector_<> x_;
        Vector_<> y_;
        Matrix_<T_> f_;

    public:
        Interp2XLinear_(const String_& name, const Vector_<>& x, const Vector_<>& y, const Matrix_<T_>& f);
        void Write(Archive::Store_& dst) const override;
        T_ operator()(double x, double y) const override;
        const Vector_<>& x() const { return x_; }
        const Vector_<>& y() const { return y_; }
    };

    template <class T_>
    Interp2XLinear_<T_>::Interp2XLinear_(const String_& name, const Vector_<>& x, const Vector_<>& y, const Matrix_<T_>& f)
        : Interp2_(name), x_(x), y_(y), f_(f) {
        REQUIRE((x_.size() == f_.Rows()) && (y_.size() == f_.Cols()), "x_, y_ size must be equal to f_ size");
        REQUIRE(IsMonotonic(x_, std::less_equal<>()), "x_ array should be monotonic");
        REQUIRE(IsMonotonic(y_, std::less_equal<>()), "y_ array should be monotonic");
    }

    template <class T_>
    T_ Interp2XLinear_<T_>::operator()(double x, double y) const {
        return Interp2DLinearImplX(x_, y_, f_, x, y);
    }

    using Interp2Linear_ = Interp2XLinear_<>;

    namespace Interp {
        Interp2_* NewLinear2(const String_& name, const Vector_<>& x, const Vector_<>& y, const Matrix_<>& f);
    }
}

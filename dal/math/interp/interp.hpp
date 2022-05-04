//
// Created by wegam on 2020/10/25.
//

#pragma once

#include <dal/storage/archive.hpp>
#include <map>

namespace Dal {

    template <class T_ = double>
    class BASE_EXPORT Interp1X_ : public Storable_ {
    public:
        explicit Interp1X_(const String_& name);
        virtual T_ operator()(double x) const = 0;
        virtual bool IsInBounds(double x) const { return true; }
    };

    using Interp1_ = Interp1X_<>;

    template <class T_>
    Interp1X_<T_>::Interp1X_(const String_& name) : Storable_("Interp1", name) {}
} // namespace Dal

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

    template <class T_>
    inline T_ InterpLinearImplX(const Vector_<>& x, const Vector_<T_>& y, double x0) {
        auto pge = LowerBound(x, x0);
        if (pge == x.end())
            return y.back();
        else if (pge == x.begin() || IsZero(x0 - *pge))
            return y[pge - x.begin()];
        else {
            auto plt = Previous(pge);
            const double gFrac = (x0 - *plt) / (*pge - *plt);
            auto flt = y.begin() + (plt - x.begin());
            return *flt + gFrac * (*Next(flt) - *flt);
        }
    }

    template <class T_ = double>
    class BASE_EXPORT Interp1XLinear_ : public Interp1X_<T_> {
        Vector_<> x_;
        Vector_<T_> f_;

    public:
        Interp1XLinear_(const String_& name, const Vector_<>& x, const Vector_<T_>& f);
        Interp1XLinear_(const String_& name, const std::map<double, T_>& f);
        void Write(Archive::Store_& dst) const override;
        T_ operator()(double x) const override;
        const Vector_<>& x() const { return x_; }
        const Vector_<T_>& f() const { return f_; }
    };

    template <class T_>
    Interp1XLinear_<T_>::Interp1XLinear_(const String_& name, const Vector_<>& x, const Vector_<T_>& f)
        : Interp1_(name), x_(x), f_(f) {
        REQUIRE(x_.size() == f_.size(), "x_ size must be equal to f_ size");
        REQUIRE(IsMonotonic(x_, std::less_equal<>()), "x_ array should be monotonic");
    }

    template <class T_>
    Interp1XLinear_<T_>::Interp1XLinear_(const String_& name, const std::map<double, T_>& f)
        : Interp1_(name), x_(Keys(f)), f_(MapValues(f)) {
        REQUIRE(IsMonotonic(x_, std::less_equal<>()), "x_ array should be monotonic");
    }

    template <class T_>
    T_ Interp1XLinear_<T_>::operator()(double x) const {
        return InterpLinearImplX(x_, f_, x);
    }

    using Interp1Linear_ = Interp1XLinear_<>;

    namespace Interp {
        Interp1_* NewLinear(const String_& name, const Vector_<>& x, const Vector_<>& f);
    }
} // namespace Dal
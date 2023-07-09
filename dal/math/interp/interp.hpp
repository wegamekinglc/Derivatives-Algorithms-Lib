//
// Created by wegam on 2020/10/25.
//

#pragma once


#include <map>
#include <dal/storage/archive.hpp>
#include <dal/math/operators.hpp>

namespace Dal {

    class BASE_EXPORT Interp1_ : public Storable_ {
    public:
        explicit Interp1_(const String_& name);
        virtual double operator()(double x) const = 0;
        [[nodiscard]] virtual bool IsInBounds(double x) const { return true; }
    };

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

    template <class T_ = double>
    inline T_ InterpLinearImplX(const Vector_<>& x, const Vector_<T_>& y, const T_& x0) {
        auto pge = LowerBound(x, value(x0));
        if (pge == x.end())
            return y.back();
        else if (pge == x.begin() || IsZero(x0 - *pge))
            return y[pge - x.begin()];
        else {
            auto plt = Previous(pge);
            const auto gFrac = value((x0 - *plt) / (*pge - *plt));
            auto flt = y.begin() + (plt - x.begin());
            return *flt + gFrac * (*Next(flt) - *flt);
        }
    }

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
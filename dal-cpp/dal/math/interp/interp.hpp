//
// Created by wegam on 2020/10/25.
//

#pragma once


#include <dal/storage/archive.hpp>
#include <dal/math/operators.hpp>
#include <dal/utilities/algorithms.hpp>

namespace Dal {

    class BASE_EXPORT Interp1_ : public Storable_ {
    public:
        explicit Interp1_(const String_& name);
        virtual double operator()(double x) const = 0;
        [[nodiscard]] virtual bool IsInBounds(double x) const { return true; }
    };

    template <class T_ = double>
    FORCE_INLINE T_ InterpLinearImplX(const Vector_<>& x, const Vector_<T_>& y, const T_& x0) {
        ASSERT(!x.empty() && x.size() == y.size(), "InterpLinearImplX: x and y must be non-empty and the same size");
        auto pge = LowerBound(x, Value(x0));
        if (pge == x.end())
            return y.back();
        if (pge == x.begin() || IsZero(x0 - *pge))
            return y[pge - x.begin()];
        auto plt = Previous(pge);
        const auto gFrac = (Value(x0) - *plt) / (*pge - *plt);
        auto flt = y.begin() + (plt - x.begin());
        return *flt + gFrac * (*Next(flt) - *flt);
    }

} // namespace Dal

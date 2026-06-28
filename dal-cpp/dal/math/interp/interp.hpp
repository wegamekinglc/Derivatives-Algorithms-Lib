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
    FORCE_INLINE T_ InterpLinearImplX(const Vector_<>& x, const Vector_<T_>& y, const T_& x0, size_t* hint = nullptr) {
        REQUIRE(x.size() == y.size(), "InterpLinearImplX: x and y sizes must match");
        const double xv = Value(x0);
        const size_t n = x.size();
        size_t iGE;
        bool resolved = false;
        if (hint && n > 0) {
            size_t idx = *hint < n ? *hint : n - 1;
            if (xv >= x[idx]) {
                while (idx + 1 < n && x[idx + 1] <= xv)
                    ++idx;
            } else {
                while (idx > 0 && x[idx] > xv)
                    --idx;
            }
            const size_t cand = (idx + 1 == n && xv >= x[idx])
                                    ? n
                                    : (x[idx] == xv ? idx : idx + 1);
            const bool valid = (cand == n && (n == 0 || xv >= x[n - 1])) ||
                               (cand < n && x[cand] >= xv && (cand == 0 || x[cand - 1] < xv));
            if (valid) {
                iGE = cand;
                *hint = idx;
                resolved = true;
            }
        }
        if (!resolved)
            iGE = LowerBound(x, xv) - x.begin();
        if (iGE == n)
            return y.back();
        if (iGE == 0 || IsZero(x0 - x[iGE]))
            return y[iGE];
        const double gFrac = (xv - x[iGE - 1]) / (x[iGE] - x[iGE - 1]);
        return y[iGE - 1] + gFrac * (y[iGE] - y[iGE - 1]);
    }

} // namespace Dal

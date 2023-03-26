//
// Created by wegam on 2023/3/26.
//

#pragma once

#include <dal/platform/platform.hpp>
#include <dal/math/vectors.hpp>
#include <dal/time/date.hpp>
#include <dal/storage/storable.hpp>

namespace Dal {
    struct PiecewiseConstant_ : Storable_ {
        Vector_<Date_> knotDates_;
        Vector_<> fRight_;    // nothing to the left of the first knot
        Vector_<> sofar_;    // precomputed integrals to knot dates

        // compute sofar_ (e.g. after a change)
        [[nodiscard]] Vector_<> Sofar() const;

        void Update() { sofar_ = Sofar(); }

        PiecewiseConstant_(const Vector_ <Date_> &knots, const Vector_<> &f_right, const String_ &name = String_())
                : Storable_("PiecewiseConstant", name), knotDates_(knots), fRight_(f_right) { Update(); }

        [[nodiscard]] double IntegralTo(const Date_ &dt) const;

        void Write(Archive::Store_ &dst) const override;
    };

    namespace PWC {
        // at the knot point, returns the limit-from-above (f_right)
        double F(const PiecewiseConstant_ &func,
                 const Date_ &t,
                 bool *is_knot = nullptr);

        inline double Integral(const PiecewiseConstant_ &func,
                               const Date_ &from,
                               const Date_ &to) {
            return func.IntegralTo(to) - func.IntegralTo(from);
        }

        inline PiecewiseConstant_ *NewConstant(double val, const Date_ &from = Date::Minimum()) {
            return new PiecewiseConstant_(Vector::V1(from), Vector::V1(val));
        }
    }
}

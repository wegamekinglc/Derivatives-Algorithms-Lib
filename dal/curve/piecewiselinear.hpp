//
// Created by wegam on 2023/3/26.
//

#pragma once

#include <dal/math/vectors.hpp>

namespace Dal {
    class Date_;


    struct PiecewiseLinear_ {
        Vector_<Date_> knotDates_;
        Vector_<> fLeft_;
        Vector_<> fRight_;
        Vector_<> sofar_;    // precomputed integrals to knot dates

        // compute sofar_ (e.g. after a change)
        [[nodiscard]] Vector_<> Sofar() const;

        void Update() { sofar_ = Sofar(); }

        PiecewiseLinear_(const Vector_<Date_>& knots, const Vector_<>& f_left, const Vector_<>& f_right) : knotDates_(
                knots), fLeft_(f_left), fRight_(f_right) { Update(); }

        [[nodiscard]] double IntegralTo(const Date_ &date) const;
        [[nodiscard]] double ValueAt(const Date_ &date, bool from_right = true) const;
    };
}
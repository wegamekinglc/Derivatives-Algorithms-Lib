//
// Created by dal-implementer on 2026/6/20.
//

#pragma once

#include <dal/curve/discount.hpp>
#include <dal/curve/fittable.hpp>
#include <dal/curve/yccomponent.hpp>
#include <dal/math/aad/aad.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/time/date.hpp>
#include <dal/utilities/algorithms.hpp>

namespace Dal {
    // See docs/methodology/yield_curve_jacobian.md §Joint Multi-Curve Analytic Jacobian.
    namespace Tape {
        constexpr double DAYS_PER_YEAR_PWLF = 365.0;

        template <class T_, class B_ = DiscountCurve_<double>>
        class DiscountPWLF_ : public CurveWithBase_<DiscountCurve_<T_>, B_>, public FittableCurve_ {
            Vector_<Date_> knotDates_;
            // T_-typed PWL forward parameters: fLeftT_[k], fRightT_[k] per knot k. Registered as
            // independents on the tape (the free-parameter vector is 2 * nKnots with NO anchor
            // exclusion -- every knot is free).
            Vector_<T_> fLeftT_;
            Vector_<T_> fRightT_;
            // T_-typed running integral sofarT_[k] = integral of the PWL forward from knot 0 to
            // knot k. T_-typed so the dependence on fLeftT_/fRightT_ records on the tape.
            // Recomputed by UpdateT() whenever fLeftT_/fRightT_ change.
            Vector_<T_> sofarT_;
            // Double knot abscissae (serial-day offsets from knot 0). Computed once at
            // construction; identical for any T_.
            Vector_<double> knotAbscissae_;

            void UpdateT();
            [[nodiscard]] T_ IntegralTo(double t) const;
            [[nodiscard]] T_ IntegralToInterior(double t, int iGE) const;

        public:
            DiscountPWLF_(const String_& name,
                          const String_& ccy,
                          const Vector_<Date_>& knotDates,
                          const Vector_<T_>& fLeftT,
                          const Vector_<T_>& fRightT,
                          const Handle_<B_>& base = Handle_<B_>());

            T_ operator()(const Date_& from, const Date_& to) const override;
            [[nodiscard]] int NX() const override;
            void ApplyDX(Vector_<>::const_iterator dx, double leverage) override;
            void Write(Archive::Store_& dst) const override;
            [[nodiscard]] DiscountPWLF_<T_, B_>* Clone(const String_& new_name,
                                                       const YCComponent_::substitutions_t& base_changes) const override;

            [[nodiscard]] const Vector_<Date_>& KnotDates() const { return knotDates_; }
            [[nodiscard]] Vector_<T_> FLeft() const { return fLeftT_; }
            [[nodiscard]] Vector_<T_> FRight() const { return fRightT_; }
        };
    } // namespace Tape
} // namespace Dal

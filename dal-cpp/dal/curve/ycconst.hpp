//
// Created by wegam on 2026/5/9.
//

#pragma once

#include <dal/curve/discount.hpp>
#include <dal/curve/fittable.hpp>
#include <dal/curve/yccomponent.hpp>
#include <dal/time/date.hpp>

namespace Dal {
    struct PiecewiseConstant_;

    namespace Tape {
        template <class T_, class B_ = DiscountCurve_<double>>
        class DiscountPWC_ : public CurveWithBase_<DiscountCurve_<T_>, B_>, public FittableCurve_ {
            Vector_<Date_> knotDates_;
            Vector_<T_> fRightT_;
            Vector_<T_> sofarT_;

            void UpdateT();
            [[nodiscard]] T_ IntegralTo(const Date_& date) const;

        public:
            DiscountPWC_(const String_& name,
                         const String_& ccy,
                         const Vector_<Date_>& knotDates,
                         const Vector_<T_>& fRightT,
                         const Handle_<B_>& base = Handle_<B_>());

            T_ operator()(const Date_& from, const Date_& to) const override;
            [[nodiscard]] int NX() const override;
            void ApplyDX(Vector_<>::const_iterator dx, double leverage) override;
            void Write(Archive::Store_& dst) const override;
            [[nodiscard]] DiscountPWC_* Clone(const String_& newName, const YCComponent_::substitutions_t& baseChanges) const override;

            [[nodiscard]] const Vector_<Date_>& KnotDates() const { return knotDates_; }
            [[nodiscard]] Vector_<T_> FRight() const { return fRightT_; }
        };
    } // namespace Tape

    DiscountCurve_* NewDiscountPWC(const String_& name,
                                   const String_& ccy,
                                   const PiecewiseConstant_& fwds,
                                   const Handle_<DiscountCurve_>& base = Handle_<DiscountCurve_>());
} // namespace Dal

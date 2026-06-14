//
// Created by dal-implementer on 2026/6/14.
//

#pragma once

#include <dal/curve/logdfscheme.hpp>
#include <dal/curve/discount.hpp>
#include <dal/curve/fittable.hpp>
#include <dal/curve/yccomponent.hpp>
#include <dal/math/interp/interp.hpp>
#include <dal/time/daybasis.hpp>

namespace Dal {
    // Concrete DF-node discount curve. Declared in the header so callers (the
    // calibration example, tests) can read back node dates / DFs via dynamic_cast.
    // Construction is factory-only via NewDiscountLogDF.
    class DiscountLogDF_ : public CurveWithBase_<DiscountCurve_>, public FittableCurve_ {
        Vector_<Date_> nodeDates_;
        DayBasis_ dayCount_;
        Vector_<> yf_;
        Vector_<> logDF_;
        LogDfScheme_ scheme_;
        Handle_<Interp1_> interp_;

        void RebuildInterp();
        [[nodiscard]] double LogDfAt(double yf) const;

    public:
        DiscountLogDF_(const String_& name,
                       const String_& ccy,
                       const Vector_<Date_>& nodeDates,
                       const Vector_<>& logDF,
                       const DayBasis_& dayCount,
                       LogDfScheme_ scheme,
                       const Handle_<DiscountCurve_>& base = Handle_<DiscountCurve_>());

        double operator()(const Date_& from, const Date_& to) const override;
        [[nodiscard]] int NX() const override;
        void ApplyDX(Vector_<>::const_iterator dx, double leverage) override;
        void Write(Archive::Store_& dst) const override;
        [[nodiscard]] DiscountLogDF_* Clone(const String_& new_name,
                                            const YCComponent_::substitutions_t& base_changes) const override;

        [[nodiscard]] const Vector_<Date_>& NodeDates() const { return nodeDates_; }
        [[nodiscard]] const Vector_<>& NodeLogDF() const { return logDF_; }
        [[nodiscard]] LogDfScheme_ Scheme() const { return scheme_; }
        [[nodiscard]] Vector_<> NodeDF() const;
    };

    DiscountCurve_* NewDiscountLogDF(const String_& name,
                                     const String_& ccy,
                                     const Vector_<Date_>& nodeDates,
                                     const Vector_<>& logDF,
                                     const DayBasis_& dayCount,
                                     LogDfScheme_ scheme,
                                     const Handle_<DiscountCurve_>& base = Handle_<DiscountCurve_>());
} // namespace Dal

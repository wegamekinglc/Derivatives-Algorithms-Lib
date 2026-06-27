//
// Created by dal-implementer on 2026/6/14.
//

#pragma once

#include <utility>
#include <dal/curve/logdfscheme.hpp>
#include <dal/curve/discount.hpp>
#include <dal/curve/fittable.hpp>
#include <dal/curve/yccomponent.hpp>
#include <dal/math/interp/interp.hpp>
#include <dal/time/daybasis.hpp>

namespace Dal {
    namespace Tape {
    template <class T_>
    class DiscountLogDF_ : public CurveWithBase_<DiscountCurve_<T_>, DiscountCurve_<double>>, public FittableCurve_ {
        Vector_<Date_> nodeDates_;
        DayBasis_ dayCount_;
        Vector_<> yf_;
        Vector_<T_> logDF_;
        LogDfScheme_ scheme_;
        Handle_<Interp1_> interp_;
        // LOG_CUBIC_NATURAL / MIXED second-derivative sensitivity matrix; see docs/methodology/log_discount_curve.md §"Basis Weights by Interpolation Scheme".
        Vector_<Vector_<>> fppCoef_;
        // For MIXED: index of the cutoff knot inside yf_ (set even when scheme is non-mixed, so
        // StorageBasisWeightsAt can dispatch without recomputing it). -1 when not applicable.
        int mixedCutoffIndex_ = -1;
        double mixedCutoffYf_ = 0.0;

        void RebuildInterp();
        void RebuildBasisAux();
        // T_-dispatched log-DF at year-fraction; see docs/methodology/log_discount_curve.md §"Basis Weights by Interpolation Scheme".
        [[nodiscard]] T_ LogDfAt(double yf) const;
        // LOG_CUBIC_NATURAL basis weights at yf on segment k; see docs/methodology/log_discount_curve.md §"Basis Weights by Interpolation Scheme".
        [[nodiscard]] Vector_<std::pair<int, double>> CubicBasisAt(int k, double yf) const;
        // LOG_CUBIC_NATURAL extrapolation weights for yf > yf_.back() (secant extension of the
        // last segment): only the last two free nodes carry nonzero weight.
        [[nodiscard]] Vector_<std::pair<int, double>> CubicExtrapWeights(double yf) const;
        // Knot-position basis weights at yf; see docs/methodology/log_discount_curve.md §"Basis Weights by Interpolation Scheme".
        [[nodiscard]] Vector_<std::pair<int, double>> StorageBasisWeightsAt(double yf) const;

    public:
        DiscountLogDF_(const String_& name,
                       const String_& ccy,
                       const Vector_<Date_>& nodeDates,
                       const Vector_<T_>& logDF,
                       const DayBasis_& dayCount,
                       LogDfScheme_ scheme,
                       const Handle_<Dal::DiscountCurve_>& base = Handle_<Dal::DiscountCurve_>());

        T_ operator()(const Date_& from, const Date_& to) const override;
        [[nodiscard]] int NX() const override;
        void ApplyDX(Vector_<>::const_iterator dx, double leverage) override;
        void Write(Archive::Store_& dst) const override;
        [[nodiscard]] DiscountLogDF_<T_>* Clone(const String_& newName,
                                                const YCComponent_::substitutions_t& baseChanges) const override;

        [[nodiscard]] const Vector_<Date_>& NodeDates() const { return nodeDates_; }
        [[nodiscard]] Vector_<> NodeLogDF() const;
        [[nodiscard]] const DayBasis_& DayCount() const { return dayCount_; }
        [[nodiscard]] LogDfScheme_ Scheme() const { return scheme_; }
        [[nodiscard]] Vector_<> NodeDF() const;
    };
    } // namespace Tape

    using DiscountLogDF_ = Tape::DiscountLogDF_<double>;

    DiscountCurve_* NewDiscountLogDF(const String_& name,
                                     const String_& ccy,
                                     const Vector_<Date_>& nodeDates,
                                     const Vector_<>& logDF,
                                     const DayBasis_& dayCount,
                                     LogDfScheme_ scheme,
                                     const Handle_<DiscountCurve_>& base = Handle_<DiscountCurve_>());
} // namespace Dal

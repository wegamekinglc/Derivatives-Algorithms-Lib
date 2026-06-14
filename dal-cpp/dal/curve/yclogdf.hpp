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
        // Spline second-derivative sensitivity matrix for LOG_CUBIC_NATURAL: fppCoef_[k][j] holds
        // d(fpp[k])/d(logDF[j]). For LOG_LINEAR / MIXED-head segments it is empty and unused. The
        // matrix is a function of the knot abscissae yf_ only, so it is computed once at construction.
        Vector_<Vector_<>> fppCoef_;
        // For MIXED: index of the cutoff knot inside yf_ (set even when scheme is non-mixed, so
        // InterpBasisWeights can dispatch without recomputing it). -1 when not applicable.
        int mixedCutoffIndex_ = -1;
        double mixedCutoffYf_ = 0.0;

        void RebuildInterp();
        void RebuildBasisAux();
        [[nodiscard]] double LogDfAt(double yf) const;
        // LOG_CUBIC_NATURAL basis weights at yf, on the segment [yf_[k], yf_[k+1]] with the
        // fppCoef_ second-derivative sensitivities. Output is the (storage node index, weight)
        // pairs whose weight is non-zero; caller remaps storage node k to solver column k-1.
        [[nodiscard]] Vector_<std::pair<int, double>> CubicBasisAt(int k, double yf) const;
        // LOG_CUBIC_NATURAL extrapolation weights for yf > yf_.back() (secant extension of the
        // last segment): only the last two free nodes carry nonzero weight.
        [[nodiscard]] Vector_<std::pair<int, double>> CubicExtrapWeights(double yf) const;
        // MIXED scheme per-segment dispatch: linear head up to mixedCutoffYf_, cubic tail beyond.
        [[nodiscard]] Vector_<std::pair<int, double>> InterpBasisWeightsMixed(int k, double yf) const;
        // Scheme-dispatched in-range basis weights on segment [yf_[k], yf_[k+1]].
        [[nodiscard]] Vector_<std::pair<int, double>> InterpBasisWeightsByScheme(int k, double yf) const;

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
        [[nodiscard]] const DayBasis_& DayCount() const { return dayCount_; }
        [[nodiscard]] LogDfScheme_ Scheme() const { return scheme_; }
        [[nodiscard]] Vector_<> NodeDF() const;

        // Interpolation basis weights at query year-fraction yf, indexed by FREE node (solver
        // column) index. Returns the (column, weight) pairs whose support covers yf. Empty for
        // yf < 0 (before the anchor; calibration forbids this) or when the curve cannot produce
        // a meaningful Jacobian column (e.g. the curve has degenerate knots).
        //
        // For each returned pair (jFree, w):
        //   dlogDF(yf) / dlogDF_(free node jFree) = w
        //   dDF(anchor, dateAt(yf)) / dx[jFree]   = DF(anchor, dateAt(yf)) * w
        //
        // Storage node 0 (anchor, pinned at logDF = 0) is structurally zero and NEVER appears
        // in the returned vector -- any (1-g) weight that would land on node 0 is dropped because
        // the anchor logDF is a constant and its derivative contribution is zero.
        [[nodiscard]] Vector_<std::pair<int, double>> InterpBasisWeights(double yf) const;
    };

    DiscountCurve_* NewDiscountLogDF(const String_& name,
                                     const String_& ccy,
                                     const Vector_<Date_>& nodeDates,
                                     const Vector_<>& logDF,
                                     const DayBasis_& dayCount,
                                     LogDfScheme_ scheme,
                                     const Handle_<DiscountCurve_>& base = Handle_<DiscountCurve_>());
} // namespace Dal

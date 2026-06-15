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
    // Phase A templatization: DiscountLogDF_ is an alias of DiscountLogDFT_<double>. The double
    // specialization stays byte-for-byte identical to the pre-Phase-A DiscountLogDF_ (same member
    // layout, same LogDfAt using interp_ directly, same operator() with std::exp). The Number_
    // specialization is constructed only by the AAD-tape Gradient override in calibration.cpp, and
    // its LogDfAt routes through the basis-weight machinery instead of interp_ (the tape records
    // the dependence on logDF_ that way). Declared in the header so calibration and tests can read
    // back node dates / DFs via dynamic_cast. Construction is factory-only via NewDiscountLogDF
    // (double) -- the Number_ factory lives in calibration.cpp and is unreachable from public code.
    template <class T_>
    class DiscountLogDFT_ : public CurveWithBase_<DiscountCurveT_<T_>, DiscountCurve_>, public FittableCurve_ {
        Vector_<Date_> nodeDates_;
        DayBasis_ dayCount_;
        Vector_<> yf_;
        Vector_<T_> logDF_;
        LogDfScheme_ scheme_;
        Handle_<Interp1_> interp_;
        // Spline second-derivative sensitivity matrix for LOG_CUBIC_NATURAL: fppCoef_[k][j] holds
        // d(fpp[k])/d(logDF[j]). For LOG_LINEAR / MIXED-head segments it is empty and unused. The
        // matrix is a function of the knot abscissae yf_ only, so it is computed once at construction
        // and is identical for any T_.
        Vector_<Vector_<>> fppCoef_;
        // For MIXED: index of the cutoff knot inside yf_ (set even when scheme is non-mixed, so
        // StorageBasisWeightsAt can dispatch without recomputing it). -1 when not applicable.
        int mixedCutoffIndex_ = -1;
        double mixedCutoffYf_ = 0.0;

        void RebuildInterp();
        void RebuildBasisAux();
        // double path: calls interp_(yf) directly (byte-identical to pre-Phase-A).
        // Number_ path: accumulates the basis weights against logDF_ so the tape records the
        // dependence on the free-node logDF values (see .claude/designs/aad-analytic-jacobian-phase-a-plan.md
        // §5.1). Dispatch is compile-time via if constexpr.
        [[nodiscard]] T_ LogDfAt(double yf) const;
        // LOG_CUBIC_NATURAL basis weights at yf, on the segment [yf_[k], yf_[k+1]] with the
        // fppCoef_ second-derivative sensitivities. Output is the (storage node index, weight)
        // pairs whose weight is non-zero; caller remaps storage node k to solver column k-1.
        [[nodiscard]] Vector_<std::pair<int, double>> CubicBasisAt(int k, double yf) const;
        // LOG_CUBIC_NATURAL extrapolation weights for yf > yf_.back() (secant extension of the
        // last segment): only the last two free nodes carry nonzero weight.
        [[nodiscard]] Vector_<std::pair<int, double>> CubicExtrapWeights(double yf) const;
        // Knot-position-only (no logDF dependence) basis weights at query year-fraction yf, as
        // (storage node index, weight) pairs. Used by the Number_-typed LogDfAt forward path to
        // accumulate the basis weights against logDF_ on the tape.
        [[nodiscard]] Vector_<std::pair<int, double>> StorageBasisWeightsAt(double yf) const;

    public:
        DiscountLogDFT_(const String_& name,
                        const String_& ccy,
                        const Vector_<Date_>& nodeDates,
                        const Vector_<T_>& logDF,
                        const DayBasis_& dayCount,
                        LogDfScheme_ scheme,
                        const Handle_<DiscountCurve_>& base = Handle_<DiscountCurve_>());

        T_ operator()(const Date_& from, const Date_& to) const override;
        [[nodiscard]] int NX() const override;
        void ApplyDX(Vector_<>::const_iterator dx, double leverage) override;
        void Write(Archive::Store_& dst) const override;
        [[nodiscard]] DiscountLogDFT_<T_>* Clone(const String_& new_name,
                                                 const YCComponent_::substitutions_t& base_changes) const override;

        [[nodiscard]] const Vector_<Date_>& NodeDates() const { return nodeDates_; }
        [[nodiscard]] Vector_<> NodeLogDF() const;
        [[nodiscard]] const DayBasis_& DayCount() const { return dayCount_; }
        [[nodiscard]] LogDfScheme_ Scheme() const { return scheme_; }
        [[nodiscard]] Vector_<> NodeDF() const;
    };

    using DiscountLogDF_ = DiscountLogDFT_<double>;

    DiscountCurve_* NewDiscountLogDF(const String_& name,
                                     const String_& ccy,
                                     const Vector_<Date_>& nodeDates,
                                     const Vector_<>& logDF,
                                     const DayBasis_& dayCount,
                                     LogDfScheme_ scheme,
                                     const Handle_<DiscountCurve_>& base = Handle_<DiscountCurve_>());
} // namespace Dal

//
// Created by dal-implementer on 2026/6/14.
//

#include <algorithm>
#include <cmath>
#include <type_traits>
#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/curve/yclogdf.hpp>
#include <dal/curve/fittable.hpp>
#include <dal/curve/yccomponent.hpp>
#include <dal/curve/discount.hpp>
#include <dal/math/aad/aad.hpp>
#include <dal/math/interp/interp.hpp>
#include <dal/math/interp/interplinear.hpp>
#include <dal/math/interp/interpcubic.hpp>
#include <dal/math/interp/interpmixed.hpp>
#include <dal/math/vectors.hpp>
#include <dal/storage/archive.hpp>
#include <dal/time/date.hpp>
#include <dal/time/daybasis.hpp>
#include <dal/utilities/algorithms.hpp>
#include <dal/utilities/exceptions.hpp>

/*IF--------------------------------------------------------------------------
storable DiscountLogDF
   Discount curve on explicit (node date, log DF) pairs with a pluggable interpolation rule
version 2
manual
&members
name is ?string
ccy is ?string
nodeDates is date[]
logDF is number[]
dayCount is string
scheme is string
base is ?handle DiscountCurve
-IF-------------------------------------------------------------------------*/

namespace Dal {

#include <dal/auto/MG_DiscountLogDF_v2_Write.inc>

    namespace {
        // Build the log-DF interpolator from knot year-fractions and log(DF) values.
        // Centralised here so calibration, ApplyDX, and deserialisation all share one definition;
        // each scheme is fully determined by (yf, logDF) -- boundary conditions are fixed (natural
        // cubic) and the mixed cutoff is the (nKnots-4)-th knot, matching calibration.cpp.
        Handle_<Interp1_> BuildLogDfInterpFromYf(const String_& name,
                                                 LogDfScheme_ scheme,
                                                 const Vector_<>& yf,
                                                 const Vector_<>& logDF) {
            switch (scheme.Switch()) {
            case LogDfScheme_::Value_::LOG_LINEAR:
                // linear on log(DF) is log-linear on DF
                return Handle_<Interp1_>(Interp::NewLinear(name + "_loglin", yf, logDF));
            case LogDfScheme_::Value_::LOG_CUBIC_NATURAL: {
                // natural cubic on log(DF) (Boundary_(2, 0.0)) -- the primary choice per design §3.2 D3.
                const Interp::Boundary_ natural(2, 0.0);
                return Handle_<Interp1_>(Interp::NewCubic(name + "_logcub", yf, logDF, natural, natural));
            }
            case LogDfScheme_::Value_::MIXED: {
                // cutoff at the (nKnots-4)-th knot so the cubic tail has >= 3 knots beyond it.
                const int cutoffIndex = std::max(1, static_cast<int>(yf.size()) - 5);
                MixedSchemeSpec_ spec;
                spec.cutoffYf_ = yf[cutoffIndex];
                return Handle_<Interp1_>(NewMixedLogDF(name + "_mixed", yf, logDF, spec));
            }
            default:
                THROW(String_("Unknown LOG_DISCOUNT scheme: ") + scheme.String());
            }
        }

        // Solve a symmetric tridiagonal system M z = b in place where M is specified by its
        // subdiagonal `a` (size n-1), diagonal `d` (size n), and superdiagonal `c` (size n-1).
        // Returns z (size n). Standard Thomas algorithm; assumes M is diagonally dominant
        // (true for the natural-cubic spline matrix).
        Vector_<> SolveTriDiagonal(const Vector_<>& a, const Vector_<>& d, const Vector_<>& c, const Vector_<>& b) {
            const int n = static_cast<int>(d.size());
            REQUIRE(static_cast<int>(a.size()) == n - 1 && static_cast<int>(c.size()) == n - 1 && static_cast<int>(b.size()) == n,
                    "SolveTriDiagonal: inconsistent sub/diag/super/rhs sizes");
            Vector_<> cp(n - 1);
            Vector_<> dp(n);
            dp[0] = d[0];
            cp[0] = c[0] / dp[0];
            for (int i = 1; i < n; ++i) {
                if (i < n - 1) {
                    dp[i] = d[i] - a[i - 1] * cp[i - 1];
                    cp[i] = c[i] / dp[i];
                } else {
                    dp[i] = d[i] - a[i - 1] * cp[i - 1];
                }
            }
            Vector_<> x(n);
            // Forward substitution: y[i] = (b[i] - a[i-1]*y[i-1]) / dp[i]
            Vector_<> y(n);
            y[0] = b[0] / dp[0];
            for (int i = 1; i < n; ++i)
                y[i] = (b[i] - a[i - 1] * y[i - 1]) / dp[i];
            // Back-substitution: x[n-1] = y[n-1]; x[i] = y[i] - cp[i]*x[i+1]
            x[n - 1] = y[n - 1];
            for (int i = n - 2; i >= 0; --i)
                x[i] = y[i] - cp[i] * x[i + 1];
            return x;
        }

        // db[i]/d(f[j]) for the natural-cubic rhs at interior node i, source knot j, given the
        // segment lengths h. The four Kronecker-delta terms of the second-difference collapse to
        // this dispatch; extracted so the per-source loop stays linear in branches.
        double NaturalCubicRhsEntry(int j, int i, const Vector_<>& h) {
            double val = 0.0;
            if (j == i + 1)
                val += 1.0 / h[i + 1];
            if (j == i) {
                val -= 1.0 / h[i + 1];
                val -= 1.0 / h[i];
            }
            if (j == i - 1)
                val += 1.0 / h[i];
            return 6.0 * val;
        }

        // Build the sub/diag/sup rows of the (n-2) x (n-2) interior natural-cubic system in the
        // segment-length vector h (size n, with h[0] unused).
        void NaturalCubicInteriorMatrix(const Vector_<>& h, int m, Vector_<>* sub, Vector_<>* diag, Vector_<>* sup) {
            for (int r = 0; r < m; ++r) {
                const int i = r + 1; // interior node index
                (*diag)[r] = 2.0 * (h[i] + h[i + 1]);
                if (r > 0)
                    (*sub)[r - 1] = h[i];
                if (r < m - 1)
                    (*sup)[r] = h[i + 1];
            }
        }

        // Natural-cubic-spline second-derivative sensitivity matrix:
        //   fppCoef[k][j] = d(fpp[k]) / d(f[j])
        // where fpp[0] = fpp[n-1] = 0 (natural BC) and fpp[1..n-2] are obtained by solving
        //   h[i-1]*fpp[i-1] + 2*(h[i-1]+h[i])*fpp[i] + h[i]*fpp[i+1] = b[i]   for i = 1..n-2
        // with h[k] = x[k] - x[k-1] and b[i] = 6*((f[i+1]-f[i])/h[i+1] - (f[i]-f[i-1])/h[i]).
        // We solve n column systems (one per nonzero source f[j]).
        Vector_<Vector_<>> BuildNaturalCubicFppCoef(const Vector_<>& x) {
            const int n = static_cast<int>(x.size());
            Vector_<Vector_<>> coef(n, Vector_<>(n, 0.0));
            if (n < 3)
                return coef; // need >= 3 knots for an interior system
            Vector_<> h(n);
            for (int i = 1; i < n; ++i)
                h[i] = x[i] - x[i - 1];
            const int m = n - 2;
            Vector_<> sub(m - 1);
            Vector_<> diag(m);
            Vector_<> sup(m - 1);
            NaturalCubicInteriorMatrix(h, m, &sub, &diag, &sup);
            for (int j = 0; j < n; ++j) {
                Vector_<> rhs(m, 0.0);
                for (int r = 0; r < m; ++r)
                    rhs[r] = NaturalCubicRhsEntry(j, r + 1, h);
                const Vector_<> sol = SolveTriDiagonal(sub, diag, sup, rhs);
                coef[0][j] = 0.0;
                coef[n - 1][j] = 0.0;
                for (int i = 1; i <= n - 2; ++i)
                    coef[i][j] = sol[i - 1];
            }
            return coef;
        }

        // Locate the largest index k such that yf_[k] <= yf; return 0 if yf < yf_[1]. Identical
        // semantics to InterpLinearImplX's lower-bound branch used by Interp1Linear_::operator().
        int LowerBoundSegment(const Vector_<>& yf, double yfQuery) {
            const int n = static_cast<int>(yf.size());
            // Anchor (yf[0]) is 0; we are interested in the segment [k, k+1].
            int k = 0;
            for (int i = 1; i < n - 1; ++i) {
                if (yf[i] <= yfQuery)
                    k = i;
                else
                    break;
            }
            // Clamp: yfQuery may equal yf[n-1] exactly (last knot) -- segment is k=n-2.
            if (k > n - 2)
                k = n - 2;
            return k;
        }

        // Linear-basis storage-node weights on segment [yf[k], yf[k+1]]: (1-g) at storage k, g at
        // storage k+1, with g = (yfQuery - yf[k])/h. Shared by LOG_LINEAR and the MIXED head.
        Vector_<std::pair<int, double>> LinearSegmentWeights(const Vector_<>& yf, int k, double yfQuery) {
            const double h = yf[k + 1] - yf[k];
            const double g = (yfQuery - yf[k]) / h;
            Vector_<std::pair<int, double>> storageWeights;
            storageWeights.emplace_back(k, 1.0 - g);
            storageWeights.emplace_back(k + 1, g);
            return storageWeights;
        }
    } // namespace

    // The ctor body is parameterised on T_ but the runtime validation (REQUIRE on logDF[0] == 0,
    // IsMonotonic) only makes sense for double -- for Number_ the value extraction would record
    // spurious tape nodes. The geometric invariants (nodeDates, dayCount, yf, scheme, interp,
    // fppCoef) are T_-independent and built unconditionally. The Number_ factory in calibration.cpp
    // pins logDF[0] = 0 itself before construction, so the runtime check is redundant there.
    template <class T_>
    DiscountLogDFT_<T_>::DiscountLogDFT_(const String_& name,
                                          const String_& ccy,
                                          const Vector_<Date_>& nodeDates,
                                          const Vector_<T_>& logDF,
                                          const DayBasis_& dayCount,
                                          LogDfScheme_ scheme,
                                          const Handle_<DiscountCurve_>& base)
        : CurveWithBase_<DiscountCurveT_<T_>, DiscountCurve_>(name, ccy, base),
          nodeDates_(nodeDates),
          dayCount_(dayCount),
          yf_(nodeDates.size()),
          logDF_(logDF),
          scheme_(scheme) {
        REQUIRE(nodeDates_.size() == logDF_.size(),
                "log-DF discount curve: nodeDates and logDF must have equal length");
        REQUIRE(nodeDates_.size() >= 2,
                "log-DF discount curve: need at least 2 nodes (anchor + one free)");
        REQUIRE(IsMonotonic(nodeDates_), "log-DF discount curve: node dates must be strictly increasing");
        if constexpr (std::is_same_v<T_, double>) {
            REQUIRE(!logDF_.empty() && std::abs(logDF_[0]) < 1e-15,
                    "log-DF discount curve: anchor node (index 0) must be pinned at logDF = 0");
            for (int i = 0; i < static_cast<int>(nodeDates_.size()); ++i)
                REQUIRE(std::isfinite(logDF_[i]),
                        String_("log-DF discount curve: logDF[") + String::FromInt(i) + "] is not finite");
        }
        const Date_& anchor = nodeDates_.front();
        for (int i = 0; i < static_cast<int>(nodeDates_.size()); ++i)
            yf_[i] = dayCount_(anchor, nodeDates_[i], nullptr);
        REQUIRE(IsMonotonic(yf_), "log-DF discount curve: year-fractions must be strictly increasing");
        RebuildInterp();
    }

    template <class T_>
    void DiscountLogDFT_<T_>::RebuildInterp() {
        // interp_ is always double-valued and depends only on yf_ and the (double-extracted) logDF
        // values -- it is the knot-position-driven interpolator the double path reads through.
        // For Number_ we cannot build a double interp_ from Number_ values; we extract via Value()
        // so the interpolator exists for any code that defensively calls it, but the Number_ LogDfAt
        // path does NOT consult interp_ (it accumulates basis weights against logDF_ directly).
        Vector_<> logDFDouble(yf_.size());
        for (int i = 0; i < static_cast<int>(logDF_.size()); ++i)
            logDFDouble[i] = static_cast<double>(logDF_[i]);
        interp_ = BuildLogDfInterpFromYf(this->Name(), scheme_, yf_, logDFDouble);
        RebuildBasisAux();
    }

    template <class T_>
    void DiscountLogDFT_<T_>::RebuildBasisAux() {
        fppCoef_.clear();
        mixedCutoffIndex_ = -1;
        mixedCutoffYf_ = 0.0;
        if (scheme_ == LogDfScheme_::Value_::LOG_CUBIC_NATURAL) {
            fppCoef_ = BuildNaturalCubicFppCoef(yf_);
        } else if (scheme_ == LogDfScheme_::Value_::MIXED) {
            // The cutoff is the (nKnots-4)-th knot (see BuildLogDfInterpFromYf). The cubic tail
            // needs fppCoef_ over the tail subarray; we store the full-curve fppCoef_ as if the
            // cubic ran over the whole array -- BUT the actual cubic is built on the tail only.
            // To stay correct, we compute fppCoef_ on the cubic TAIL subarray (from cutoffIndex
            // onward) and store offsets so CubicBasisAt can map back to storage nodes.
            const int cutoffIndex = std::max(1, static_cast<int>(yf_.size()) - 5);
            mixedCutoffIndex_ = cutoffIndex;
            mixedCutoffYf_ = yf_[cutoffIndex];
            // The cubic tail is yf_[cutoffIndex..end]; build fppCoef_ on this subarray.
            const int tailLen = static_cast<int>(yf_.size()) - cutoffIndex;
            Vector_<> tailYf(tailLen);
            for (int i = 0; i < tailLen; ++i)
                tailYf[i] = yf_[cutoffIndex + i];
            Vector_<Vector_<>> tailCoef = BuildNaturalCubicFppCoef(tailYf);
            // Expand tailCoef back to full-curve storage indices.
            fppCoef_ = Vector_<Vector_<>>(yf_.size(), Vector_<>(yf_.size(), 0.0));
            for (int k = 0; k < tailLen; ++k)
                for (int j = 0; j < tailLen; ++j)
                    fppCoef_[cutoffIndex + k][cutoffIndex + j] = tailCoef[k][j];
        }
    }

    template <class T_>
    Vector_<std::pair<int, double>> DiscountLogDFT_<T_>::CubicBasisAt(int k, double yf) const {
        // On segment [yf_[k], yf_[k+1]] with h = yf_[k+1] - yf_[k]:
        //   S(yf) = a*f[k] + b*f[k+1] - a*b*(h^2/6) * ((1+a)*fpp[k] + (1+b)*fpp[k+1])
        // where a = 1-b, b = (yf - yf_[k])/h. The basis weight at storage node j is
        //   b_j(yf) = a*delta(j,k) + b*delta(j,k+1)
        //             - a*b*(h^2/6) * ((1+a)*fppCoef_[k][j] + (1+b)*fppCoef_[k+1][j])
        // fppCoef_ is the storage-indexed matrix (empty entries are zero).
        const double h = yf_[k + 1] - yf_[k];
        const double b = (yf - yf_[k]) / h;
        const double a = 1.0 - b;
        const double factor = a * b * Square(h) / 6.0;
        const int n = static_cast<int>(yf_.size());
        Vector_<std::pair<int, double>> retval;
        retval.reserve(n);
        for (int j = 0; j < n; ++j) {
            const double term = (1.0 + a) * fppCoef_[k][j] + (1.0 + b) * fppCoef_[k + 1][j];
            double w = 0.0;
            if (j == k)
                w += a;
            if (j == k + 1)
                w += b;
            w -= factor * term;
            retval.emplace_back(j, w);
        }
        return retval;
    }

    template <class T_>
    Vector_<std::pair<int, double>> DiscountLogDFT_<T_>::CubicExtrapWeights(double yf) const {
        // LogDfAt extrapolates past yf_.back() by the last segment's log-DF secant slope:
        //   logDF(yf) = logDF[n-1] + slopeLast * (yf - yf_[n-1])
        // where slopeLast = (logDF[n-1] - logDF[n-2]) / (yf_[n-1] - yf_[n-2]).
        // So dlogDF(yf)/dlogDF[j] is 1 if j==n-1, slopeLast/slopeLast==1 if j==n-2 (with weight
        // -(yf-yf_[n-1])/h), and 0 elsewhere. For LOG_CUBIC_NATURAL the same secant-extension is
        // used (see LogDfAt docstring) -- it does NOT use the cubic derivative at the boundary.
        const int n = static_cast<int>(yf_.size());
        const double dyfLast = yf_[n - 1] - yf_[n - 2];
        Vector_<std::pair<int, double>> retval;
        if (dyfLast <= 0.0) {
            retval.emplace_back(n - 1, 1.0);
            return retval;
        }
        // weight at storage node n-2: -(yf - yf_[n-1]) / dyfLast
        // weight at storage node n-1: 1 + (yf - yf_[n-1]) / dyfLast
        const double excess = (yf - yf_[n - 1]) / dyfLast;
        retval.emplace_back(n - 2, -excess);
        retval.emplace_back(n - 1, 1.0 + excess);
        return retval;
    }

    template <class T_>
    Vector_<std::pair<int, double>> DiscountLogDFT_<T_>::StorageBasisWeightsAt(double yf) const {
        // Returns STORAGE-node weights (no anchor drop, no solver-column remap). The Number_-typed
        // LogDfAt accumulates these against logDF_ to produce a tape-registered logDF(yf). The
        // weights are functions of knot positions only, so they are identical for any T_.
        if (yf < 0.0 || yf_.size() < 2)
            return {};
        if (yf > yf_.back())
            return CubicExtrapWeights(yf);
        const int k = LowerBoundSegment(yf_, yf);
        switch (scheme_.Switch()) {
        case LogDfScheme_::Value_::LOG_LINEAR:
            return LinearSegmentWeights(yf_, k, yf);
        case LogDfScheme_::Value_::LOG_CUBIC_NATURAL:
            return CubicBasisAt(k, yf);
        case LogDfScheme_::Value_::MIXED:
            return (yf <= mixedCutoffYf_) ? LinearSegmentWeights(yf_, k, yf) : CubicBasisAt(k, yf);
        default:
            THROW(String_("DiscountLogDFT_::StorageBasisWeightsAt: unknown scheme: ") + scheme_.String());
        }
    }

    template <class T_>
    T_ DiscountLogDFT_<T_>::operator()(const Date_& from, const Date_& to) const {
        const double yfFrom = dayCount_(nodeDates_.front(), from, nullptr);
        const double yfTo = dayCount_(nodeDates_.front(), to, nullptr);
        if constexpr (std::is_same_v<T_, double>) {
            // Byte-identical to pre-Phase-A DiscountLogDF_::operator().
            const double logDfFrom = LogDfAt(yfFrom);
            const double logDfTo = LogDfAt(yfTo);
            return std::exp(logDfTo - logDfFrom) * (this->base_ ? (*this->base_)(from, to) : 1.0);
        } else {
            // Number_ path: tape records dependence of logDf* on each free-node logDF_ value.
            // The base curve is double and is treated as a constant multiplier.
            const T_ logDfFrom = LogDfAt(yfFrom);
            const T_ logDfTo = LogDfAt(yfTo);
            const double baseFactor = this->base_ ? (*this->base_)(from, to) : 1.0;
            return Dal::AAD::exp(logDfTo - logDfFrom) * baseFactor;
        }
    }

    // Evaluate log(DF) at year-fraction yf.
    //
    // For T_=double (byte-identical to pre-Phase-A): the path uses (*interp_)(yf) in-range, and
    // the final-segment secant slope out of range. The interp_ rebuild at construction stores the
    // double-extracted logDF values so this matches the pre-Phase-A behaviour exactly.
    //
    // For T_=Number_: the path accumulates StorageBasisWeightsAt(yf) against logDF_, so the tape
    // records the dependence of logDF(yf) on each free-node logDF_ value. Out-of-range the same
    // secant weights apply. This is the same machinery as the CP1 chain-rule path (§5.1 of the
    // Phase A design), re-used as the forward evaluation here.
    template <class T_>
    T_ DiscountLogDFT_<T_>::LogDfAt(double yf) const {
        if constexpr (std::is_same_v<T_, double>) {
            if (yf <= yf_.back())
                return (*interp_)(yf);
            const int n = static_cast<int>(yf_.size());
            const double dyfLast = yf_[n - 1] - yf_[n - 2];
            if (dyfLast <= 0.0)
                return logDF_.back();
            const double slopeLast = (logDF_[n - 1] - logDF_[n - 2]) / dyfLast;
            return logDF_[n - 1] + slopeLast * (yf - yf_[n - 1]);
        } else {
            // Anchor (storage node 0) is pinned at logDF=0; dropping it from the weighted sum is
            // value-preserving (0 * w_0 = 0) AND produces the correct Jacobian: d(logDF(yf)) /
            // d(logDF_[0]) must be zero because the anchor is a constant. Including it would
            // register a tape dependence on logDF_[0] that the calibration overrides anyway, but
            // it is cleaner to omit it here so the column map (solver col = storage node - 1)
            // holds without exception.
            T_ acc(static_cast<double>(0.0));
            for (const auto& [storageNode, w] : StorageBasisWeightsAt(yf)) {
                if (storageNode == 0)
                    continue;
                acc += static_cast<double>(w) * logDF_[storageNode];
            }
            return acc;
        }
    }

    // Free-node count: the anchor (node 0, logDF pinned at 0) is excluded from the unknown
    // vector. For a square 13-instrument / 13-free-node LOG_DISCOUNT solve this is what makes
    // the parameter dimension match the instrument count.
    template <class T_>
    int DiscountLogDFT_<T_>::NX() const { return static_cast<int>(logDF_.size()) - 1; }

    // Bump only the FREE nodes logDF_[1..] by dx[i] * leverage; keep logDF_[0] pinned at 0 and
    // rebuild interp_ so operator() reflects the bumped curve. A stale interp_ after a bump would
    // silently desync the curve from its node values -- the risk-engine landmine the reviewer flagged.
    // The Number_ factory in calibration.cpp never calls ApplyDX (the AAD path constructs the
    // curve directly with the tape-registered logDF vector), so the body below is only ever
    // exercised for T_=double; for T_=Number_ it would compile but is unreachable.
    template <class T_>
    void DiscountLogDFT_<T_>::ApplyDX(Vector_<>::const_iterator dx, double leverage) {
        for (int i = 1; i < static_cast<int>(logDF_.size()); ++i)
            logDF_[i] += leverage * *dx++;
        RebuildInterp();
    }

    template <class T_>
    void DiscountLogDFT_<T_>::Write(Archive::Store_& dst) const {
        // Serialisation is double-only; the Number_ path never persists. Extract via static_cast.
        Vector_<> logDFDouble(logDF_.size());
        for (int i = 0; i < static_cast<int>(logDF_.size()); ++i)
            logDFDouble[i] = static_cast<double>(logDF_[i]);
        DiscountLogDF_v2::XWrite(dst,
                                 this->Name(),
                                 this->ccy_.String(),
                                 nodeDates_,
                                 logDFDouble,
                                 dayCount_.String(),
                                 scheme_.String(),
                                 this->base_);
    }

    template <class T_>
    DiscountLogDFT_<T_>* DiscountLogDFT_<T_>::Clone(const String_& new_name,
                                                    const YCComponent_::substitutions_t& base_changes) const {
        return new DiscountLogDFT_<T_>(new_name, this->ccy_.String(), nodeDates_, logDF_, dayCount_, scheme_, this->NewBase(base_changes));
    }

    template <class T_>
    Vector_<> DiscountLogDFT_<T_>::NodeDF() const {
        Vector_<> retval(logDF_.size());
        for (int i = 0; i < static_cast<int>(logDF_.size()); ++i)
            retval[i] = std::exp(static_cast<double>(logDF_[i]));
        return retval;
    }

    template <class T_>
    Vector_<> DiscountLogDFT_<T_>::NodeLogDF() const {
        // Returns the double view of logDF_ -- for T_=double this is logDF_ itself (byte-identical
        // to pre-Phase-A); for T_=Number_ it is the primal values. The Number_ path is never
        // introspected this way at runtime (the AAD override works off the tape, not the curve),
        // but the method exists so dynamic_cast<DiscountLogDFT_<Number_>*> does not yield a type
        // missing the public surface a caller might assume.
        Vector_<> retval(logDF_.size());
        for (int i = 0; i < static_cast<int>(logDF_.size()); ++i)
            retval[i] = static_cast<double>(logDF_[i]);
        return retval;
    }

    // Explicit instantiations. DiscountLogDFT_<double> is the hot path (F loop, bumped fallback,
    // serialisation). DiscountLogDFT_<Dal::AAD::Number_> is instantiated only under the native
    // backend; it pulls in the AAD-aware branches via if constexpr and is linked into the Phase A
    // Gradient override in calibration.cpp. Under external backends Number_ does not exist, so the
    // Number_ instantiation is gated out and Phase A compiles away.
    template class DiscountLogDFT_<double>;
    template class DiscountLogDFT_<Dal::AAD::Number_>;

    DiscountCurve_* NewDiscountLogDF(const String_& name,
                                     const String_& ccy,
                                     const Vector_<Date_>& nodeDates,
                                     const Vector_<>& logDF,
                                     const DayBasis_& dayCount,
                                     LogDfScheme_ scheme,
                                     const Handle_<DiscountCurve_>& base) {
        return new DiscountLogDFT_<double>(name, ccy, nodeDates, logDF, dayCount, scheme, base);
    }

#include <dal/auto/MG_DiscountLogDF_v1_Read.inc>
#include <dal/auto/MG_DiscountLogDF_v2_Read.inc>

    Storable_* DiscountLogDF_v1::Reader_::Build() const {
        // Legacy v1 stored the built Interp1_ handle directly and did NOT persist the LogDfScheme_.
        // The scheme cannot be recovered from the deserialised handle: every Interp1_ subtype reports
        // the same Storable_::type_ ("Interp1", fixed by the Interp1_ base constructor -- it is not
        // "Cubic1"/"Interp1Linear"), and the concrete interpolators live in anonymous namespaces so
        // RTTI cannot tell them apart either. v1 therefore always reconstructs as LOG_LINEAR, the only
        // scheme honestly rebuildable from (nodeDates, logDF) alone. This is exactly why v2 -- the
        // canonical format -- carries the scheme by name; callers needing cubic/mixed must use v2.
        return new DiscountLogDFT_<double>(
            name_, ccy_, nodeDates_, logDF_, DayBasis_(dayCount_), LogDfScheme_::Value_::LOG_LINEAR, base_);
    }

    Storable_* DiscountLogDF_v2::Reader_::Build() const {
        // v2 stores the scheme by name and rebuilds the interpolator from (nodeDates, logDF).
        return new DiscountLogDFT_<double>(name_, ccy_, nodeDates_, logDF_, DayBasis_(dayCount_), LogDfScheme_(scheme_), base_);
    }
} // namespace Dal

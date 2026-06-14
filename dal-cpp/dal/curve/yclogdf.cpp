//
// Created by dal-implementer on 2026/6/14.
//

#include <algorithm>
#include <cmath>
#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/curve/yclogdf.hpp>
#include <dal/curve/fittable.hpp>
#include <dal/curve/yccomponent.hpp>
#include <dal/curve/discount.hpp>
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
    } // namespace

    DiscountLogDF_::DiscountLogDF_(const String_& name,
                                   const String_& ccy,
                                   const Vector_<Date_>& nodeDates,
                                   const Vector_<>& logDF,
                                   const DayBasis_& dayCount,
                                   LogDfScheme_ scheme,
                                   const Handle_<DiscountCurve_>& base)
        : CurveWithBase_<DiscountCurve_>(name, ccy, base),
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
        REQUIRE(!logDF_.empty() && std::abs(logDF_[0]) < 1e-15,
                "log-DF discount curve: anchor node (index 0) must be pinned at logDF = 0");
        const Date_& anchor = nodeDates_.front();
        for (int i = 0; i < static_cast<int>(nodeDates_.size()); ++i) {
            yf_[i] = dayCount_(anchor, nodeDates_[i], nullptr);
            REQUIRE(std::isfinite(logDF_[i]),
                    String_("log-DF discount curve: logDF[") + String::FromInt(i) + "] is not finite");
        }
        REQUIRE(IsMonotonic(yf_), "log-DF discount curve: year-fractions must be strictly increasing");
        RebuildInterp();
    }

    void DiscountLogDF_::RebuildInterp() {
        interp_ = BuildLogDfInterpFromYf(Name(), scheme_, yf_, logDF_);
        RebuildBasisAux();
    }

    namespace {
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

        // Natural-cubic-spline second-derivative sensitivity matrix:
        //   fppCoef[k][j] = d(fpp[k]) / d(f[j])
        // where fpp[0] = fpp[n-1] = 0 (natural BC) and fpp[1..n-2] are obtained by solving
        //   h[i-1]*fpp[i-1] + 2*(h[i-1]+h[i])*fpp[i] + h[i]*fpp[i+1] = b[i]   for i = 1..n-2
        // with h[k] = x[k] - x[k-1] and b[i] = 6*((f[i+1]-f[i])/h[i+1] - (f[i]-f[i-1])/h[i]).
        // We solve nRight column systems (one per nonzero source f[j]).
        Vector_<Vector_<>> BuildNaturalCubicFppCoef(const Vector_<>& x) {
            const int n = static_cast<int>(x.size());
            Vector_<Vector_<>> coef(n, Vector_<>(n, 0.0));
            if (n < 3)
                return coef; // need >= 3 knots for an interior system
            // Segment lengths h[k] = x[k] - x[k-1], k = 1..n-1.
            Vector_<> h(n);
            for (int i = 1; i < n; ++i)
                h[i] = x[i] - x[i - 1];
            // Interior system is (n-2) x (n-2), indexing rows/cols by interior node 1..n-2.
            const int m = n - 2;
            Vector_<> sub(m - 1);
            Vector_<> diag(m);
            Vector_<> sup(m - 1);
            for (int r = 0; r < m; ++r) {
                const int i = r + 1; // interior node index
                diag[r] = 2.0 * (h[i] + h[i + 1]);
                if (r > 0)
                    sub[r - 1] = h[i];
                if (r < m - 1)
                    sup[r] = h[i + 1];
            }
            // For each source knot j, build the rhs b (size m) and solve.
            for (int j = 0; j < n; ++j) {
                Vector_<> rhs(m, 0.0);
                for (int r = 0; r < m; ++r) {
                    const int i = r + 1; // interior node
                    // db[i]/d(f[j]) = 6 * ((delta(j,i+1)-delta(j,i))/h[i+1] - (delta(j,i)-delta(j,i-1))/h[i])
                    double val = 0.0;
                    if (j == i + 1)
                        val += 1.0 / h[i + 1];
                    if (j == i)
                        val -= 1.0 / h[i + 1];
                    if (j == i)
                        val -= 1.0 / h[i];
                    if (j == i - 1)
                        val += 1.0 / h[i];
                    rhs[r] = 6.0 * val;
                }
                const Vector_<> sol = SolveTriDiagonal(sub, diag, sup, rhs);
                // fpp[0] = fpp[n-1] = 0; fpp[i] = sol[i-1] for i = 1..n-2.
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
    } // namespace

    void DiscountLogDF_::RebuildBasisAux() {
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

    Vector_<std::pair<int, double>> DiscountLogDF_::CubicBasisAt(int k, double yf) const {
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

    Vector_<std::pair<int, double>> DiscountLogDF_::CubicExtrapWeights(double yf) const {
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


    double DiscountLogDF_::operator()(const Date_& from, const Date_& to) const {
        const double yfFrom = dayCount_(nodeDates_.front(), from, nullptr);
        const double yfTo = dayCount_(nodeDates_.front(), to, nullptr);
        const double logDfFrom = LogDfAt(yfFrom);
        const double logDfTo = LogDfAt(yfTo);
        return std::exp(logDfTo - logDfFrom) * (base_ ? (*base_)(from, to) : 1.0);
    }

    // Evaluate log(DF) at year-fraction yf, extrapolating past the last node by extending the final
    // segment's log-DF secant slope. For yf beyond yf_.back() each Interp1_ subtype clamps to its last
    // knot value (flat DF, i.e. zero forward), which is not the desired curve behaviour.
    //
    // Note the distinction by scheme:
    //   - log-linear: each segment of log(DF) is linear, so the secant slope over [yf[n-2], yf[n-1]]
    //     IS the (constant) instantaneous forward of that segment. Extrapolation therefore holds the
    //     last segment's forward flat -- true flat-forward continuation.
    //   - log-cubic / mixed: the last segment of log(DF) is a cubic, so the secant slope is the
    //     AVERAGE forward over the segment, not the instantaneous forward at yf[n-1] (which would be
    //     the cubic's derivative there). Extending the secant is a deliberate, simple approximation
    //     that keeps the curve C0 with the in-range cubic and avoids recomputing a derivative per
    //     query; it does NOT reproduce the cubic's instantaneous forward. v2 readers carry the scheme
    //     explicitly, so callers needing the true cubic-tail forward should switch schemes.
    //
    // The left edge (yf < yf_.front() == 0) only occurs for `from` dates before the anchor, which the
    // calibration forbids; we leave the interpolator's native (flat) behaviour there.
    double DiscountLogDF_::LogDfAt(double yf) const {
        if (yf <= yf_.back())
            return (*interp_)(yf);
        const int n = static_cast<int>(yf_.size());
        const double dyfLast = yf_[n - 1] - yf_[n - 2];
        if (dyfLast <= 0.0)
            return logDF_.back();
        const double slopeLast = (logDF_[n - 1] - logDF_[n - 2]) / dyfLast;
        return logDF_[n - 1] + slopeLast * (yf - yf_[n - 1]);
    }

    // Free-node count: the anchor (node 0, logDF pinned at 0) is excluded from the unknown
    // vector. For a square 13-instrument / 13-free-node LOG_DISCOUNT solve this is what makes
    // the parameter dimension match the instrument count.
    int DiscountLogDF_::NX() const { return static_cast<int>(logDF_.size()) - 1; }

    // Bump only the FREE nodes logDF_[1..] by dx[i] * leverage; keep logDF_[0] pinned at 0 and
    // rebuild interp_ so operator() reflects the bumped curve. A stale interp_ after a bump would
    // silently desync the curve from its node values -- the risk-engine landmine the reviewer flagged.
    void DiscountLogDF_::ApplyDX(Vector_<>::const_iterator dx, double leverage) {
        for (int i = 1; i < static_cast<int>(logDF_.size()); ++i)
            logDF_[i] += leverage * *dx++;
        RebuildInterp();
    }

    void DiscountLogDF_::Write(Archive::Store_& dst) const {
        DiscountLogDF_v2::XWrite(dst,
                                 this->Name(),
                                 this->ccy_.String(),
                                 nodeDates_,
                                 logDF_,
                                 dayCount_.String(),
                                 scheme_.String(),
                                 base_);
    }

    DiscountLogDF_* DiscountLogDF_::Clone(const String_& new_name,
                                          const YCComponent_::substitutions_t& base_changes) const {
        return new DiscountLogDF_(new_name, this->ccy_.String(), nodeDates_, logDF_, dayCount_, scheme_, NewBase(base_changes));
    }

    Vector_<> DiscountLogDF_::NodeDF() const {
        Vector_<> retval(logDF_.size());
        for (int i = 0; i < static_cast<int>(logDF_.size()); ++i)
            retval[i] = std::exp(logDF_[i]);
        return retval;
    }

    Vector_<std::pair<int, double>> DiscountLogDF_::InterpBasisWeights(double yf) const {
        // Defensive guard: calibration forbids yf < 0 (before the anchor). Return empty so the
        // assembler skips this point and the bumped fallback engages for any instrument reading
        // such a date.
        if (yf < 0.0 || yf_.size() < 2)
            return {};
        // Helper: take the (storage node, weight) pairs and (a) drop anchor storage node 0,
        // (b) remap storage node k to solver column k-1, (c) drop ~zero weights.
        const auto remap = [](const Vector_<std::pair<int, double>>& storageWeights) {
            Vector_<std::pair<int, double>> out;
            out.reserve(storageWeights.size());
            for (const auto& [storageNode, w] : storageWeights) {
                if (storageNode <= 0)
                    continue; // anchor (node 0) is pinned at logDF=0 -> structural zero contribution
                if (w == 0.0)
                    continue;
                out.emplace_back(storageNode - 1, w);
            }
            return out;
        };
        // Extrapolation past the last knot: all schemes use the same final-segment secant extension
        // in LogDfAt (see yclogdf.cpp docstring). Apply the same secant weights uniformly.
        if (yf > yf_.back())
            return remap(CubicExtrapWeights(yf));
        // Locate the segment [k, k+1] that contains yf.
        const int k = LowerBoundSegment(yf_, yf);
        const double h = yf_[k + 1] - yf_[k];
        if (scheme_ == LogDfScheme_::Value_::LOG_LINEAR) {
            // Linear basis: (1-g) at storage k, g at storage k+1, with g = (yf - yf_[k])/h.
            const double g = (yf - yf_[k]) / h;
            Vector_<std::pair<int, double>> storageWeights;
            storageWeights.emplace_back(k, 1.0 - g);
            storageWeights.emplace_back(k + 1, g);
            return remap(storageWeights);
        }
        if (scheme_ == LogDfScheme_::Value_::LOG_CUBIC_NATURAL) {
            return remap(CubicBasisAt(k, yf));
        }
        if (scheme_ == LogDfScheme_::Value_::MIXED) {
            // Dispatch by side of cutoff: linear head (<= cutoff), cubic tail (> cutoff).
            if (yf <= mixedCutoffYf_) {
                // Linear head: only storage nodes [0..cutoffIndex] carry weight. On segment [k,k+1]
                // with k <= cutoffIndex-1: (1-g) at storage k, g at storage k+1.
                const double g = (yf - yf_[k]) / h;
                Vector_<std::pair<int, double>> storageWeights;
                storageWeights.emplace_back(k, 1.0 - g);
                storageWeights.emplace_back(k + 1, g);
                return remap(storageWeights);
            }
            // Cubic tail: fppCoef_ is already in storage-indexed form (RebuildBasisAux maps the
            // tail subarray back to global indices). CubicBasisAt consumes it directly.
            return remap(CubicBasisAt(k, yf));
        }
        THROW(String_("DiscountLogDF_::InterpBasisWeights: unknown scheme: ") + scheme_.String());
    }

    DiscountCurve_* NewDiscountLogDF(const String_& name,
                                     const String_& ccy,
                                     const Vector_<Date_>& nodeDates,
                                     const Vector_<>& logDF,
                                     const DayBasis_& dayCount,
                                     LogDfScheme_ scheme,
                                     const Handle_<DiscountCurve_>& base) {
        return new DiscountLogDF_(name, ccy, nodeDates, logDF, dayCount, scheme, base);
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
        return new DiscountLogDF_(
            name_, ccy_, nodeDates_, logDF_, DayBasis_(dayCount_), LogDfScheme_::Value_::LOG_LINEAR, base_);
    }

    Storable_* DiscountLogDF_v2::Reader_::Build() const {
        // v2 stores the scheme by name and rebuilds the interpolator from (nodeDates, logDF).
        return new DiscountLogDF_(name_, ccy_, nodeDates_, logDF_, DayBasis_(dayCount_), LogDfScheme_(scheme_), base_);
    }
} // namespace Dal

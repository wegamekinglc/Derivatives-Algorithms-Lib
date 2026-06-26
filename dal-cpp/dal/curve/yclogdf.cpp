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
        // Centralised here so calibration, ApplyDX, and deserialisation share one definition.
        Handle_<Interp1_> BuildLogDfInterpFromYf(const String_& name,
                                                 LogDfScheme_ scheme,
                                                 const Vector_<>& yf,
                                                 const Vector_<>& logDF) {
            switch (scheme.Switch()) {
            case LogDfScheme_::Value_::LOG_LINEAR:
                // linear on log(DF) is log-linear on DF
                return Handle_<Interp1_>(Interp::NewLinear(name + "_loglin", yf, logDF));
            case LogDfScheme_::Value_::LOG_CUBIC_NATURAL: {
                const Interp::Boundary_ natural(2, 0.0);
                return Handle_<Interp1_>(Interp::NewCubic(name + "_logcub", yf, logDF, natural, natural));
            }
            case LogDfScheme_::Value_::MIXED: {
                // cutoff at (nKnots-4) so the cubic tail has >= 3 knots beyond it.
                const int cutoffIndex = std::max(1, static_cast<int>(yf.size()) - 5);
                MixedSchemeSpec_ spec;
                spec.cutoffYf_ = yf[cutoffIndex];
                return Handle_<Interp1_>(NewMixedLogDF(name + "_mixed", yf, logDF, spec));
            }
            default:
                THROW(String_("Unknown LOG_DISCOUNT scheme: ") + scheme.String());
            }
        }

        // Standard Thomas algorithm; M is diagonally dominant (true for natural-cubic splines).
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
            Vector_<> y(n);
            y[0] = b[0] / dp[0];
            for (int i = 1; i < n; ++i)
                y[i] = (b[i] - a[i - 1] * y[i - 1]) / dp[i];
            x[n - 1] = y[n - 1];
            for (int i = n - 2; i >= 0; --i)
                x[i] = y[i] - cp[i] * x[i + 1];
            return x;
        }

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

        void NaturalCubicInteriorMatrix(const Vector_<>& h, int m, Vector_<>* sub, Vector_<>* diag, Vector_<>* sup) {
            for (int r = 0; r < m; ++r) {
                const int i = r + 1;
                (*diag)[r] = 2.0 * (h[i] + h[i + 1]);
                if (r > 0)
                    (*sub)[r - 1] = h[i];
                if (r < m - 1)
                    (*sup)[r] = h[i + 1];
            }
        }

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

        int LowerBoundSegment(const Vector_<>& yf, double yfQuery) {
            const int n = static_cast<int>(yf.size());
            int k = 0;
            for (int i = 1; i < n - 1; ++i) {
                if (yf[i] <= yfQuery)
                    k = i;
                else
                    break;
            }
            // Clamp: yfQuery may equal yf[n-1] exactly.
            if (k > n - 2)
                k = n - 2;
            return k;
        }

        Vector_<std::pair<int, double>> LinearSegmentWeights(const Vector_<>& yf, int k, double yfQuery) {
            const double h = yf[k + 1] - yf[k];
            const double g = (yfQuery - yf[k]) / h;
            Vector_<std::pair<int, double>> storageWeights;
            storageWeights.emplace_back(k, 1.0 - g);
            storageWeights.emplace_back(k + 1, g);
            return storageWeights;
        }
    } // namespace

    namespace Tape {

    // Runtime validation (isfinite, logDF[0]==0) is double-only -- Number_ value extraction
    // would record spurious tape nodes. The Number_ factory pins logDF[0]=0 before construction.
    template <class T_>
    DiscountLogDF_<T_>::DiscountLogDF_(const String_& name,
                                          const String_& ccy,
                                          const Vector_<Date_>& nodeDates,
                                          const Vector_<T_>& logDF,
                                          const DayBasis_& dayCount,
                                          LogDfScheme_ scheme,
                                          const Handle_<Dal::DiscountCurve_>& base)
        : CurveWithBase_<DiscountCurve_<T_>, DiscountCurve_<double>>(name, ccy, base),
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
    void DiscountLogDF_<T_>::RebuildInterp() {
        // interp_ is always double-valued; extract via Value() for existence,
        // but the Number_ LogDfAt path does not consult it.
        Vector_<> logDFDouble(yf_.size());
        for (int i = 0; i < static_cast<int>(logDF_.size()); ++i)
            logDFDouble[i] = Dal::AAD::Value(logDF_[i]);
        interp_ = BuildLogDfInterpFromYf(this->Name(), scheme_, yf_, logDFDouble);
        RebuildBasisAux();
    }

    template <class T_>
    void DiscountLogDF_<T_>::RebuildBasisAux() {
        fppCoef_.clear();
        mixedCutoffIndex_ = -1;
        mixedCutoffYf_ = 0.0;
        if (scheme_ == LogDfScheme_::Value_::LOG_CUBIC_NATURAL) {
            fppCoef_ = BuildNaturalCubicFppCoef(yf_);
        } else if (scheme_ == LogDfScheme_::Value_::MIXED) {
            // Cutoff at (n-4)-th knot; cubic tail fppCoef_ computed on subarray, expanded back.
            const int cutoffIndex = std::max(1, static_cast<int>(yf_.size()) - 5);
            mixedCutoffIndex_ = cutoffIndex;
            mixedCutoffYf_ = yf_[cutoffIndex];
            const int tailLen = static_cast<int>(yf_.size()) - cutoffIndex;
            Vector_<> tailYf(tailLen);
            for (int i = 0; i < tailLen; ++i)
                tailYf[i] = yf_[cutoffIndex + i];
            Vector_<Vector_<>> tailCoef = BuildNaturalCubicFppCoef(tailYf);
            fppCoef_ = Vector_<Vector_<>>(yf_.size(), Vector_<>(yf_.size(), 0.0));
            for (int k = 0; k < tailLen; ++k)
                for (int j = 0; j < tailLen; ++j)
                    fppCoef_[cutoffIndex + k][cutoffIndex + j] = tailCoef[k][j];
        }
    }

    template <class T_>
    Vector_<std::pair<int, double>> DiscountLogDF_<T_>::CubicBasisAt(int k, double yf) const {
        // fppCoef_ is the storage-indexed second-derivative sensitivity matrix.
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
    Vector_<std::pair<int, double>> DiscountLogDF_<T_>::CubicExtrapWeights(double yf) const {
        // Secant-extension of the last segment: does NOT use the cubic derivative at the boundary,
        // even for LOG_CUBIC_NATURAL.
        const int n = static_cast<int>(yf_.size());
        const double dyfLast = yf_[n - 1] - yf_[n - 2];
        Vector_<std::pair<int, double>> retval;
        if (dyfLast <= 0.0) {
            retval.emplace_back(n - 1, 1.0);
            return retval;
        }
        const double excess = (yf - yf_[n - 1]) / dyfLast;
        retval.emplace_back(n - 2, -excess);
        retval.emplace_back(n - 1, 1.0 + excess);
        return retval;
    }

    template <class T_>
    Vector_<std::pair<int, double>> DiscountLogDF_<T_>::StorageBasisWeightsAt(double yf) const {
        // Returns storage-node weights (knot-position-dependent only, same for any T_).
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
            THROW(String_("DiscountLogDF_::StorageBasisWeightsAt: unknown scheme: ") + scheme_.String());
        }
    }

    template <class T_>
    T_ DiscountLogDF_<T_>::operator()(const Date_& from, const Date_& to) const {
        const double yfFrom = dayCount_(nodeDates_.front(), from, nullptr);
        const double yfTo = dayCount_(nodeDates_.front(), to, nullptr);
        if constexpr (std::is_same_v<T_, double>) {
            const double logDfFrom = LogDfAt(yfFrom);
            const double logDfTo = LogDfAt(yfTo);
            return std::exp(logDfTo - logDfFrom) * (this->base_ ? (*this->base_)(from, to) : 1.0);
        } else {
            // Base curve is double and treated as a constant multiplier.
            const T_ logDfFrom = LogDfAt(yfFrom);
            const T_ logDfTo = LogDfAt(yfTo);
            const double baseFactor = this->base_ ? (*this->base_)(from, to) : 1.0;
            return Dal::AAD::exp(logDfTo - logDfFrom) * baseFactor;
        }
    }

    template <class T_>
    T_ DiscountLogDF_<T_>::LogDfAt(double yf) const {
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
            // Anchor is pinned at logDF=0; excluding it gives the correct Jacobian
            // and keeps the column map (solver col = storage node - 1) clean.
            T_ acc(static_cast<double>(0.0));
            for (const auto& [storageNode, w] : StorageBasisWeightsAt(yf)) {
                if (storageNode == 0)
                    continue;
                acc += static_cast<double>(w) * logDF_[storageNode];
            }
            return acc;
        }
    }

    // Free-node count: anchor (node 0) is pinned, so solver dimension = n-1.
    template <class T_>
    int DiscountLogDF_<T_>::NX() const { return static_cast<int>(logDF_.size()) - 1; }

    // Stale interp_ after bump silently desyncs the curve from node values.
    template <class T_>
    void DiscountLogDF_<T_>::ApplyDX(Vector_<>::const_iterator dx, double leverage) {
        for (int i = 1; i < static_cast<int>(logDF_.size()); ++i)
            logDF_[i] += leverage * *dx++;
        RebuildInterp();
    }

    template <class T_>
    void DiscountLogDF_<T_>::Write(Archive::Store_& dst) const {
        // Extract primal via backend-neutral Value facade.
        Vector_<> logDFDouble(logDF_.size());
        for (int i = 0; i < static_cast<int>(logDF_.size()); ++i)
            logDFDouble[i] = Dal::AAD::Value(logDF_[i]);
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
    DiscountLogDF_<T_>* DiscountLogDF_<T_>::Clone(const String_& newName,
                                                    const YCComponent_::substitutions_t& baseChanges) const {
        return new DiscountLogDF_<T_>(newName, this->ccy_.String(), nodeDates_, logDF_, dayCount_, scheme_, this->NewBase(baseChanges));
    }

    template <class T_>
    Vector_<> DiscountLogDF_<T_>::NodeDF() const {
        Vector_<> retval(logDF_.size());
        for (int i = 0; i < static_cast<int>(logDF_.size()); ++i)
            retval[i] = std::exp(Dal::AAD::Value(logDF_[i]));
        return retval;
    }

    template <class T_>
    Vector_<> DiscountLogDF_<T_>::NodeLogDF() const {
        Vector_<> retval(logDF_.size());
        for (int i = 0; i < static_cast<int>(logDF_.size()); ++i)
            retval[i] = Dal::AAD::Value(logDF_[i]);
        return retval;
    }

    template class DiscountLogDF_<double>;
    template class DiscountLogDF_<Dal::AAD::Number_>;

    } // namespace Tape

    DiscountCurve_* NewDiscountLogDF(const String_& name,
                                     const String_& ccy,
                                     const Vector_<Date_>& nodeDates,
                                     const Vector_<>& logDF,
                                     const DayBasis_& dayCount,
                                     LogDfScheme_ scheme,
                                     const Handle_<DiscountCurve_>& base) {
        return new Tape::DiscountLogDF_<double>(name, ccy, nodeDates, logDF, dayCount, scheme, base);
    }

#include <dal/auto/MG_DiscountLogDF_v1_Read.inc>
#include <dal/auto/MG_DiscountLogDF_v2_Read.inc>

    Storable_* DiscountLogDF_v1::Reader_::Build() const {
        // Legacy v1 stored a built Interp1_ handle and did NOT persist LogDfScheme_.
        // The scheme cannot be recovered from the handle (all interp subtypes report
        // type "Interp1"), so v1 always reconstructs as LOG_LINEAR.
        // v2 (the canonical format) carries the scheme by name.
        return new Tape::DiscountLogDF_<double>(
            name_, ccy_, nodeDates_, logDF_, DayBasis_(dayCount_), LogDfScheme_::Value_::LOG_LINEAR, base_);
    }

    Storable_* DiscountLogDF_v2::Reader_::Build() const {
        return new Tape::DiscountLogDF_<double>(name_, ccy_, nodeDates_, logDF_, DayBasis_(dayCount_), LogDfScheme_(scheme_), base_);
    }
} // namespace Dal

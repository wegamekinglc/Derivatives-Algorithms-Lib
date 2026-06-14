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

    void DiscountLogDF_::RebuildInterp() { interp_ = BuildLogDfInterpFromYf(Name(), scheme_, yf_, logDF_); }

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
        // Legacy v1 stored the Interp1_ handle directly and did NOT persist the LogDfScheme_. We
        // reconstruct the scheme from the stored handle's Storable_::type_ so old files still read.
        //
        // v2 is the canonical format: it carries the scheme by name and rebuilds the interpolator
        // from (nodeDates, logDF), so it does not need this inference. The mapping below is therefore
        // a best-effort backward-compat shim and only covers schemes that v1 could actually emit:
        //   - v1 LOG_LINEAR wrote Interp::NewLinear, which serialises as "Interp1Linear".
        //   - v1 LOG_CUBIC_NATURAL wrote Interp::NewCubic, which serialises as "Cubic1".
        //   - v1 MIXED used NewMixedLogDF, whose Write() always throws, so no v1 file can carry a
        //     mixed interpolator; that scheme is therefore not reachable here and is not mapped.
        // Any other type_ (or a null handle) falls back to LOG_LINEAR, matching v1's default build.
        LogDfScheme_ scheme = LogDfScheme_::Value_::LOG_LINEAR;
        if (interp_) {
            const String_& t = interp_->type_;
            if (t == "Cubic1")
                scheme = LogDfScheme_::Value_::LOG_CUBIC_NATURAL;
        }
        return new DiscountLogDF_(name_, ccy_, nodeDates_, logDF_, DayBasis_(dayCount_), scheme, base_);
    }

    Storable_* DiscountLogDF_v2::Reader_::Build() const {
        // v2 stores the scheme by name and rebuilds the interpolator from (nodeDates, logDF).
        return new DiscountLogDF_(name_, ccy_, nodeDates_, logDF_, DayBasis_(dayCount_), LogDfScheme_(scheme_), base_);
    }
} // namespace Dal

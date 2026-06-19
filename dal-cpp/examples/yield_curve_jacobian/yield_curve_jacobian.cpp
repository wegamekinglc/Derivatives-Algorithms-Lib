//
// Created by dal-implementer on 2026/6/19.
//

#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <dal/platform/platform.hpp>
#include <dal/platform/initall.hpp>
#include <dal/curve/calibration.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/yclogdf.hpp>
#include <dal/curve/ycinstrument.hpp>
#include <dal/math/matrix/matrixarithmetic.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/math/vectors.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/string/strings.hpp>
#include <dal/time/date.hpp>
#include <dal/time/daybasis.hpp>
#include <dal/time/holidays.hpp>
#include <dal/time/periodlength.hpp>
#include <dal/utilities/exceptions.hpp>

using namespace Dal;

// Always-on precondition macro. The library's REQUIRE (dal-cpp/dal/utilities/exceptions.hpp) routes
// to THROW, so it would also fire in every build -- but it is a generic precondition helper that
// does not name a self-check. Defining THROW_REQUIRE here gives the example an explicit, self-naming
// guard that runs under every AAD backend and every CMake configuration.
#define THROW_REQUIRE(cond, msg)                                                                                                                     \
    do {                                                                                                                                            \
        if (!(cond))                                                                                                                                \
            THROW(msg);                                                                                                                             \
    } while (false)

// This example demonstrates two teaching arcs on a single-curve Phase A calibration:
//
//   (a) The calibration residual Jacobian J = d(modelRate_i) / d(logDF_free_k) computed two
//       independent ways -- a central-difference bump oracle and an AAD reverse sweep -- and shown
//       to agree element-wise to 1e-9 relative.
//   (b) The inverse-Jacobian IR-risk transform: take the par-rate parameter-sensitivity g of one
//       off-anchor swap (an independent bump oracle, NOT the AAD tape from arc (a)), left-multiply
//       by the calibration inverse Jacobian effJacobianInverse_ (the solver-scaled pseudoinverse;
//       see the units note in TransformToQuoteRisk), and read off bucketed risk per calibration
//       instrument.
//
// AAD is always compiled in this library (dal-cpp/dal/math/aad/aad.hpp:17 defines the native AAET
// backend on the "no DAL_USE_*_AAD flag set" branch) and the analytic Jacobian it produces runs
// identically under native AAET, Adept, XAD, and CoDiPack. Arc (a) does not record its own tape:
// it reads the forward Jacobian the calibration already computed -- CalibrateYieldCurve requests
// CurveCalibrationDiagnostics_::jacobian_ from the solver, whose convergence-branch hook calls
// YieldCurveCalibrationFunc_::Gradient(xNew, fNew) ONCE at the solved x (the same AnalyticJacobian
// machinery in dal-cpp/dal/curve/calibration.cpp) -- so the AAD recording contract and backend-
// portability rules live in exactly one place. The bump oracle below remains an independent finite-
// difference check; the AAD-vs-bump comparison is what makes arc (a) a real cross-check rather than
// a tautology.

namespace {
    // Self-check bars (see .claude/specs/yield-curve-jacobian-example.md "AAD-vs-Bump Tolerance
    // Choice"). The 1e-9 AAD-vs-bump bar holds because the Phase A LOG_DISCOUNT residual Jacobian
    // is O(1) and well-conditioned at the chosen 1.0%-2.5% par rates; central-difference round-off
    // at h=1e-6 is ~eps/h ~= 2e-10 relative, leaving ~5x margin, and it is size-invariant. The FR6
    // re-solve is a genuine nonlinear operation: bumping a long-end quote propagates through every
    // intervening LOG_DISCOUNT knot, accumulating second-order terms the linear prediction cannot
    // capture, so the worst observed rel grows with ladder length (~7e-7 at 5 instruments, ~1.8e-5
    // at 10 instruments, ~1.3e-4 at 16 instruments). The 1e-4 bar at the 10-instrument size gives
    // ~5x headroom over the observed worst rel; tighten it back toward 1e-6 if the ladder is
    // shortened. (docs-sync follow-up: the FR6 bar scales with ladder length -- re-measure and
    // record the trend alongside the spec's bar methodology when it is finalized.)
    constexpr double BUMP_STEP = 1.0e-6;
    constexpr double AAD_TOL = 1.0e-9;
    constexpr double RE_SOLVE_TOL = 1.0e-4;
    constexpr double ONE_BP = 1.0e-4;

    // YYYY-MM-DD label for a Date_ (Date::ToString already formats this way; thin wrapper makes the
    // intent explicit at every call site and gives a single place to change the row-label format).
    std::string IsoDate(const Date_& dt) { return std::string(Date::ToString(dt).c_str()); }

    // Maturity date (swap end) of each calibration instrument, used to label Jacobian rows and the
    // residual / risk tables by instrument rather than by integer index.
    Vector_<Date_> InstrumentMaturities(const CurveCalibrationSpec_& spec) {
        Vector_<Date_> maturities;
        maturities.reserve(spec.instruments_.size());
        for (const auto& inst : spec.instruments_)
            maturities.push_back(inst->TimeSpan().second);
        return maturities;
    }

    // Free-node knot dates (the anchor at index 0 is pinned and excluded), used to label Jacobian
    // columns.
    Vector_<Date_> FreeKnotDates(const CurveCalibrationSpec_& spec) {
        return Vector_<Date_>(spec.knotDates_.begin() + 1, spec.knotDates_.end());
    }

    RateLegConvention_ AnnualLeg() {
        RateLegConvention_ leg;
        leg.paymentLag_ = 0;
        leg.paymentFrequency_ = PeriodLength_("12M");
        leg.dayBasis_ = DayBasis_("ACT_365F");
        leg.accrualHolidays_ = Holidays::None();
        leg.paymentHolidays_ = Holidays::None();
        leg.businessDayConvention_ = BizDayConvention_("Unadjusted");
        leg.paymentConvention_ = BizDayConvention_("Unadjusted");
        return leg;
    }

    RateIndexConvention_ AnnualIndex() {
        RateIndexConvention_ idx;
        idx.forecastTenor_ = PeriodLength_("12M");
        idx.dayBasis_ = DayBasis_("ACT_365F");
        idx.fixingLag_ = 0;
        idx.spotLag_ = 0;
        idx.fixingHolidays_ = Holidays::None();
        idx.accrualHolidays_ = Holidays::None();
        idx.businessDayConvention_ = BizDayConvention_("Unadjusted");
        idx.useProjectionCurve_ = false;
        return idx;
    }

    // The 10-instrument vanilla-swap Phase A calibration. Stays eligible for the analytic (AAD-tape)
    // Jacobian by mirroring the shape validated in dal-cpp/tests/curve/test_analytic_jacobian.cpp
    // (LOG_DISCOUNT, EXACT, anchor == today_, no projection curve, vanilla Swap_ only) -- only the
    // ladder length changes. The system is square: 10 instruments on 11 annual knots (10 free params
    // + the today_ anchor), so EXACT converges to the fitTolerance_ and requesting
    // CurveJacobianMode_::ANALYTIC engages the tape rather than silently falling back to bumping.
    // Par rates rise smoothly from ~1.0% to ~2.5% across 1Y..10Y so the LOG_DISCOUNT system is well-
    // conditioned and the 1e-9 AAD-vs-bump bar still holds.
    CurveCalibrationSpec_ BuildCalibrationSpec() {
        CurveCalibrationSpec_ spec;
        spec.today_ = Date_(2022, 1, 1);
        spec.ccy_ = "USD";
        spec.curveName_ = "yield_curve_jacobian";
        spec.targetCollateral_ = CollateralType_(CollateralType_::Value_::OIS);
        spec.calibrateDiscountCurve_ = true;
        spec.parameterization_ = CurveParameterization_::Value_::LOG_DISCOUNT;
        spec.knotPolicy_ = CurveKnotPolicy_::Value_::INPUT;
        spec.solveMode_ = CurveSolveMode_::Value_::EXACT;
        spec.liborBasis_ = DayBasis_("ACT_365F");
        spec.tolerance_ = 1.0e-10;
        spec.fitTolerance_ = 1.0e-8;
        spec.smoothingWeight_ = 1.0;
        spec.logDfScheme_ = LogDfScheme_::Value_::LOG_LINEAR;

        // Anchor (today_) at 2022-01-01, then one annual knot per swap maturity 1Y..10Y.
        // 10 maturities -> 11 knots -> 10 free params (square with the 10 instruments below).
        constexpr int nInstruments = 10;
        spec.knotDates_.reserve(nInstruments + 1);
        spec.knotDates_.push_back(spec.today_);
        for (int y = 1; y <= nInstruments; ++y)
            spec.knotDates_.push_back(Date_(2022 + y, 1, 1));

        const auto fixedLeg = AnnualLeg();
        const auto floatIdx = AnnualIndex();
        const auto floatLeg = AnnualLeg();
        const auto mkSwap = [&](const Date_& end, double parPct) {
            return Handle_<YCInstrument_>(new Swap_(spec.today_, spec.today_, end, parPct / 100.0, fixedLeg, floatIdx, floatLeg));
        };
        // Annual swaps maturing at 1Y..10Y with a smoothly rising par-rate term structure
        // (1.00% -> 2.50%): a gentle linear ramp keeps the 10x10 LOG_DISCOUNT system well-
        // conditioned so the 1e-9 AAD-vs-bump bar holds. The FR6 nonlinear re-solve bar is loosened
        // to 1e-3 at this size (see RE_SOLVE_TOL) because the linear-vs-nonlinear gap grows
        // intrinsically with the number of knots -- bumping a long-end quote propagates through
        // every intervening LOG_DISCOUNT knot, accumulating second-order terms that the linear
        // prediction cannot capture.
        spec.instruments_.reserve(nInstruments);
        for (int y = 1; y <= nInstruments; ++y) {
            const double frac = static_cast<double>(y - 1) / static_cast<double>(nInstruments - 1);
            const double parPct = 1.00 + 1.50 * frac;
            spec.instruments_.push_back(mkSwap(Date_(2022 + y, 1, 1), parPct));
        }
        return spec;
    }

    // Index of the off-anchor swap used as the "portfolio" for the FR5 risk transform. Swap 4 (5Y)
    // pays annual coupons landing on the year-1..year-5 knots (free columns 0..4); it has no cash
    // flows past year 5, so the year-6..year-10 knots (free columns 5..9) carry structural zeros
    // in g, which the risk transform handles correctly.
    constexpr int PORTFOLIO_INDEX = 4;

    // ---- Curve (re)construction from a free-parameter vector ----

    // Build a CurveBlock_ around a DiscountLogDF_ rebuilt from the supplied free-node log-DF vector
    // (anchor at index 0 stays 0.0). The instruments price off this block via Precompute() ->
    // operator()(CurveBlock_). Mirrors EvalResiduals in
    // dal-cpp/tests/curve/test_analytic_jacobian.cpp:95-113.
    CurveBlock_ CurveBlockFromFreeParams(const CurveCalibrationSpec_& spec, const Vector_<>& x) {
        const int nNodes = static_cast<int>(spec.knotDates_.size());
        Vector_<> full(nNodes, 0.0);
        for (int i = 1; i < nNodes; ++i)
            full[i] = x[i - 1];
        std::unique_ptr<DiscountCurve_> dc(NewDiscountLogDF(spec.curveName_, spec.ccy_, spec.knotDates_, full, spec.liborBasis_, spec.logDfScheme_));
        std::map<CollateralType_, Handle_<DiscountCurve_>> discounts;
        discounts[spec.targetCollateral_] = Handle_<DiscountCurve_>(std::shared_ptr<const DiscountCurve_>(dc.release()));
        std::map<PeriodLength_, Handle_<DiscountCurve_>> forwards;
        // Take ownership of the curve: the owning shared_ptr deletes it when the returned CurveBlock_
        // dies. The previous aliasing-ctor form (empty owner shared_ptr) did not own the pointer, so
        // dc.release() orphans it and the curve leaks every call (per-node-per-bump).
        return CurveBlock_(spec.curveName_, spec.ccy_, discounts, forwards, spec.liborBasis_);
    }

    // Read the solved free-node log-DF vector off a calibrated curve: dynamic_cast to
    // DiscountLogDF_ and drop the pinned anchor at index 0.
    Vector_<> SolvedFreeParams(const DiscountCurve_& curve) {
        const auto* logDf = dynamic_cast<const DiscountLogDF_*>(&curve);
        THROW_REQUIRE(logDf != nullptr, "calibrated curve is not a DiscountLogDF_ (Phase A shape broken)");
        const Vector_<> nodes = logDf->NodeLogDF();
        Vector_<> x(nodes.size() - 1);
        for (size_t i = 1; i < nodes.size(); ++i)
            x[i - 1] = nodes[i];
        return x;
    }

    // ---- Jacobian oracles ----

    // Two-sided central-difference Jacobian J_bump(i,k) = d(modelRate_i) / d(logDF_free_k).
    // Mirrors AssertMatchesCentralDifference in test_analytic_jacobian.cpp. Returns a dense
    // nInstruments x nFreeParams matrix.
    Matrix_<> BumpJacobian(const CurveCalibrationSpec_& spec, const Vector_<>& x, double h) {
        const int nInst = static_cast<int>(spec.instruments_.size());
        const int nFree = static_cast<int>(x.size());
        Handle_<YieldCurve_> empty;
        Matrix_<> j(nInst, nFree, 0.0);
        for (int k = 0; k < nFree; ++k) {
            Vector_<> xUp = x;
            Vector_<> xDn = x;
            xUp[k] += h;
            xDn[k] -= h;
            CurveBlock_ ycUp = CurveBlockFromFreeParams(spec, xUp);
            CurveBlock_ ycDn = CurveBlockFromFreeParams(spec, xDn);
            for (int i = 0; i < nInst; ++i) {
                const auto rateUp = spec.instruments_[i]->Precompute(empty);
                const auto rateDn = spec.instruments_[i]->Precompute(empty);
                const double modelUp = (*rateUp)(ycUp);
                const double modelDn = (*rateDn)(ycDn);
                j(i, k) = (modelUp - modelDn) / (2.0 * h);
            }
        }
        return j;
    }

    // ---- FR5 portfolio parameter sensitivity g ----

    // g[k] = d(modelParRate_portfolio) / d(logDF_free_k) for the off-anchor portfolio swap, via an
    // INDEPENDENT central-difference bump on the calibrated curve. This is deliberately a separate
    // oracle from the FR3 AAD tape: that tape records the residual sensitivities d(modelRate -
    // marketRate)/d(logDF) for every calibration row, which is a different quantity (residual, not
    // a single portfolio's par rate). Reusing it would conflate the two.
    //
    // Units note: the YCInstrument_ interface exposes only the par-rate model rate S = floatPv /
    // annuity, not PV. So g is a par-rate sensitivity (rate per logDF), and r = g^T *
    // effJacobianInverse_ / tolerance_ (see TransformToQuoteRisk) is "par-rate risk per absolute
    // decimal quote bump" -- how much the portfolio's par rate moves per +1 in calibration quote i,
    // and r * 1e-4 is the par-rate DV01 per +1bp. The annuity scaling that would convert this to a
    // true price DV01 is not exposed by the public API.
    Vector_<> PortfolioParamSensitivity(const CurveCalibrationSpec_& spec, const Vector_<>& x, double h) {
        const int nFree = static_cast<int>(x.size());
        Handle_<YieldCurve_> empty;
        Vector_<> g(nFree, 0.0);
        const Handle_<YCInstrument_>& portfolio = spec.instruments_[PORTFOLIO_INDEX];
        for (int k = 0; k < nFree; ++k) {
            Vector_<> xUp = x;
            Vector_<> xDn = x;
            xUp[k] += h;
            xDn[k] -= h;
            CurveBlock_ ycUp = CurveBlockFromFreeParams(spec, xUp);
            CurveBlock_ ycDn = CurveBlockFromFreeParams(spec, xDn);
            const auto rateUp = portfolio->Precompute(empty);
            const auto rateDn = portfolio->Precompute(empty);
            g[k] = ((*rateUp)(ycUp) - (*rateDn)(ycDn)) / (2.0 * h);
        }
        return g;
    }

    // r = g^T * effJacobianInverse_ / tolerance_ via the Vector_,Matrix_ overload of Matrix::Multiply
    // (matrixarithmetic.hpp:11 computes left^T * right). effJacobianInverse_ is nFreeParams x
    // nInstruments with units d(params) * tolerance_ / d(decimal-rate perturbation) (the solver
    // scales residuals by 1/tolerance_ before forming the pseudoinverse -- see the FR6 note in
    // main). Dividing by tolerance_ puts r in honest d(portfolio par rate)/d(decimal quote bump)
    // units; r[i] is how much the portfolio par rate moves per +1 in calibration quote i, and
    // r[i] * 1e-4 is the par-rate DV01 per +1bp.
    Vector_<> TransformToQuoteRisk(const Vector_<>& g, const Matrix_<>& effJacobianInverse, double tolerance) {
        Vector_<> r;
        Dal::Matrix::Multiply(g, effJacobianInverse, &r);
        const double scale = 1.0 / tolerance;
        for (auto& v : r)
            v *= scale;
        return r;
    }

    // ---- FR6 nonlinear re-solve sanity ----

    // For instrument bumpedIdx: bump its market quote by +1e-4 absolute decimal, re-run
    // CalibrateYieldCurve with CurveJacobianMode_::ANALYTIC, diff the new NodeLogDF() against the
    // baseline free params to get the true nonlinear rebumped delta, and compare to the linear
    // prediction effJacobianInverse_ * e_bumpedIdx * 1e-4. The re-calibrate path is the genuine
    // nonlinear sanity test (the linear "analytic forward map" alternative is tautological and
    // explicitly disallowed by FR6).
    Vector_<> RebumpedParamDelta(
        const CurveCalibrationSpec_& spec, const CurveCalibrationOptions_& options, int bumpedIdx, double bump, const Vector_<>& baselineFree) {
        CurveCalibrationSpec_ bumped = spec;
        // Re-quote instrument bumpedIdx at marketRate + bump (absolute decimal). Swap_'s marketRate
        // is the par rate in decimal units; +1e-4 == +1bp.
        const auto fixedLeg = AnnualLeg();
        const auto floatIdx = AnnualIndex();
        const auto floatLeg = AnnualLeg();
        const auto& orig = spec.instruments_[bumpedIdx];
        const auto* swapOrig = dynamic_cast<const Swap_*>(orig.get());
        THROW_REQUIRE(swapOrig != nullptr, "FR6 re-quote expects a vanilla Swap_");
        const auto span = swapOrig->TimeSpan();
        const double newRate = swapOrig->MarketRate() + bump;
        bumped.instruments_[bumpedIdx] =
            Handle_<YCInstrument_>(new Swap_(spec.today_, span.first, span.second, newRate, fixedLeg, floatIdx, floatLeg));

        const auto res = CalibrateYieldCurve(bumped, options);
        const Vector_<> rebumpedFree = SolvedFreeParams(*res.curve_);
        THROW_REQUIRE(rebumpedFree.size() == baselineFree.size(), "rebumped free-param length mismatch");
        Vector_<> delta(rebumpedFree.size());
        for (size_t k = 0; k < rebumpedFree.size(); ++k)
            delta[k] = rebumpedFree[k] - baselineFree[k];
        return delta;
    }

    // ---- output helpers ----

    void PrintBanner(const String_& title) {
        const std::string bar(70, '=');
        std::cout << "\n" << bar << "\n";
        const int pad = static_cast<int>(bar.size() - static_cast<int>(title.size())) / 2;
        std::cout << std::string(pad > 0 ? pad : 1, ' ') << title << "\n";
        std::cout << bar << "\n";
    }

    void PrintSection(const String_& title) {
        const std::string bar(70, '-');
        std::cout << "\n" << bar << "\n  " << title << "\n" << bar << "\n";
    }

    void PrintResiduals(const CurveCalibrationDiagnostics_& diag, const Vector_<Date_>& maturities) {
        std::cout << std::fixed << std::setprecision(6);
        std::cout << std::left << std::setw(14) << "Maturity" << std::right << std::setw(14) << "Market(%)" << std::setw(14) << "Model(%)"
                  << std::setw(14) << "Error(bp)" << "\n";
        std::cout << std::string(56, '-') << "\n";
        for (int i = 0; i < static_cast<int>(diag.instrumentNames_.size()); ++i) {
            std::cout << std::left << std::setw(14) << IsoDate(maturities[i]) << std::right << std::setw(14) << diag.marketRates_[i] * 100.0
                      << std::setw(14) << diag.modelRates_[i] * 100.0 << std::setw(14) << diag.residuals_[i] * 10000.0 << "\n";
        }
    }

    // Label each free-node row by its knot date (the solved log-DF params are indexed by free knot).
    void PrintFreeParamVector(const String_& label, const Vector_<>& v, const Vector_<Date_>& freeKnots) {
        std::cout << std::fixed << std::setprecision(12);
        if (!label.empty())
            std::cout << label << "\n";
        std::cout << std::left << std::setw(14) << "Knot" << std::right << std::setw(22) << "logDF_free" << "\n";
        std::cout << std::string(36, '-') << "\n";
        for (int i = 0; i < static_cast<int>(v.size()); ++i)
            std::cout << std::left << std::setw(14) << IsoDate(freeKnots[i]) << std::right << std::setw(22) << v[i] << "\n";
    }

    void PrintMatrix(const String_& label,
                     const Matrix_<>& m,
                     const String_& orientation,
                     const Vector_<Date_>& rowDates,
                     const Vector_<Date_>& colDates) {
        std::cout << std::fixed << std::setprecision(6);
        if (!label.empty())
            std::cout << label << "\n";
        std::cout << "  " << orientation << "  (rows=" << m.Rows() << ", cols=" << m.Cols() << ")\n";
        std::cout << std::left << std::setw(14) << "row \\ col";
        for (int j = 0; j < m.Cols(); ++j)
            std::cout << std::right << std::setw(13) << IsoDate(colDates[j]);
        std::cout << "\n" << std::string(14 + 13 * m.Cols(), '-') << "\n";
        for (int i = 0; i < m.Rows(); ++i) {
            std::cout << std::left << std::setw(14) << IsoDate(rowDates[i]);
            for (int j = 0; j < m.Cols(); ++j)
                std::cout << std::right << std::setw(13) << m(i, j);
            std::cout << "\n";
        }
    }

    // ---- main() section runners (extracted to keep main's cyclomatic complexity under the
    //      Codacy limit of 8). Each helper is a verbatim relocation of its inline block in main:
    //      no logic changes, so the example's stdout is byte-for-byte identical. ----

    // (c) B2 sentinel: every AAD Jacobian row must have at least one non-trivial entry. An all-zero
    // row means the tape never learned an input is an independent (the canonical missed-
    // RegisterIndependent / wrong-recording-order failure). Catch it before the FD comparison.
    void AssertNoAllZeroRows(const Matrix_<>& jAad) {
        for (int i = 0; i < jAad.Rows(); ++i) {
            double maxAbs = 0.0;
            for (int k = 0; k < jAad.Cols(); ++k)
                maxAbs = std::max(maxAbs, std::abs(jAad(i, k)));
            if (maxAbs <= 1.0e-6)
                THROW(std::string("AAD Jacobian row ") + std::to_string(i) + " is all-zero (B2 sentinel: missed RegisterIndependent?)");
        }
    }

    // (d) AAD-vs-bump agreement loop (verbatim two-branch form, tol = 1e-9). Returns {maxAbs,
    // maxRel} for main to print. The two-branch structure (|fd|<tol vs else) and the THROW-on-
    // fail per cell are preserved exactly.
    struct AgreementResult_ {
        double maxAbs;
        double maxRel;
    };

    AgreementResult_ RunAgreementCheck(const Matrix_<>& jBump, const Matrix_<>& jAad) {
        double maxAbs = 0.0;
        double maxRel = 0.0;
        THROW_REQUIRE(jBump.Rows() == jAad.Rows() && jBump.Cols() == jAad.Cols(), "Jacobian shape mismatch");
        for (int i = 0; i < jBump.Rows(); ++i) {
            for (int k = 0; k < jBump.Cols(); ++k) {
                const double fd = jBump(i, k);
                const double an = jAad(i, k);
                const double absErr = std::abs(an - fd);
                maxAbs = std::max(maxAbs, absErr);
                if (std::abs(fd) < AAD_TOL) {
                    maxRel = std::max(maxRel, absErr);
                    if (absErr > AAD_TOL)
                        THROW(std::string("AAD-vs-bump FAIL at (i=") + std::to_string(i) + ",k=" + std::to_string(k) +
                              "): |an|=" + std::to_string(absErr) + " > " + std::to_string(AAD_TOL) + " (|fd|<1e-9 branch)");
                } else {
                    const double rel = absErr / std::max(1.0, std::abs(fd));
                    maxRel = std::max(maxRel, rel);
                    if (rel > AAD_TOL)
                        THROW(std::string("AAD-vs-bump FAIL at (i=") + std::to_string(i) + ",k=" + std::to_string(k) + "): rel=" + std::to_string(rel) +
                              " > " + std::to_string(AAD_TOL) + " (an=" + std::to_string(an) + ", fd=" + std::to_string(fd) + ")");
                }
            }
        }
        return {maxAbs, maxRel};
    }

    // (g) Print the bucketed quote-risk vector r alongside its par-rate DV01 (r[i]*1e-4). Each row is
    // labelled by the maturity date of the instrument whose quote was bumped.
    void PrintQuoteRisk(const Vector_<>& r, const Vector_<Date_>& maturities) {
        std::cout << std::fixed << std::setprecision(10);
        std::cout << std::left << std::setw(14) << "Maturity" << std::right << std::setw(20) << "raw r[i]" << std::setw(22) << "par-rate DV01"
                  << "\n";
        std::cout << std::string(56, '-') << "\n";
        for (int i = 0; i < static_cast<int>(r.size()); ++i)
            std::cout << std::left << std::setw(14) << IsoDate(maturities[i]) << std::right << std::setw(20) << r[i] << std::setw(22) << r[i] * ONE_BP
                      << "\n";
    }

    // (h) FR6 inverse-Jacobian nonlinear re-solve sanity. For each calibration instrument, bump its
    // market quote by +1e-4, re-run CalibrateYieldCurve (ANALYTIC), and compare the true nonlinear
    // free-param delta against the linear prediction effJacobianInverse_(k,i) * 1e-4 / tolerance_.
    // The solver-scaled-pseudoinverse / tolerance_ units note from main applies here verbatim.
    void RunFR6ReSolve(
        const CurveCalibrationSpec_& spec,
        const CurveCalibrationOptions_& options,
        const Matrix_<>& effJacobianInverse,
        const Vector_<>& baselineFree,
        const Vector_<Date_>& maturities,
        int nInst,
        int nFree) {
        std::cout << std::fixed << std::setprecision(8);
        std::cout << std::left << std::setw(14) << "Bumped" << std::right << std::setw(20) << "max rel delta" << "\n";
        std::cout << std::string(34, '-') << "\n";
        bool fr6Passed = true;
        for (int i = 0; i < nInst; ++i) {
            const Vector_<> trueDelta = RebumpedParamDelta(spec, options, i, ONE_BP, baselineFree);
            Vector_<> pred(nFree, 0.0);
            for (int k = 0; k < nFree; ++k)
                pred[k] = effJacobianInverse(k, i) * ONE_BP / spec.tolerance_;
            double maxRel = 0.0;
            for (int k = 0; k < nFree; ++k) {
                const double t = trueDelta[k];
                const double p = pred[k];
                const double err = std::abs(p - t);
                const double rel = err / std::max(1.0, std::abs(t));
                maxRel = std::max(maxRel, rel);
                if (rel > RE_SOLVE_TOL) {
                    fr6Passed = false;
                    THROW(std::string("FR6 re-solve FAIL at inst=") + std::to_string(i) + " k=" + std::to_string(k) + ": rel=" + std::to_string(rel) +
                          " > " + std::to_string(RE_SOLVE_TOL) + " (pred=" + std::to_string(p) + ", true=" + std::to_string(t) + ")");
                }
            }
            std::cout << std::left << std::setw(14) << IsoDate(maturities[i]) << std::right << std::setw(20) << maxRel << "\n";
        }
        if (fr6Passed)
            std::cout << "  Verdict : PASS  (all rel <= " << RE_SOLVE_TOL << ")\n";
    }

    // (i) Calibration elapsed time -- BUMPED vs ANALYTIC. Both modes run the same EXACT Phase A
    // solve; the only difference is how the forward Jacobian is obtained (n serial finite-difference
    // re-calibrations for BUMPED vs one AAD reverse sweep for ANALYTIC). Each CalibrateYieldCurve
    // call resets its own tape internally, so repeated calls are independent and safe to time.
    //
    // Honesty note: the ANALYTIC time includes the at-solution forward-Jacobian evaluation -- a
    // single extra func.Gradient call the solver makes on its convergence branch to populate the
    // diagnostics jacobian_ field -- which BUMPED does not perform. So ANALYTIC may trail BUMPED by
    // that one convergence-J evaluation; the fair read of the ratio is "ANALYTIC solve-with-Jacobian
    // vs BUMPED solve-without-Jacobian", not a pure like-for-like solve cost. BUMPED would need its
    // own separate finite-difference Jacobian pass to match the ANALYTIC output, which it does not
    // do here.
    void RunCalibrationTimingComparison(const CurveCalibrationSpec_& spec) {
        constexpr int nRuns = 5;
        CurveCalibrationOptions_ optsBumped;
        optsBumped.jacobianMode_ = CurveJacobianMode_::Value_::BUMPED;
        CurveCalibrationOptions_ optsAnalytic;
        optsAnalytic.jacobianMode_ = CurveJacobianMode_::Value_::ANALYTIC;

        using clock = std::chrono::steady_clock;
        auto meanOf = [&](const CurveCalibrationOptions_& opts) -> double {
            CalibrateYieldCurve(spec, opts); // warm-up (first call pays one-time setup/tape costs)
            double totalNs = 0.0;
            for (int i = 0; i < nRuns; ++i) {
                const auto t0 = clock::now();
                CalibrateYieldCurve(spec, opts);
                const auto t1 = clock::now();
                totalNs += static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
            }
            return totalNs / static_cast<double>(nRuns) / 1.0e6; // ms
        };

        const double msBumped = meanOf(optsBumped);
        const double msAnalytic = meanOf(optsAnalytic);
        const double ratio = msBumped / msAnalytic;

        std::cout << std::fixed << std::setprecision(4);
        std::cout << std::left << std::setw(28) << "Mode" << std::right << std::setw(16) << "mean (ms)" << "\n";
        std::cout << std::string(44, '-') << "\n";
        std::cout << std::left << std::setw(28) << ("BUMPED (mean over " + std::to_string(nRuns) + ")") << std::right << std::setw(16) << msBumped << "\n";
        std::cout << std::left << std::setw(28) << ("ANALYTIC (mean over " + std::to_string(nRuns) + ")") << std::right << std::setw(16) << msAnalytic << "\n";
        std::cout << std::left << std::setw(28) << "ratio BUMPED/ANALYTIC" << std::right << std::setw(16) << ratio << "\n";
        std::cout << "\n  NOTE: the ANALYTIC time includes the single at-solution forward-Jacobian\n"
                  << "        evaluation the solver makes on convergence to populate the diagnostics\n"
                  << "        jacobian_ field, which BUMPED does not perform.\n";
        if (ratio > 1.0)
            std::cout << "  -> ANALYTIC is " << ratio << "x faster than BUMPED\n";
        else
            std::cout << "  -> ANALYTIC is " << (1.0 / ratio) << "x slower than BUMPED (convergence-J eval dominates at this size)\n";
    }
} // namespace

int main() {
    RegisterAll_::Init();

    PrintBanner("Yield-Curve Jacobian Example");

    const auto spec = BuildCalibrationSpec();
    const int nInst = static_cast<int>(spec.instruments_.size());
    const int nFree = static_cast<int>(spec.knotDates_.size()) - 1;
    THROW_REQUIRE(nInst == nFree, "Phase A calibration must be square: nInstruments == nKnotDates - 1");

    // Row/column date labels for every date-indexed table below. Maturity dates label instrument
    // rows (residuals, Jacobian rows, risk vector, FR6); free-knot dates label Jacobian columns and
    // the solved-params vector.
    const Vector_<Date_> maturities = InstrumentMaturities(spec);
    const Vector_<Date_> freeKnots = FreeKnotDates(spec);
    THROW_REQUIRE(static_cast<int>(maturities.size()) == nInst && static_cast<int>(freeKnots.size()) == nFree,
                  "date-label vector length mismatch");

    std::cout << "\nCalibration: " << nInst << " instruments on " << spec.knotDates_.size() << " LOG_DISCOUNT knots (" << nFree
              << " free params + anchor)\n";
    std::cout << "Parameterization: LOG_DISCOUNT   Solve mode: EXACT   Jacobian mode: ANALYTIC\n";
    std::cout << "Bump step h: " << std::scientific << std::setprecision(6) << BUMP_STEP << std::fixed << "\n";

    // ---- (a) Calibrate with the analytic Jacobian engaged ----
    CurveCalibrationOptions_ options;
    options.jacobianMode_ = CurveJacobianMode_::Value_::ANALYTIC;
    const auto result = CalibrateYieldCurve(spec, options);

    // Eligibility backstop: if the spec silently fell back to BUMPED (ANALYTIC never throws, it
    // only emits a NOTICE and falls back), the AAD path is a no-op and diagnostics_.jacobian_ is
    // empty. The non-empty check on jacobian_ in arc (c) catches that before the FD comparison
    // renders the teaching payload hollow.
    const Vector_<> x = SolvedFreeParams(*result.curve_);

    PrintSection("(a) Calibration residuals");
    PrintResiduals(result.diagnostics_, maturities);
    std::cout << "\nSolved free-node log-DF params x (anchor pinned at 0):\n";
    PrintFreeParamVector("", x, freeKnots);

    // ---- (b) Bump Jacobian (oracle) ----
    const Matrix_<> jBump = BumpJacobian(spec, x, BUMP_STEP);
    PrintSection("(b) Bump Jacobian  J_bump = d(modelRate_i) / d(logDF_free_k)");
    PrintMatrix("", jBump, "rows = instruments, cols = free params; central diff h = 1e-6", maturities, freeKnots);

    // ---- (c) AAD Jacobian (analytic reverse sweep, read from the calibration diagnostics) ----
    // jacobian_ is populated by the solver's convergence-branch hook on the EXACT + ANALYTIC +
    // eligible path via a single func.Gradient(xNew, fNew) call at the solved x -- the same point
    // the bump oracle evaluates -- so the AAD-vs-bump comparison below cross-checks the library's
    // analytic Jacobian against an independent finite-difference oracle rather than a re-evaluation
    // of the same code.
    const Matrix_<>& jAad = result.diagnostics_.jacobian_;
    THROW_REQUIRE(!jAad.Empty(), "diagnostics_.jacobian_ is empty -- ANALYTIC path did not engage (ineligible spec?)");
    // B2 sentinel: every row must have at least one non-trivial entry. An all-zero row means the
    // tape never learned an input is an independent (the canonical missed-RegisterIndependent /
    // wrong-recording-order failure). Catch it before the FD comparison.
    AssertNoAllZeroRows(jAad);
    PrintSection("(c) AAD Jacobian  J_aad = d(modelRate_i) / d(logDF_free_k)");
    PrintMatrix("", jAad, "rows = instruments, cols = free params; reverse sweep, 1 per row", maturities, freeKnots);

    // ---- (d) AAD-vs-bump agreement (verbatim two-branch form, tol = 1e-9) ----
    PrintSection("(d) AAD-vs-bump agreement  (verbatim two-branch form, tol = 1e-9)");
    const AgreementResult_ agree = RunAgreementCheck(jBump, jAad);
    std::cout << std::fixed << std::setprecision(12);
    std::cout << "  max abs discrepancy : " << agree.maxAbs << "\n";
    std::cout << "  max rel discrepancy : " << agree.maxRel << "\n";
    std::cout << "  Verdict              : PASS  (rel <= 1e-9)\n";

    // ---- (e) effJacobianInverse_ from the EXACT solve ----
    const Matrix_<>& effJacobianInverse = result.diagnostics_.effJacobianInverse_;
    THROW_REQUIRE(effJacobianInverse.Rows() == nFree && effJacobianInverse.Cols() == nInst,
                  "effJacobianInverse_ shape must be nFreeParams x nInstruments (populated only by EXACT solve)");
    PrintSection("(e) effJacobianInverse_  d(params) * tolerance_ / d(decimal-rate perturbation)");
    PrintMatrix("", effJacobianInverse, "rows = free params, cols = instruments; solver-scaled pseudoinverse", freeKnots, maturities);

    // ---- (f) Portfolio parameter sensitivity g ----
    const Vector_<> g = PortfolioParamSensitivity(spec, x, BUMP_STEP);
    PrintSection("(f) Portfolio parameter-sensitivity  g = d(modelParRate_portfolio) / d(logDF_free_k)");
    std::cout << "  portfolio = swap " << PORTFOLIO_INDEX << " (maturity " << IsoDate(maturities[PORTFOLIO_INDEX]) << "); length = nFree = " << g.size()
              << "\n";
    std::cout << "  units: par-rate per unit log-DF bump (annuity-scaled PV not exposed by YCInstrument_)\n";
    PrintFreeParamVector("", g, freeKnots);

    // ---- (g) Bucketed IR risk r = g^T * effJacobianInverse_ ----
    const Vector_<> r = TransformToQuoteRisk(g, effJacobianInverse, spec.tolerance_);
    PrintSection("(g) Bucketed IR risk  r = g^T * effJacobianInverse_");
    std::cout << "  length = nInstruments = " << r.size() << "\n";
    std::cout << "  units: par-rate per absolute decimal quote bump\n";
    PrintQuoteRisk(r, maturities);

    // ---- (h) FR6 inverse-Jacobian nonlinear re-solve sanity ----
    PrintSection("(h) FR6 inverse-Jacobian nonlinear re-solve sanity");
    std::cout << "  bump each marketRate by +1e-4, re-run CalibrateYieldCurve (ANALYTIC),\n";
    std::cout << "  compare true delta vs linear prediction effJacobianInverse_(k,i) * 1e-4 / tolerance_ (tol = " << RE_SOLVE_TOL << " rel)\n";
    // The solver scales each residual row by 1/tolerance_ before forming the pseudoinverse
    // (underdetermined.cpp XScaledFunc_::F divides residuals by tol; J() calls DivideRows(tol)).
    // So effJacobianInverse_(k,i) carries units d(params)/d(scaled residual), i.e.
    // d(params) * tolerance_ / d(decimal-rate perturbation). The linear prediction for a decimal
    // quote bump delta_m on instrument i is therefore effJacobianInverse_(k,i) * delta_m / tolerance_.
    // This was verified empirically against the nonlinear re-solve (it does NOT match a bare
    // effJacobianInverse_ * delta_m, which is off by the tolerance factor).
    RunFR6ReSolve(spec, options, effJacobianInverse, x, maturities, nInst, nFree);

    // ---- (i) Calibration elapsed time -- BUMPED vs ANALYTIC ----
    PrintSection("(i) Calibration elapsed time  (BUMPED vs ANALYTIC, mean over N runs)");
    RunCalibrationTimingComparison(spec);

    PrintBanner("All self-checks passed.");
    return 0;
}

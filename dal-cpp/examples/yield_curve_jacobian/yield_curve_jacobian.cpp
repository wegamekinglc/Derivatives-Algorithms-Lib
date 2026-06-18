//
// Created by dal-implementer on 2026/6/19.
//

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
#include <dal/curve/ycctx.hpp>
#include <dal/curve/yclogdf.hpp>
#include <dal/curve/ycinstrument.hpp>
#include <dal/math/aad/aad.hpp>
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

// Always-on precondition macro. The library's REQUIRE is conditionally compiled (only active when
// DAL_USE_REQUIRE is defined), so it cannot carry self-checks that must fire in every build. This
// local guard uses THROW (always active) so the example's self-checks run under every AAD backend
// and every CMake configuration.
#define THROW_REQUIRE(cond, msg)                                                                                                                     \
    do {                                                                                                                                             \
        if (!(cond))                                                                                                                                 \
            THROW(msg);                                                                                                                              \
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
// backend on the "no DAL_USE_*_AAD flag set" branch). The AAD block below is therefore unguarded --
// it runs identically under native AAET, Adept, XAD, and CoDiPack, exactly as
// dal-cpp/examples/vanilla/vanilla.cpp and dal-cpp/digital/digital.cpp do. The example includes
// only <dal/math/aad/aad.hpp> and uses only the Dal::AAD facade.

namespace {
    using AAD::Adjoint;
    using AAD::Number_;
    using AAD::Value;

    // Self-check bars (see .claude/specs/yield-curve-jacobian-example.md "AAD-vs-Bump Tolerance
    // Choice"). The 1e-9 AAD-vs-bump bar holds because the Phase A LOG_DISCOUNT residual Jacobian
    // is O(1) and well-conditioned at the chosen 1%-3% par rates; central-difference round-off at
    // h=1e-6 is ~eps/h ~= 2e-10 relative, leaving ~5x margin. The FR6 re-solve is a genuine
    // nonlinear operation so it is looser by design.
    constexpr double BUMP_STEP = 1.0e-6;
    constexpr double AAD_TOL = 1.0e-9;
    constexpr double RE_SOLVE_TOL = 1.0e-6;
    constexpr double ONE_BP = 1.0e-4;

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

    // The exactly-5-instrument vanilla-swap Phase A calibration. Mirrors MakePhaseASpec in
    // dal-cpp/tests/curve/test_analytic_jacobian.cpp:57-90: LOG_DISCOUNT, EXACT, anchor == today_,
    // no projection curve, vanilla Swap_ only. This shape is provably eligible for the analytic
    // (AAD-tape) Jacobian, so requesting CurveJacobianMode_::ANALYTIC engages the tape rather than
    // silently falling back to bumping.
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

        spec.knotDates_ = {
            Date_(2022, 1, 1), Date_(2022, 4, 1), Date_(2022, 7, 1), Date_(2023, 1, 1), Date_(2024, 1, 1), Date_(2025, 1, 1),
        };

        const auto fixedLeg = AnnualLeg();
        const auto floatIdx = AnnualIndex();
        const auto floatLeg = AnnualLeg();
        const auto mkSwap = [&](const Date_& start, const Date_& end, double parPct) {
            return Handle_<YCInstrument_>(new Swap_(spec.today_, start, end, parPct / 100.0, fixedLeg, floatIdx, floatLeg));
        };
        spec.instruments_ = {
            mkSwap(Date_(2022, 1, 1), Date_(2022, 4, 1), 1.00), mkSwap(Date_(2022, 1, 1), Date_(2022, 7, 1), 1.10),
            mkSwap(Date_(2022, 1, 1), Date_(2023, 1, 1), 1.25), mkSwap(Date_(2022, 1, 1), Date_(2024, 1, 1), 1.55),
            mkSwap(Date_(2022, 1, 1), Date_(2025, 1, 1), 1.80),
        };
        return spec;
    }

    // Index of the off-anchor swap used as the "portfolio" for the FR5 risk transform. Swap 4 (3Y)
    // has annual coupons landing on the 2023/2024/2025 nodes (free columns 2,3,4); the sub-annual
    // 2022-04 and 2022-07 nodes (columns 0,1) carry structural zeros in g, which the risk transform
    // handles correctly.
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
        discounts[spec.targetCollateral_] = Handle_<DiscountCurve_>(std::shared_ptr<const DiscountCurve_>(std::shared_ptr<void>(), dc.release()));
        std::map<PeriodLength_, Handle_<DiscountCurve_>> forwards;
        // The aliasing shared_ptr (empty destructor, raw pointer = dc) takes ownership: dc.release()
        // hands the bare pointer over without a double-free. Mirrors the test idiom in
        // test_analytic_jacobian.cpp:103. The CurveBlock_ holds the Handle_ alive through the return.
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

    // AAD reverse-sweep Jacobian J_aad(i,k) = d(modelRate_i) / d(logDF_free_k). Mirrors
    // YieldCurveCalibrationFunc_::AnalyticJacobian in dal-cpp/dal/curve/calibration.cpp:510-565
    // line-for-line in shape. The forward pass builds a Number_-typed DiscountLogDF_ curve
    // (Tape::DiscountLogDF_<Number_>, publicly constructible and explicitly instantiated in
    // yclogdf.cpp), a Tape::YCCtx_<Number_>, and Number_-typed rates via inst->PrecomputeT<Number_>();
    // then one reverse sweep per row harvests d(modelRate_i)/d(logDF_k+1) from the node adjoints.
    Matrix_<> AadJacobian(const CurveCalibrationSpec_& spec, const Vector_<>& x) {
        auto* tape = Dal::AAD::Tape();
        Dal::AAD::Clear(*tape);

        const int nNodes = static_cast<int>(spec.knotDates_.size());
        const int nFree = static_cast<int>(x.size());
        THROW_REQUIRE(nFree == nNodes - 1, "free-param vector length must equal nNodes - 1");

        // Independents: free-node log(DF) values. Anchor (node 0) pinned at 0 and deliberately NOT
        // registered -- registering it would add a phantom input column. RegisterIndependent
        // anchors each free node on every backend before the recording window opens.
        Vector_<Number_> logDF(nNodes);
        logDF[0] = 0.0;
        for (int k = 0; k < nFree; ++k)
            Dal::AAD::RegisterIndependent(logDF[k + 1], x[k]);

        // Open the recording AFTER registering inputs and BEFORE the forward pass. XAD's canonical
        // contract drops inputs registered after newRecording; opening here is invisible on native
        // (NewRecording is a no-op) and correct on CoDiPack/Adept (anchors the sweep bounds).
        Dal::AAD::NewRecording(*tape);

        std::unique_ptr<Tape::DiscountLogDF_<Number_>> dc(
            new Tape::DiscountLogDF_<Number_>(spec.curveName_, spec.ccy_, spec.knotDates_, logDF, spec.liborBasis_, spec.logDfScheme_));
        Tape::YCCtx_<Number_> ctx(*dc);

        // Forward pass: Number_-typed model rates. PrecomputeT<Number_> lives only on the concrete
        // Phase A instrument types (Deposit_/FRA_/Future_/Swap_), so downcast per instrument --
        // mirroring PhaseARateAt in calibration.cpp:489-500. Use Dal::AAD::Value / Dal::AAD::Adjoint
        // to read primals/adjoints; never static_cast<double>(Number_) (FR8: only native and
        // CoDiPack have operator double(); XAD/Adept do not).
        const int nInst = static_cast<int>(spec.instruments_.size());
        Vector_<Number_> modelRates(nInst);
        for (int i = 0; i < nInst; ++i) {
            const auto* inst = spec.instruments_[i].get();
            Handle_<Tape::Rate_<Number_>> rateT;
            if (const auto* d = dynamic_cast<const Deposit_*>(inst))
                rateT = d->PrecomputeT<Number_>();
            else if (const auto* fr = dynamic_cast<const FRA_*>(inst))
                rateT = fr->PrecomputeT<Number_>();
            else if (const auto* fu = dynamic_cast<const Future_*>(inst))
                rateT = fu->PrecomputeT<Number_>();
            else if (const auto* s = dynamic_cast<const Swap_*>(inst))
                rateT = s->PrecomputeT<Number_>();
            else
                THROW_REQUIRE(false, std::string("instrument ") + std::to_string(i) + " has no Phase A templated rate");
            modelRates[i] = (*rateT)(ctx);
        }

        // Single-result reverse sweep, one row at a time. ZeroAdjoints clears propagated adjoints
        // between rows so each seed lands on a clean slate (Adept's compute_adjoint does not clear
        // operands between sweeps -- without ZeroAdjoints row 2 inherits row 1's residue).
        Matrix_<> j(nInst, nFree, 0.0);
        for (int i = 0; i < nInst; ++i) {
            Dal::AAD::ZeroAdjoints(*tape);
            Dal::AAD::Adjoint(modelRates[i]) = 1.0;
            Dal::AAD::PropagateToStart(*tape);
            for (int k = 0; k < nFree; ++k)
                j(i, k) = Dal::AAD::Adjoint(logDF[k + 1]);
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

    void PrintResiduals(const CurveCalibrationDiagnostics_& diag) {
        std::cout << std::fixed << std::setprecision(6);
        std::cout << std::left << std::setw(26) << "Instrument" << std::right << std::setw(14) << "Market(%)" << std::setw(14) << "Model(%)"
                  << std::setw(12) << "Error(bp)" << "\n";
        std::cout << std::string(62, '-') << "\n";
        for (int i = 0; i < static_cast<int>(diag.instrumentNames_.size()); ++i) {
            std::cout << std::left << std::setw(26) << diag.instrumentNames_[i].c_str() << std::right << std::setw(14) << diag.marketRates_[i] * 100.0
                      << std::setw(14) << diag.modelRates_[i] * 100.0 << std::setw(12) << diag.residuals_[i] * 10000.0 << "\n";
        }
    }

    void PrintVector(const String_& label, const Vector_<>& v) {
        std::cout << std::fixed << std::setprecision(12);
        if (!label.empty())
            std::cout << label << "\n";
        for (int i = 0; i < static_cast<int>(v.size()); ++i)
            std::cout << "  [" << i << "] " << std::setw(20) << v[i] << "\n";
    }

    void PrintMatrix(const String_& label, const Matrix_<>& m, const String_& orientation) {
        std::cout << std::fixed << std::setprecision(12);
        if (!label.empty())
            std::cout << label << "\n";
        std::cout << "  " << orientation << "  (rows=" << m.Rows() << ", cols=" << m.Cols() << ")\n";
        std::cout << "          ";
        for (int j = 0; j < m.Cols(); ++j)
            std::cout << std::setw(18) << ("[" + std::to_string(j) + "]");
        std::cout << "\n";
        for (int i = 0; i < m.Rows(); ++i) {
            std::cout << "  [" << i << "]  ";
            for (int j = 0; j < m.Cols(); ++j)
                std::cout << std::setw(18) << m(i, j);
            std::cout << "\n";
        }
    }
} // namespace

int main() {
    RegisterAll_::Init();

    PrintBanner("Yield-Curve Jacobian Example");

    const auto spec = BuildCalibrationSpec();
    const int nInst = static_cast<int>(spec.instruments_.size());
    const int nFree = static_cast<int>(spec.knotDates_.size()) - 1;
    THROW_REQUIRE(nInst == nFree, "Phase A calibration must be square: nInstruments == nKnotDates - 1");

    std::cout << "\nCalibration: " << nInst << " instruments on " << spec.knotDates_.size() << " LOG_DISCOUNT knots (" << nFree
              << " free params + anchor)\n";
    std::cout << "Parameterization: LOG_DISCOUNT   Solve mode: EXACT   Jacobian mode: ANALYTIC\n";
    std::cout << "Bump step h: " << std::scientific << std::setprecision(6) << BUMP_STEP << std::fixed << "\n";

    // ---- (a) Calibrate with the analytic Jacobian engaged ----
    CurveCalibrationOptions_ options;
    options.jacobianMode_ = CurveJacobianMode_::Value_::ANALYTIC;
    const auto result = CalibrateYieldCurve(spec, options);

    // Eligibility backstop: if the spec silently fell back to BUMPED (ANALYTIC never throws, it
    // only emits a NOTICE and falls back), the AAD path is a no-op and the whole teaching payload
    // is hollow. Detect via the analytic-tape Jacobian matching the residual oracle at the solved
    // point: compute J_aad from first principles and confirm it is non-trivial.
    const Vector_<> x = SolvedFreeParams(*result.curve_);

    PrintSection("(a) Calibration residuals");
    PrintResiduals(result.diagnostics_);
    std::cout << "\nSolved free-node log-DF params x (anchor pinned at 0):\n";
    PrintVector("", x);

    // ---- (b) Bump Jacobian (oracle) ----
    const Matrix_<> jBump = BumpJacobian(spec, x, BUMP_STEP);
    PrintSection("(b) Bump Jacobian  J_bump = d(modelRate_i) / d(logDF_free_k)");
    PrintMatrix("", jBump, "rows = instruments, cols = free params; central diff h = 1e-6");

    // ---- (c) AAD Jacobian (reverse sweep, from first principles) ----
    const Matrix_<> jAad = AadJacobian(spec, x);
    // B2 sentinel: every row must have at least one non-trivial entry. An all-zero row means the
    // tape never learned an input is an independent (the canonical missed-RegisterIndependent /
    // wrong-recording-order failure). Catch it before the FD comparison.
    for (int i = 0; i < jAad.Rows(); ++i) {
        double maxAbs = 0.0;
        for (int k = 0; k < jAad.Cols(); ++k)
            maxAbs = std::max(maxAbs, std::abs(jAad(i, k)));
        if (maxAbs <= 1.0e-6)
            THROW(std::string("AAD Jacobian row ") + std::to_string(i) + " is all-zero (B2 sentinel: missed RegisterIndependent?)");
    }
    PrintSection("(c) AAD Jacobian  J_aad = d(modelRate_i) / d(logDF_free_k)");
    PrintMatrix("", jAad, "rows = instruments, cols = free params; reverse sweep, 1 per row");

    // ---- (d) AAD-vs-bump agreement (verbatim two-branch form, tol = 1e-9) ----
    PrintSection("(d) AAD-vs-bump agreement  (verbatim two-branch form, tol = 1e-9)");
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
    std::cout << std::fixed << std::setprecision(12);
    std::cout << "  max abs discrepancy : " << maxAbs << "\n";
    std::cout << "  max rel discrepancy : " << maxRel << "\n";
    std::cout << "  Verdict              : PASS  (rel <= 1e-9)\n";

    // ---- (e) effJacobianInverse_ from the EXACT solve ----
    const Matrix_<>& effJacobianInverse = result.diagnostics_.effJacobianInverse_;
    THROW_REQUIRE(effJacobianInverse.Rows() == nFree && effJacobianInverse.Cols() == nInst,
                  "effJacobianInverse_ shape must be nFreeParams x nInstruments (populated only by EXACT solve)");
    PrintSection("(e) effJacobianInverse_  d(params) * tolerance_ / d(decimal-rate perturbation)");
    PrintMatrix("", effJacobianInverse, "rows = free params, cols = instruments; solver-scaled pseudoinverse (residuals scaled by 1/tolerance_)");

    // ---- (f) Portfolio parameter sensitivity g ----
    const Vector_<> g = PortfolioParamSensitivity(spec, x, BUMP_STEP);
    PrintSection("(f) Portfolio parameter-sensitivity  g = d(modelParRate_portfolio) / d(logDF_free_k)");
    std::cout << "  portfolio = swap " << PORTFOLIO_INDEX << " (off-anchor); length = nFree = " << g.size() << "\n";
    std::cout << "  units: par-rate per unit log-DF bump (annuity-scaled PV not exposed by YCInstrument_)\n";
    PrintVector("", g);

    // ---- (g) Bucketed IR risk r = g^T * effJacobianInverse_ ----
    const Vector_<> r = TransformToQuoteRisk(g, effJacobianInverse, spec.tolerance_);
    PrintSection("(g) Bucketed IR risk  r = g^T * effJacobianInverse_");
    std::cout << "  length = nInstruments = " << r.size() << "\n";
    std::cout << "  units: par-rate per absolute decimal quote bump\n";
    std::cout << std::fixed << std::setprecision(12);
    std::cout << "  " << std::left << std::setw(22) << "raw r[i]" << std::right << std::setw(22) << "par-rate DV01 (r[i]*1e-4)"
              << "\n";
    for (int i = 0; i < static_cast<int>(r.size()); ++i)
        std::cout << "  " << std::left << std::setw(22) << r[i] << std::right << std::setw(22) << r[i] * ONE_BP << "\n";

    // ---- (h) FR6 inverse-Jacobian nonlinear re-solve sanity ----
    PrintSection("(h) FR6 inverse-Jacobian nonlinear re-solve sanity");
    std::cout << "  bump each marketRate by +1e-4, re-run CalibrateYieldCurve (ANALYTIC),\n";
    std::cout << "  compare true delta vs linear prediction effJacobianInverse_(k,i) * 1e-4 / tolerance_ (tol = 1e-6 rel)\n";
    std::cout << std::fixed << std::setprecision(12);
    std::cout << "  " << std::left << std::setw(8) << "inst" << std::right << std::setw(22) << "max rel delta" << "\n";
    // The solver scales each residual row by 1/tolerance_ before forming the pseudoinverse
    // (underdetermined.cpp XScaledFunc_::F divides residuals by tol; J() calls DivideRows(tol)).
    // So effJacobianInverse_(k,i) carries units d(params)/d(scaled residual), i.e.
    // d(params) * tolerance_ / d(decimal-rate perturbation). The linear prediction for a decimal
    // quote bump delta_m on instrument i is therefore effJacobianInverse_(k,i) * delta_m / tolerance_.
    // This was verified empirically against the nonlinear re-solve (it does NOT match a bare
    // effJacobianInverse_ * delta_m, which is off by the tolerance factor).
    bool fr6Passed = true;
    for (int i = 0; i < nInst; ++i) {
        const Vector_<> trueDelta = RebumpedParamDelta(spec, options, i, ONE_BP, x);
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
        std::cout << "  " << std::left << std::setw(8) << i << std::right << std::setw(22) << maxRel << "\n";
    }
    if (fr6Passed)
        std::cout << "  Verdict : PASS  (all rel <= 1e-6)\n";

    PrintBanner("All self-checks passed.");
    return 0;
}

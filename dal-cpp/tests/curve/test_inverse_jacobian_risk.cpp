//
// Created by dal-tester on 2026/6/19.
//

#include <dal/curve/calibration.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/discount.hpp>
#include <dal/curve/ycinstrument.hpp>
#include <dal/curve/yclogdf.hpp>
#include <dal/math/matrix/matrixarithmetic.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/platform/platform.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/time/date.hpp>
#include <dal/time/daybasis.hpp>
#include <dal/time/holidays.hpp>
#include <dal/time/periodlength.hpp>
#include <dal/utilities/exceptions.hpp>
#include <gtest/gtest.h>

using namespace Dal;

// These tests gate the inverse-Jacobian IR-risk machinery that dal-cpp/examples/yield_curve_jacobian
// demonstrates but does NOT register in CTest (examples are plain executables with no add_test call,
// so the example's THROW-based self-checks never fire under `ctest`). The novel, subtle piece under
// test is the unit correction on diagnostics_.effJacobianInverse_: the underdetermined solver scales
// every residual row by 1/tolerance_ before forming the pseudoinverse
// (dal-cpp/dal/math/optimization/underdetermined.cpp -- XScaledFunc_::F divides residuals by tol,
// J() calls DivideRows(tol)), so effJacobianInverse_ carries units
// d(params) * tolerance_ / d(decimal-rate perturbation). Both the linear re-solve prediction and the
// bucketed-risk transform r = g^T * effJacobianInverse_ must therefore divide by tolerance_. If a
// future change drops that division, these tests fail. The example self-check is good documentation
// but is not a regression gate; this file is the gate.

namespace {
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

    // Square 5-instrument Phase A ladder (5 knots -> 5 free params + anchor). Mirrors the shape in
    // test_analytic_jacobian.cpp MakePhaseASpec so the analytic Jacobian is eligible and the EXACT
    // solve populates effJacobianInverse_.
    CurveCalibrationSpec_ MakeSquarePhaseASpec() {
        CurveCalibrationSpec_ spec;
        spec.today_ = Date_(2022, 1, 1);
        spec.ccy_ = "USD";
        spec.curveName_ = "inverse_jacobian_risk_test";
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

    CurveCalibrationOptions_ AnalyticOptions() {
        CurveCalibrationOptions_ opts;
        opts.jacobianMode_ = CurveJacobianMode_::Value_::ANALYTIC;
        return opts;
    }

    // Helper (not a test body, so no ASSERT_* macros): cast the solved curve to DiscountLogDF_ and
    // drop the pinned anchor. THROW on type mismatch -- always active, mirroring the example.
    Vector_<> SolvedFreeParams(const DiscountCurve_& curve, int nFree) {
        const auto* logDf = dynamic_cast<const DiscountLogDF_*>(&curve);
        if (logDf == nullptr)
            THROW("calibrated curve is not a DiscountLogDF_");
        const Vector_<> nodes = logDf->NodeLogDF();
        Vector_<> x(nFree);
        for (int i = 0; i < nFree; ++i)
            x[i] = nodes[i + 1];
        return x;
    }
} // namespace

// FR6 nonlinear re-solve: bump instrument i's market quote by +1bp, re-run CalibrateYieldCurve, and
// assert the linear prediction effJacobianInverse_(:,i) * 1e-4 / tolerance_ matches the true
// rebumped parameter delta to 1e-6 relative. The /tolerance_ factor is the unit correction on
// effJacobianInverse_ (the solver-scaled pseudoinverse); dropping it makes the prediction off by
// ~1e10. The 1e-6 bar reflects the genuine curvature of the nonlinear re-solve (per spec
// yield-curve-jacobian-example.md FR6). Runs on every AAD backend because the analytic Jacobian and
// the EXACT-solve pseudoinverse are backend-neutral.
TEST(InverseJacobianRiskTest, TestReSolveMatchesLinearPrediction) {
    const auto spec = MakeSquarePhaseASpec();
    const int nInst = static_cast<int>(spec.instruments_.size());
    const int nFree = static_cast<int>(spec.knotDates_.size()) - 1;
    ASSERT_EQ(nInst, nFree);

    const auto base = CalibrateYieldCurve(spec, AnalyticOptions());
    ASSERT_LT(base.diagnostics_.maxAbsResidual_, 1.0e-7);
    const Matrix_<>& effJacobianInverse = base.diagnostics_.effJacobianInverse_;
    ASSERT_EQ(effJacobianInverse.Rows(), nFree);
    ASSERT_EQ(effJacobianInverse.Cols(), nInst);

    const Vector_<> xBase = SolvedFreeParams(*base.curve_, nFree);
    const double bump = 1.0e-4;

    for (int i = 0; i < nInst; ++i) {
        CurveCalibrationSpec_ bumped = spec;
        const auto fixedLeg = AnnualLeg();
        const auto floatIdx = AnnualIndex();
        const auto floatLeg = AnnualLeg();
        const auto* swapOrig = dynamic_cast<const Swap_*>(spec.instruments_[i].get());
        ASSERT_NE(swapOrig, nullptr);
        const auto span = swapOrig->TimeSpan();
        bumped.instruments_[i] =
            Handle_<YCInstrument_>(new Swap_(spec.today_, span.first, span.second, swapOrig->MarketRate() + bump, fixedLeg, floatIdx, floatLeg));

        const auto res = CalibrateYieldCurve(bumped, AnalyticOptions());
        ASSERT_LT(res.diagnostics_.maxAbsResidual_, 1.0e-7);
        const Vector_<> xBumped = SolvedFreeParams(*res.curve_, nFree);

        for (int k = 0; k < nFree; ++k) {
            const double trueDelta = xBumped[k] - xBase[k];
            const double pred = effJacobianInverse(k, i) * bump / spec.tolerance_;
            const double rel = std::abs(pred - trueDelta) / std::max(1.0, std::abs(trueDelta));
            ASSERT_LE(rel, 1.0e-6) << "inst=" << i << " k=" << k << " pred=" << pred << " true=" << trueDelta;
        }
    }
}

// FR5 bucketed-risk transform: r = g^T * effJacobianInverse_ / tolerance_ is the linear risk number
// (par-rate per absolute decimal quote bump) for the off-anchor portfolio swap. The /tolerance_
// factor is the unit correction on effJacobianInverse_ (solver-scaled pseudoinverse; see the file
// header). The regression this catches: if the /tolerance_ division is dropped, every entry of r
// collapses to ~1e-10 and the diagonal-dominance + order-unity assertions below fail. We do NOT
// assert r[portfolioIdx] == 1 exactly: r is the LINEAR approximation of a nonlinear map, so a +1
// bump on the portfolio's own pillar produces r ~ 0.999 (curvature ~1e-3 at the +1 scale), not 1.
// The diagonal-dominance and order-unity bounds are tight enough to catch the missing-divide
// regression without asserting a false exact identity.
TEST(InverseJacobianRiskTest, TestBucketedRiskTransformAppliesToleranceCorrection) {
    const auto spec = MakeSquarePhaseASpec();
    const int nInst = static_cast<int>(spec.instruments_.size());
    const int nFree = static_cast<int>(spec.knotDates_.size()) - 1;

    const auto result = CalibrateYieldCurve(spec, AnalyticOptions());
    ASSERT_LT(result.diagnostics_.maxAbsResidual_, 1.0e-7);
    const Matrix_<>& effJacobianInverse = result.diagnostics_.effJacobianInverse_;
    const Vector_<> x = SolvedFreeParams(*result.curve_, nFree);

    // Portfolio == instrument nInst-1 (the 3Y off-anchor swap; its annual coupons land on the
    // 2023/2024/2025 nodes so g has non-trivial entries at columns 2,3,4).
    const int portfolioIdx = nInst - 1;
    const double h = 1.0e-6;
    Vector_<> g(nFree, 0.0);
    Handle_<YieldCurve_> empty;
    for (int k = 0; k < nFree; ++k) {
        Vector_<> full(spec.knotDates_.size(), 0.0);
        for (int j = 1; j < static_cast<int>(spec.knotDates_.size()); ++j)
            full[j] = x[j - 1];
        Vector_<> fullUp = full;
        Vector_<> fullDn = full;
        fullUp[k + 1] += h;
        fullDn[k + 1] -= h;
        std::unique_ptr<DiscountCurve_> dcUp(
            NewDiscountLogDF(spec.curveName_, spec.ccy_, spec.knotDates_, fullUp, spec.liborBasis_, spec.logDfScheme_));
        std::unique_ptr<DiscountCurve_> dcDn(
            NewDiscountLogDF(spec.curveName_, spec.ccy_, spec.knotDates_, fullDn, spec.liborBasis_, spec.logDfScheme_));
        std::map<CollateralType_, Handle_<DiscountCurve_>> up;
        std::map<CollateralType_, Handle_<DiscountCurve_>> dn;
        up[spec.targetCollateral_] = Handle_<DiscountCurve_>(std::shared_ptr<const DiscountCurve_>(std::shared_ptr<void>(), dcUp.get()));
        dn[spec.targetCollateral_] = Handle_<DiscountCurve_>(std::shared_ptr<const DiscountCurve_>(std::shared_ptr<void>(), dcDn.get()));
        std::map<PeriodLength_, Handle_<DiscountCurve_>> fwd;
        CurveBlock_ ycUp(spec.curveName_, spec.ccy_, up, fwd, spec.liborBasis_);
        CurveBlock_ ycDn(spec.curveName_, spec.ccy_, dn, fwd, spec.liborBasis_);
        const auto rateUp = spec.instruments_[portfolioIdx]->Precompute(empty);
        const auto rateDn = spec.instruments_[portfolioIdx]->Precompute(empty);
        g[k] = ((*rateUp)(ycUp) - (*rateDn)(ycDn)) / (2.0 * h);
        dcUp.release();
        dcDn.release();
    }

    Vector_<> rRaw;
    Dal::Matrix::Multiply(g, effJacobianInverse, &rRaw);
    for (auto& v : rRaw)
        v /= spec.tolerance_;

    // Order-unity diagonal: r[portfolioIdx] is the portfolio's par-rate risk to its own pillar. It
    // is ~1 (linear approximation of a +1 bump moving the par rate by ~+1); the 1e-2 band absorbs
    // the ~1e-3 linear-vs-nonlinear curvature. Dropping the /tolerance_ divide collapses this to
    // ~1e-10 and fails the lower bound immediately.
    ASSERT_GT(rRaw[portfolioIdx], 0.99) << "diagonal risk r[portfolioIdx]=" << rRaw[portfolioIdx] << " not order-unity";
    ASSERT_LT(rRaw[portfolioIdx], 1.01) << "diagonal risk r[portfolioIdx]=" << rRaw[portfolioIdx] << " not order-unity";

    // Diagonal dominance: the portfolio's own pillar carries the bulk of the risk; off-diagonals
    // (bucketed risk to other pillars) are at least an order of magnitude smaller.
    for (int j = 0; j < nInst; ++j) {
        if (j == portfolioIdx)
            continue;
        ASSERT_LT(std::abs(rRaw[j]), 1.0e-2) << "off-diagonal r[" << j << "]=" << rRaw[j] << " unexpectedly large";
    }
}

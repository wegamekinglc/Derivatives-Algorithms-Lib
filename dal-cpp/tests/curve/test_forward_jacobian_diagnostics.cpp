//
// Created by dal-implementer on 2026/6/19.
//

#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <dal/platform/platform.hpp>
#include <dal/curve/calibration.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/discount.hpp>
#include <dal/curve/yclogdf.hpp>
#include <dal/curve/ycinstrument.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/protocol/rateconvention.hpp>
#include <dal/time/date.hpp>
#include <dal/time/daybasis.hpp>
#include <dal/time/holidays.hpp>
#include <dal/time/periodlength.hpp>

using namespace Dal;

// Tests for the public forward-Jacobian diagnostics field CurveCalibrationDiagnostics_::jacobian_.
// The field is the unscaled analytic forward Jacobian d(modelRate_i) / d(logDF_free_k) evaluated at
// the calibrated solution by a single in-solver func.Gradient(xNew, fNew) call on convergence (one
// analytic-J evaluation at the solved x, deliberately NOT the solver's Broyden-perturbed iterate
// matrix, which is what effJacobianInverse_ is formed from). It is therefore distinct from
// effJacobianInverse_ -- a solver-weighted, tolerance-scaled pseudoinverse at the iterate -- and the
// two are NOT inverses in their exposed form. Populated iff solveMode_ == EXACT &&
// jacobianMode_ == ANALYTIC && eligible; default-constructed (empty, 0 x 0) otherwise.

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

    // Eligible Phase A spec: LOG_DISCOUNT + vanilla swaps starting at the anchor. Anchor pinned,
    // so nFreeParams = nKnots - 1 = 5, and nInstruments = 5 (square).
    CurveCalibrationSpec_ MakeEligibleSpec() {
        CurveCalibrationSpec_ spec;
        spec.today_ = Date_(2022, 1, 1);
        spec.ccy_ = "USD";
        spec.curveName_ = "forward_jacobian_eligible";
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

    // Ineligible spec: an instrument that does not trade at the curve anchor falls back to
    // bumped via NOTICE, independently of the now-supported curve representation.
    CurveCalibrationSpec_ MakeIneligibleSpec() {
        auto spec = MakeEligibleSpec();
        spec.instruments_[0] =
            Handle_<YCInstrument_>(new Swap_(Date_(2021, 12, 30), spec.today_, Date_(2022, 4, 1), 0.01, AnnualLeg(), AnnualIndex(), AnnualLeg()));
        return spec;
    }

    Vector_<> SolvedFreeParams(const DiscountCurve_& curve) {
        const auto* logDf = dynamic_cast<const DiscountLogDF_*>(&curve);
        REQUIRE(logDf != nullptr, "calibrated curve is not a DiscountLogDF_");
        Vector_<> nodes = logDf->NodeLogDF();
        nodes.erase(nodes.begin()); // drop the anchor (pinned at 0)
        return nodes;
    }

    // Independent two-sided central-difference Jacobian of the residual w.r.t. logDF_free at x.
    // d(residual)/d(logDF) == d(modelRate)/d(logDF) since marketRate is constant, so this oracle is
    // directly comparable to diagnostics_.jacobian_.
    Vector_<> EvalResiduals(const CurveCalibrationSpec_& spec, const Vector_<>& x) {
        Vector_<> full(spec.knotDates_.size(), 0.0);
        for (int i = 1; i < static_cast<int>(spec.knotDates_.size()); ++i)
            full[i] = x[i - 1];
        std::unique_ptr<DiscountCurve_> dc(NewDiscountLogDF(spec.curveName_, spec.ccy_, spec.knotDates_, full, spec.liborBasis_, spec.logDfScheme_));
        std::map<CollateralType_, Handle_<DiscountCurve_>> discounts;
        discounts[spec.targetCollateral_] = Handle_<DiscountCurve_>(std::shared_ptr<const DiscountCurve_>(std::shared_ptr<void>(), dc.get()));
        std::map<PeriodLength_, Handle_<DiscountCurve_>> forwards;
        CurveBlock_ yc(spec.curveName_, spec.ccy_, discounts, forwards, spec.liborBasis_);
        Vector_<> f(spec.instruments_.size());
        Handle_<YieldCurve_> empty;
        for (int i = 0; i < static_cast<int>(spec.instruments_.size()); ++i) {
            auto rate = spec.instruments_[i]->Precompute(empty);
            f[i] = (*rate)(yc)-spec.instruments_[i]->MarketRate();
        }
        return f;
    }
} // namespace

// Shape test: ANALYTIC + EXACT populates jacobian_ with shape nInstruments x (nKnots - 1).

TEST(ForwardJacobianDiagnosticsTest, TestShapeWhenAnalyticExactEligible) {
    const auto spec = MakeEligibleSpec();
    CurveCalibrationOptions_ opt;
    opt.jacobianMode_ = CurveJacobianMode_::Value_::ANALYTIC;

    const CurveCalibrationResult_ result = CalibrateYieldCurve(spec, opt);
    ASSERT_LT(result.diagnostics_.maxAbsResidual_, 1.0e-7);

    const Matrix_<>& j = result.diagnostics_.jacobian_;
    ASSERT_FALSE(j.Empty());
    ASSERT_EQ(j.Rows(), static_cast<int>(spec.instruments_.size()));
    ASSERT_EQ(j.Cols(), static_cast<int>(spec.knotDates_.size()) - 1);
}

// Agreement test: jacobian_ matches a two-sided central difference of the residual w.r.t. logDF_free
// at the solved x within 1e-9. Both the stored jacobian_ and the FD oracle evaluate at the solution,
// so this is a clean AAD-vs-FD agreement (no iterate-vs-solution gap -- jacobian_ is re-evaluated at
// the solved x, not read off the solver's iterate matrix).

TEST(ForwardJacobianDiagnosticsTest, TestMatchesCentralDifferenceAtSolvedX) {
    const auto spec = MakeEligibleSpec();
    CurveCalibrationOptions_ opt;
    opt.jacobianMode_ = CurveJacobianMode_::Value_::ANALYTIC;

    const CurveCalibrationResult_ result = CalibrateYieldCurve(spec, opt);
    ASSERT_LT(result.diagnostics_.maxAbsResidual_, 1.0e-7);

    const Matrix_<>& j = result.diagnostics_.jacobian_;
    ASSERT_EQ(j.Rows(), static_cast<int>(spec.instruments_.size()));
    ASSERT_EQ(j.Cols(), static_cast<int>(spec.knotDates_.size()) - 1);

    const Vector_<> x = SolvedFreeParams(*result.curve_);
    const double h = 1.0e-6;
    const double relTol = 1.0e-9;
    for (int c = 0; c < j.Cols(); ++c) {
        Vector_<> xUp = x;
        Vector_<> xDn = x;
        xUp[c] += h;
        xDn[c] -= h;
        const Vector_<> fUp = EvalResiduals(spec, xUp);
        const Vector_<> fDn = EvalResiduals(spec, xDn);
        for (int r = 0; r < j.Rows(); ++r) {
            const double fd = (fUp[r] - fDn[r]) / (2.0 * h);
            const double an = j(r, c);
            if (std::abs(fd) < relTol) {
                ASSERT_NEAR(an, 0.0, relTol) << "row=" << r << " col=" << c << " FD=" << fd;
            } else {
                ASSERT_NEAR(an, fd, relTol * std::max(1.0, std::abs(fd))) << "row=" << r << " col=" << c;
            }
        }
    }
}

// jacobian_ and effJacobianInverse_ have complementary shapes (nInst x nFree and nFree x nInst) and
// are co-populated on the EXACT + ANALYTIC path. They are NOT the same matrix and jacobian_ is NOT
// the inverse of effJacobianInverse_ in the exposed (unscaled vs tolerance-scaled) form; this test
// pins only their co-population and complementary shape.

TEST(ForwardJacobianDiagnosticsTest, TestCoherentWithEffJacobianInversePopulation) {
    const auto spec = MakeEligibleSpec();
    CurveCalibrationOptions_ opt;
    opt.jacobianMode_ = CurveJacobianMode_::Value_::ANALYTIC;

    const CurveCalibrationResult_ result = CalibrateYieldCurve(spec, opt);
    ASSERT_LT(result.diagnostics_.maxAbsResidual_, 1.0e-7);

    ASSERT_FALSE(result.diagnostics_.jacobian_.Empty());
    ASSERT_FALSE(result.diagnostics_.effJacobianInverse_.Empty());
    ASSERT_EQ(result.diagnostics_.jacobian_.Rows(), result.diagnostics_.effJacobianInverse_.Cols());
    ASSERT_EQ(result.diagnostics_.jacobian_.Cols(), result.diagnostics_.effJacobianInverse_.Rows());
}

TEST(ForwardJacobianDiagnosticsTest, TestCanSkipEffJacobianInverse) {
    const auto spec = MakeEligibleSpec();
    CurveCalibrationOptions_ opt;
    opt.jacobianMode_ = CurveJacobianMode_::Value_::ANALYTIC;
    opt.computeEffJacobianInverse_ = false;

    const CurveCalibrationResult_ result = CalibrateYieldCurve(spec, opt);
    ASSERT_LT(result.diagnostics_.maxAbsResidual_, 1.0e-7);
    ASSERT_TRUE(result.diagnostics_.effJacobianInverse_.Empty());
    ASSERT_FALSE(result.diagnostics_.jacobian_.Empty());
}

TEST(ForwardJacobianDiagnosticsTest, TestCanSkipForwardJacobian) {
    const auto spec = MakeEligibleSpec();
    CurveCalibrationOptions_ opt;
    opt.jacobianMode_ = CurveJacobianMode_::Value_::ANALYTIC;
    opt.computeForwardJacobian_ = false;

    const CurveCalibrationResult_ result = CalibrateYieldCurve(spec, opt);
    ASSERT_LT(result.diagnostics_.maxAbsResidual_, 1.0e-7);
    ASSERT_FALSE(result.diagnostics_.effJacobianInverse_.Empty());
    ASSERT_TRUE(result.diagnostics_.jacobian_.Empty());
}

// Empty-field regression: APPROXIMATE solve leaves jacobian_ empty.

TEST(ForwardJacobianDiagnosticsTest, TestEmptyWhenApproximateSolve) {
    auto spec = MakeEligibleSpec();
    spec.solveMode_ = CurveSolveMode_::Value_::APPROXIMATE;
    spec.fitTolerance_ = 1e-4;
    CurveCalibrationOptions_ opt;
    opt.jacobianMode_ = CurveJacobianMode_::Value_::ANALYTIC;

    const CurveCalibrationResult_ result = CalibrateYieldCurve(spec, opt);
    ASSERT_TRUE(result.diagnostics_.usedApproximateFit_);
    ASSERT_TRUE(result.diagnostics_.jacobian_.Empty());
}

// Empty-field regression: EXACT solve + BUMPED leaves jacobian_ empty.

TEST(ForwardJacobianDiagnosticsTest, TestEmptyWhenBumpedDefault) {
    const auto spec = MakeEligibleSpec();
    CurveCalibrationOptions_ opt;
    opt.jacobianMode_ = CurveJacobianMode_::Value_::BUMPED;

    const CurveCalibrationResult_ result = CalibrateYieldCurve(spec, opt);
    ASSERT_TRUE(result.diagnostics_.jacobian_.Empty());
}

// B2 regression: EXACT && BUMPED. effJacobianInverse_ IS populated, but jacobian_ must be EMPTY --
// the two fields have different population conditions by design (only the analytic path produces a
// well-defined forward J).

TEST(ForwardJacobianDiagnosticsTest, TestEmptyWhenExactBumpedButEffJacobianInversePopulated) {
    const auto spec = MakeEligibleSpec();
    CurveCalibrationOptions_ opt;
    opt.jacobianMode_ = CurveJacobianMode_::Value_::BUMPED;

    const CurveCalibrationResult_ result = CalibrateYieldCurve(spec, opt);
    ASSERT_FALSE(result.diagnostics_.usedApproximateFit_);
    // effJacobianInverse_ is populated on EXACT regardless of jacobianMode_.
    ASSERT_FALSE(result.diagnostics_.effJacobianInverse_.Empty());
    // jacobian_ is NOT well-defined for the bumped J source -- must be empty by the explicit guard.
    ASSERT_TRUE(result.diagnostics_.jacobian_.Empty());
}

// Empty-field regression: ANALYTIC on an ineligible spec falls back to bumped, so jacobian_ is
// empty (no analytic J was ever produced).

TEST(ForwardJacobianDiagnosticsTest, TestEmptyWhenAnalyticIneligibleFallback) {
    const auto spec = MakeIneligibleSpec();
    CurveCalibrationOptions_ opt;
    opt.jacobianMode_ = CurveJacobianMode_::Value_::ANALYTIC;

    const CurveCalibrationResult_ result = CalibrateYieldCurve(spec, opt);
    ASSERT_NE(result.curve_, nullptr);
    ASSERT_TRUE(result.diagnostics_.jacobian_.Empty());
}

// Multi-curve stage check: CalibrateMultiCurve routes every stage through CalibrateYieldCurve with
// default (ANALYTIC) options, so every eligible stage's jacobian_ is populated while
// effJacobianInverse_ is also populated where the stage solved EXACT.

TEST(ForwardJacobianDiagnosticsTest, TestMultiCurveStagesMatchMode) {
    auto mkStage = [](const String_& name) {
        CurveCalibrationSpec_ stage = MakeEligibleSpec();
        stage.curveName_ = name;
        stage.calibrateDiscountCurve_ = true;
        stage.targetCollateral_ = CollateralType_(CollateralType_::Value_::OIS);
        return stage;
    };

    MultiCurveCalibrationSpec_ multi;
    multi.name_ = "fwd_diag_multi";
    multi.ccy_ = "USD";
    multi.stages_ = {mkStage("stage_a"), mkStage("stage_b")};

    const MultiCurveCalibrationResult_ result = CalibrateMultiCurve(multi);
    ASSERT_EQ(result.diagnostics_.size(), multi.stages_.size());

    for (int i = 0; i < static_cast<int>(result.diagnostics_.size()); ++i) {
        const auto& d = result.diagnostics_[i];
        // Default options = ANALYTIC: eligible stages populate jacobian_ (EXACT && ANALYTIC && eligible).
        ASSERT_FALSE(d.jacobian_.Empty()) << "stage " << i << " jacobian_ should be populated under default ANALYTIC options";
        // EXACT solve (default) still populates effJacobianInverse_.
        ASSERT_FALSE(d.effJacobianInverse_.Empty()) << "stage " << i << " effJacobianInverse_ should be populated by EXACT solve";
    }
}

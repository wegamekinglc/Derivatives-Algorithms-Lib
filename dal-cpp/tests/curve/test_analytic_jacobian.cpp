//
// Created by dal-implementer on 2026/6/15.
//

#include <gtest/gtest.h>

#include <cmath>
#include <dal/currency/currency.hpp>
#include <dal/curve/calibration.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/curveparameterization.hpp>
#include <dal/curve/discount.hpp>
#include <dal/curve/piecewiselinear.hpp>
#include <dal/curve/ycconst.hpp>
#include <dal/curve/ycimp.hpp>
#include <dal/curve/ycinstrument.hpp>
#include <dal/curve/yclogdf.hpp>
#include <dal/curve/ycpwlf.hpp>
#include <dal/curve/yczerorate.hpp>
#include <dal/platform/platform.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/protocol/rateconvention.hpp>
#include <dal/time/date.hpp>
#include <dal/time/daybasis.hpp>
#include <dal/time/holidays.hpp>
#include <dal/time/periodlength.hpp>
#include <map>
#include <memory>

using namespace Dal;

// The analytic Jacobian is backend-neutral: it runs the single-result reverse-sweep loop under
// every AAD backend (native, XAD, CoDiPack, Adept) via the Dal::AAD facade (RegisterIndependent,
// ZeroAdjoints, Adjoint, PropagateToStart). Every test below runs on every backend; there is no
// skip machinery. The eligibility/fallback tests still assert an empty diagnostics_.jacobian_
// for ineligible calibrations -- that is the fallback behavior, not a backend skip.
//
// The forward analytic Jacobian is obtained ONLY as a byproduct of calibration: every test
// calibrates with CurveJacobianMode_::ANALYTIC and reads result.diagnostics_.jacobian_ (the
// unscaled analytic J at the solved free-node log-DFs). There is no standalone "analytic J at a
// point" accessor.

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

    // 5-instrument vanilla-swap ladder on LOG_DISCOUNT. Anchor == today_, every swap starts at the
    // anchor, no projection curves, vanilla Swap_ only -- the Phase A eligible shape.
    CurveCalibrationSpec_ MakePhaseASpec(LogDfScheme_ scheme = LogDfScheme_::Value_::LOG_LINEAR) {
        CurveCalibrationSpec_ spec;
        spec.today_ = Date_(2022, 1, 1);
        spec.ccy_ = "USD";
        spec.curveName_ = "phase_a_test";
        spec.parameterization_ = CurveParameterization_::Value_::LOG_DISCOUNT;
        spec.knotPolicy_ = CurveKnotPolicy_::Value_::INPUT;
        spec.solveMode_ = CurveSolveMode_::Value_::EXACT;
        spec.liborBasis_ = DayBasis_("ACT_365F");
        spec.tolerance_ = 1.0e-10;
        spec.fitTolerance_ = 1.0e-8;
        spec.smoothingWeight_ = 1.0;
        spec.logDfScheme_ = scheme;

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

    CurveCalibrationSpec_ MakeZeroRatePhaseASpec(LogDfScheme_ scheme) {
        CurveCalibrationSpec_ spec = MakePhaseASpec(scheme);
        spec.curveName_ = "zero_rate_phase_a_test";
        spec.parameterization_ = CurveParameterization_::Value_::ZERO_RATE;
        spec.knotDates_ = Vector_<Date_>(spec.knotDates_.begin() + 1, spec.knotDates_.end());
        spec.initialGuess_ = 0.02;
        // Exercise interpolation weights as well as node mapping: this maturity lies between
        // the 3M and 6M knots for every scheme.
        spec.instruments_[0] =
            Handle_<YCInstrument_>(new Swap_(spec.today_, spec.today_, Date_(2022, 5, 1), 0.0105, AnnualLeg(), AnnualIndex(), AnnualLeg()));
        return spec;
    }

    CurveCalibrationSpec_ MakeForwardParameterizationSpec(CurveParameterization_ parameterization) {
        CurveCalibrationSpec_ spec;
        spec.today_ = Date_(2024, 1, 15);
        spec.ccy_ = "USD";
        spec.curveName_ = "forward_parameterization_test";
        spec.parameterization_ = parameterization;
        spec.knotPolicy_ = CurveKnotPolicy_::Value_::INPUT;
        spec.solveMode_ = CurveSolveMode_::Value_::EXACT;
        spec.liborBasis_ = DayBasis_("ACT_365F");
        spec.tolerance_ = 1.0e-10;
        spec.fitTolerance_ = 1.0e-8;
        spec.smoothingWeight_ = 1.0;
        spec.initialGuess_ = 0.02;
        spec.knotDates_ = {Date::AddMonths(spec.today_, 3)};
        const double flatRate = 0.015;
        spec.instruments_.push_back(Handle_<YCInstrument_>(new Deposit_(spec.today_, spec.knotDates_[0], flatRate, spec.liborBasis_)));
        return spec;
    }

    CurveCalibrationSpec_ MakeMultiKnotPiecewiseLinearSpec() {
        CurveCalibrationSpec_ spec;
        spec.today_ = Date_(2024, 1, 15);
        spec.ccy_ = "USD";
        spec.curveName_ = "multi_knot_pwl_test";
        spec.parameterization_ = CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD;
        spec.knotPolicy_ = CurveKnotPolicy_::Value_::INPUT;
        spec.solveMode_ = CurveSolveMode_::Value_::EXACT;
        spec.liborBasis_ = DayBasis_("ACT_365F");
        spec.tolerance_ = 1.0e-10;
        spec.fitTolerance_ = 1.0e-8;
        spec.smoothingWeight_ = 1.0;
        spec.initialGuess_ = 0.02;
        spec.knotDates_ = {
            Date::AddMonths(spec.today_, 3),
            Date::AddMonths(spec.today_, 12),
            Date::AddMonths(spec.today_, 24),
            Date::AddMonths(spec.today_, 36),
        };

        const Vector_<> left = {0.011, 0.017, 0.025, 0.032};
        const Vector_<> right = {0.014, 0.021, 0.030, 0.035};
        const Handle_<DiscountCurve_> market(NewDiscountPWLF("multi_knot_pwl_market", spec.ccy_, PiecewiseLinear_(spec.knotDates_, left, right)));
        CurveBlock_ marketBlock(spec.curveName_, spec.ccy_, {{spec.targetCollateral_, market}}, {}, spec.liborBasis_);
        const Vector_<Date_> maturities = {
            Date::AddMonths(spec.today_, 1),  Date::AddMonths(spec.today_, 6),  Date::AddMonths(spec.today_, 9),
            Date::AddMonths(spec.today_, 15), Date::AddMonths(spec.today_, 18), Date::AddMonths(spec.today_, 30),
        };
        Handle_<YieldCurve_> empty;
        for (const auto& maturity : maturities) {
            const Handle_<YCInstrument_> prototype(new Deposit_(spec.today_, maturity, 0.0, spec.liborBasis_));
            const double marketRate = (*prototype->Precompute(empty))(marketBlock);
            spec.instruments_.push_back(Handle_<YCInstrument_>(new Deposit_(spec.today_, maturity, marketRate, spec.liborBasis_)));
        }
        return spec;
    }

    // Run an ANALYTIC calibration and return the result. The forward J lands on
    // diagnostics_.jacobian_ when the calibration is admitted (ANALYTIC && EXACT && eligible);
    // otherwise diagnostics_.jacobian_ is empty (the gate rejected or fell back to bumped).
    CurveCalibrationResult_ CalibrateAnalytic(const CurveCalibrationSpec_& spec) {
        CurveCalibrationOptions_ opt;
        opt.jacobianMode_ = CurveJacobianMode_::Value_::ANALYTIC;
        return CalibrateYieldCurve(spec, opt);
    }

    CurveCalibrationResult_ CalibrateBumped(const CurveCalibrationSpec_& spec) {
        CurveCalibrationOptions_ opt;
        opt.jacobianMode_ = CurveJacobianMode_::Value_::BUMPED;
        return CalibrateYieldCurve(spec, opt);
    }

    void AssertCurveAgreesWithBumped(const CurveCalibrationSpec_& spec, const CurveCalibrationResult_& analytic, double tolerance = 1.0e-8) {
        const CurveCalibrationResult_ bumped = CalibrateBumped(spec);
        ASSERT_LT(bumped.diagnostics_.maxAbsResidual_, 1.0e-7);
        for (const auto& knot : spec.knotDates_)
            ASSERT_NEAR((*analytic.curve_)(spec.today_, knot), (*bumped.curve_)(spec.today_, knot), tolerance);
        for (const auto& instrument : spec.instruments_) {
            const Date_ maturity = instrument->TimeSpan().second;
            ASSERT_NEAR((*analytic.curve_)(spec.today_, maturity), (*bumped.curve_)(spec.today_, maturity), tolerance)
                << "instrument=" << instrument->Name();
        }
    }

    // Solved free-node log-DFs: the calibrated DiscountLogDF_ node log-DFs with the pinned anchor
    // (node 0 == 0) dropped, matching the solver's x-vector layout (nKnots - 1 entries).
    Vector_<> SolvedFreeParams(const DiscountCurve_& curve) {
        const auto* logDf = dynamic_cast<const DiscountLogDF_*>(&curve);
        REQUIRE(logDf != nullptr, "calibrated curve is not a DiscountLogDF_");
        Vector_<> nodes = logDf->NodeLogDF();
        nodes.erase(nodes.begin()); // drop the anchor (pinned at 0)
        return nodes;
    }

    // Independent residual-vector evaluator for the same calibration set. Used to compute a
    // two-sided central-difference Jacobian that the AAD-tape Jacobian must match within 1e-9.
    // Mirrors the F() body in calibration.cpp for LOG_DISCOUNT + vanilla swap.
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

    // Calibrate with ANALYTIC, then compare diagnostics_.jacobian_ (the analytic J at the solved
    // free-node log-DFs) to a two-sided central difference of EvalResiduals at the solution. Every
    // entry must match within relTol (scaled by the FD magnitude). Asserts the byproduct J is
    // populated with the expected nInstruments x (nKnots - 1) shape first.
    void AssertJacobianMatchesCentralDifferenceAtSolution(const CurveCalibrationSpec_& spec,
                                                          double h,
                                                          double relTol,
                                                          CurveCalibrationResult_* outResult = nullptr) {
        CurveCalibrationResult_ result = CalibrateAnalytic(spec);
        ASSERT_LT(result.diagnostics_.maxAbsResidual_, 1.0e-7);

        const Matrix_<>& J = result.diagnostics_.jacobian_;
        const int nRows = static_cast<int>(spec.instruments_.size());
        const int nCols = static_cast<int>(spec.knotDates_.size()) - 1;
        ASSERT_FALSE(J.Empty());
        ASSERT_EQ(J.Rows(), nRows);
        ASSERT_EQ(J.Cols(), nCols);

        const Vector_<> x = SolvedFreeParams(*result.curve_);
        ASSERT_EQ(static_cast<int>(x.size()), nCols);
        for (int c = 0; c < nCols; ++c) {
            Vector_<> xUp = x;
            Vector_<> xDn = x;
            xUp[c] += h;
            xDn[c] -= h;
            const Vector_<> fUp = EvalResiduals(spec, xUp);
            const Vector_<> fDn = EvalResiduals(spec, xDn);
            for (int r = 0; r < nRows; ++r) {
                const double fd = (fUp[r] - fDn[r]) / (2.0 * h);
                const double an = J(r, c);
                if (std::abs(fd) < relTol) {
                    ASSERT_NEAR(an, 0.0, relTol) << "row=" << r << " col=" << c << " FD=" << fd;
                } else {
                    ASSERT_NEAR(an, fd, relTol * std::max(1.0, std::abs(fd))) << "row=" << r << " col=" << c;
                }
            }
        }
        AssertCurveAgreesWithBumped(spec, result);
        if (outResult)
            *outResult = std::move(result);
    }

    Vector_<> SolvedForwardParameters(const DiscountCurve_& curve, CurveParameterization_ parameterization) {
        if (parameterization == CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD) {
            const auto* pwc = dynamic_cast<const Tape::DiscountPWC_<double>*>(&curve);
            REQUIRE(pwc != nullptr, "calibrated curve is not a DiscountPWC_");
            return pwc->FRight();
        }
        const auto* pwl = dynamic_cast<const Tape::DiscountPWLF_<double>*>(&curve);
        REQUIRE(pwl != nullptr, "calibrated curve is not a DiscountPWLF_");
        const Vector_<> left = pwl->FLeft();
        const Vector_<> right = pwl->FRight();
        Vector_<> result(2 * left.size());
        for (int i = 0; i < static_cast<int>(left.size()); ++i) {
            result[2 * i] = left[i];
            result[2 * i + 1] = right[i];
        }
        return result;
    }

    Vector_<> EvalForwardResiduals(const CurveCalibrationSpec_& spec, const Vector_<>& parameters) {
        const CurveDefinition_ definition = MakeCurveDefinition(spec.curveName_, spec.ccy_, spec.parameterization_, spec.logDfScheme_,
                                                                spec.knotDates_, spec.today_, spec.liborBasis_);
        auto curve = BuildDiscountCurveUniqueT<double>(definition, parameters, spec.baseCurve_);
        std::map<CollateralType_, Handle_<DiscountCurve_>> discounts;
        discounts[spec.targetCollateral_] = Handle_<DiscountCurve_>(std::shared_ptr<const DiscountCurve_>(std::shared_ptr<void>(), curve.get()));
        CurveBlock_ block(spec.curveName_, spec.ccy_, discounts, {}, spec.liborBasis_);
        Vector_<> residuals(spec.instruments_.size());
        Handle_<YieldCurve_> empty;
        for (int i = 0; i < static_cast<int>(spec.instruments_.size()); ++i)
            residuals[i] = (*spec.instruments_[i]->Precompute(empty))(block)-spec.instruments_[i]->MarketRate();
        return residuals;
    }

    void AssertForwardJacobianMatchesCentralDifference(const CurveCalibrationSpec_& spec, double bump = 1.0e-6, double tolerance = 1.0e-9) {
        const CurveCalibrationResult_ result = CalibrateAnalytic(spec);
        ASSERT_LT(result.diagnostics_.maxAbsResidual_, 1.0e-7);
        const CurveDefinition_ definition = MakeCurveDefinition(spec.curveName_, spec.ccy_, spec.parameterization_, spec.logDfScheme_,
                                                                spec.knotDates_, spec.today_, spec.liborBasis_);
        const int columns = BuildCurveParameterLayout(definition).parameterCount_;
        ASSERT_EQ(result.diagnostics_.jacobian_.Rows(), static_cast<int>(spec.instruments_.size()));
        ASSERT_EQ(result.diagnostics_.jacobian_.Cols(), columns);
        const Vector_<> solved = SolvedForwardParameters(*result.curve_, spec.parameterization_);
        for (int column = 0; column < columns; ++column) {
            auto up = solved;
            auto down = solved;
            up[column] += bump;
            down[column] -= bump;
            const Vector_<> upResiduals = EvalForwardResiduals(spec, up);
            const Vector_<> downResiduals = EvalForwardResiduals(spec, down);
            for (int row = 0; row < static_cast<int>(spec.instruments_.size()); ++row) {
                const double centralDifference = (upResiduals[row] - downResiduals[row]) / (2.0 * bump);
                ASSERT_NEAR(result.diagnostics_.jacobian_(row, column), centralDifference, tolerance) << "row=" << row << ", column=" << column;
            }
        }
        AssertCurveAgreesWithBumped(spec, result);
    }

    Vector_<> EvalZeroRateResiduals(const CurveCalibrationSpec_& spec, const Vector_<>& parameters) {
        const CurveDefinition_ definition = MakeCurveDefinition(spec.curveName_, spec.ccy_, spec.parameterization_, spec.logDfScheme_,
                                                                spec.knotDates_, spec.today_, spec.liborBasis_);
        auto curve = BuildDiscountCurveUniqueT<double>(definition, parameters, spec.baseCurve_);
        std::map<CollateralType_, Handle_<DiscountCurve_>> discounts;
        discounts[spec.targetCollateral_] = Handle_<DiscountCurve_>(std::shared_ptr<const DiscountCurve_>(std::shared_ptr<void>(), curve.get()));
        CurveBlock_ block(spec.curveName_, spec.ccy_, discounts, {}, spec.liborBasis_);
        Vector_<> residuals(spec.instruments_.size());
        Handle_<YieldCurve_> empty;
        for (int i = 0; i < static_cast<int>(spec.instruments_.size()); ++i)
            residuals[i] = (*spec.instruments_[i]->Precompute(empty))(block)-spec.instruments_[i]->MarketRate();
        return residuals;
    }

    void AssertZeroRateJacobianMatchesCentralDifference(LogDfScheme_ scheme) {
        const CurveCalibrationSpec_ spec = MakeZeroRatePhaseASpec(scheme);
        const CurveCalibrationResult_ analytic = CalibrateAnalytic(spec);
        const CurveCalibrationResult_ bumped = CalibrateBumped(spec);
        ASSERT_LT(analytic.diagnostics_.maxAbsResidual_, 1.0e-7);
        ASSERT_LT(bumped.diagnostics_.maxAbsResidual_, 1.0e-7);

        const auto* analyticCurve = dynamic_cast<const DiscountZeroRate_*>(analytic.curve_.get());
        const auto* bumpedCurve = dynamic_cast<const DiscountZeroRate_*>(bumped.curve_.get());
        ASSERT_NE(analyticCurve, nullptr);
        ASSERT_NE(bumpedCurve, nullptr);
        const Vector_<> parameters = analyticCurve->NodeZeroRates();
        const Vector_<> bumpedParameters = bumpedCurve->NodeZeroRates();
        ASSERT_EQ(parameters.size(), spec.knotDates_.size());
        ASSERT_EQ(bumpedParameters.size(), parameters.size());

        const Matrix_<>& jacobian = analytic.diagnostics_.jacobian_;
        ASSERT_FALSE(jacobian.Empty());
        ASSERT_EQ(jacobian.Rows(), static_cast<int>(spec.instruments_.size()));
        ASSERT_EQ(jacobian.Cols(), static_cast<int>(parameters.size()));
        ASSERT_FALSE(analytic.diagnostics_.effJacobianInverse_.Empty());
        ASSERT_EQ(analytic.diagnostics_.effJacobianInverse_.Rows(), jacobian.Cols());
        ASSERT_EQ(analytic.diagnostics_.effJacobianInverse_.Cols(), jacobian.Rows());

        constexpr double bump = 1.0e-6;
        constexpr double tolerance = 2.0e-8;
        for (int column = 0; column < static_cast<int>(parameters.size()); ++column) {
            Vector_<> up = parameters;
            Vector_<> down = parameters;
            up[column] += bump;
            down[column] -= bump;
            const Vector_<> upResiduals = EvalZeroRateResiduals(spec, up);
            const Vector_<> downResiduals = EvalZeroRateResiduals(spec, down);
            for (int row = 0; row < jacobian.Rows(); ++row) {
                const double centralDifference = (upResiduals[row] - downResiduals[row]) / (2.0 * bump);
                ASSERT_NEAR(jacobian(row, column), centralDifference, tolerance) << "row=" << row << ", column=" << column;
            }
            ASSERT_NEAR(parameters[column], bumpedParameters[column], 1.0e-8) << "column=" << column;
        }
        for (int row = 0; row < static_cast<int>(analytic.diagnostics_.residuals_.size()); ++row)
            ASSERT_NEAR(analytic.diagnostics_.residuals_[row], bumped.diagnostics_.residuals_[row], 1.0e-8) << "row=" << row;
        for (const auto& knot : spec.knotDates_)
            ASSERT_NEAR((*analytic.curve_)(spec.today_, knot), (*bumped.curve_)(spec.today_, knot), 1.0e-8);
        for (const auto& instrument : spec.instruments_) {
            const Date_ maturity = instrument->TimeSpan().second;
            ASSERT_NEAR((*analytic.curve_)(spec.today_, maturity), (*bumped.curve_)(spec.today_, maturity), 1.0e-8)
                << "instrument=" << instrument->Name();
        }
    }
} // namespace

// Category 1: AAD-tape Jacobian matches two-sided central differences
// The Phase A override returns dModelRate_i/dx_j via one reverse sweep per row. The byproduct J
// (diagnostics_.jacobian_) is evaluated at the solved x, so the FD oracle runs at the same point:
// a clean AAD-vs-FD agreement with no iterate-vs-solution gap.

TEST(AnalyticJacobianTest, TestMatchesCentralDifferenceLogLinear) {
    auto spec = MakePhaseASpec(LogDfScheme_::Value_::LOG_LINEAR);
    AssertJacobianMatchesCentralDifferenceAtSolution(spec, 1.0e-6, 1.0e-9);
}

TEST(AnalyticJacobianTest, TestLogLinearOffKnotMaturityIncludesUpperBracket) {
    auto spec = MakePhaseASpec(LogDfScheme_::Value_::LOG_LINEAR);
    spec.instruments_[0] =
        Handle_<YCInstrument_>(new Swap_(spec.today_, spec.today_, Date_(2022, 5, 1), 0.0105, AnnualLeg(), AnnualIndex(), AnnualLeg()));

    CurveCalibrationResult_ result;
    AssertJacobianMatchesCentralDifferenceAtSolution(spec, 1.0e-6, 1.0e-9, &result);
    if (::testing::Test::HasFatalFailure())
        return;
    ASSERT_NE(result.diagnostics_.jacobian_(0, 1), 0.0);
}

TEST(AnalyticJacobianTest, TestLogLinearPaymentLagDoesNotTruncateRow) {
    auto spec = MakePhaseASpec(LogDfScheme_::Value_::LOG_LINEAR);
    auto fixedLeg = AnnualLeg();
    auto floatLeg = AnnualLeg();
    fixedLeg.paymentLag_ = 2;
    floatLeg.paymentLag_ = 2;
    spec.instruments_[3] = Handle_<YCInstrument_>(new Swap_(spec.today_, spec.today_, Date_(2024, 1, 1), 0.0155, fixedLeg, AnnualIndex(), floatLeg));

    CurveCalibrationResult_ result;
    AssertJacobianMatchesCentralDifferenceAtSolution(spec, 1.0e-6, 1.0e-9, &result);
    if (::testing::Test::HasFatalFailure())
        return;
    ASSERT_NE(result.diagnostics_.jacobian_(3, 4), 0.0);
}

TEST(AnalyticJacobianTest, TestPiecewiseConstantForwardEngagesAnalyticJacobian) {
    AssertForwardJacobianMatchesCentralDifference(MakeForwardParameterizationSpec(CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD));
}

TEST(AnalyticJacobianTest, TestPiecewiseLinearForwardEngagesAnalyticJacobian) {
    AssertForwardJacobianMatchesCentralDifference(MakeForwardParameterizationSpec(CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD));
}

TEST(AnalyticJacobianTest, TestMultiKnotPiecewiseLinearForwardMatchesCentralDifference) {
    AssertForwardJacobianMatchesCentralDifference(MakeMultiKnotPiecewiseLinearSpec(), 1.0e-5);
}

TEST(AnalyticJacobianTest, TestZeroRateLogLinearMatchesCentralDifferenceAndBumpedCalibration) {
    AssertZeroRateJacobianMatchesCentralDifference(LogDfScheme_::Value_::LOG_LINEAR);
}

TEST(AnalyticJacobianTest, TestZeroRateLogCubicNaturalMatchesCentralDifferenceAndBumpedCalibration) {
    AssertZeroRateJacobianMatchesCentralDifference(LogDfScheme_::Value_::LOG_CUBIC_NATURAL);
}

TEST(AnalyticJacobianTest, TestZeroRateMixedMatchesCentralDifferenceAndBumpedCalibration) {
    AssertZeroRateJacobianMatchesCentralDifference(LogDfScheme_::Value_::MIXED);
}

TEST(AnalyticJacobianTest, TestZeroRateCanDisableBothJacobianDiagnostics) {
    const CurveCalibrationSpec_ spec = MakeZeroRatePhaseASpec(LogDfScheme_::Value_::LOG_LINEAR);
    CurveCalibrationOptions_ options;
    options.jacobianMode_ = CurveJacobianMode_::Value_::ANALYTIC;
    options.computeEffJacobianInverse_ = false;
    options.computeForwardJacobian_ = false;

    const CurveCalibrationResult_ result = CalibrateYieldCurve(spec, options);
    ASSERT_LT(result.diagnostics_.maxAbsResidual_, 1.0e-7);
    ASSERT_TRUE(result.diagnostics_.effJacobianInverse_.Empty());
    ASSERT_TRUE(result.diagnostics_.jacobian_.Empty());
    ASSERT_NE(dynamic_cast<const DiscountZeroRate_*>(result.curve_.get()), nullptr);
}

// Category 2: Structural zeros are EXACTLY zero
// AAD produces exact structural zeros (no bump noise). Each row of the Jacobian must have at least
// one exactly-zero entry for a column beyond the instrument's cashflow support.

TEST(AnalyticJacobianTest, TestStructuralZerosAreExactlyZero) {
    auto spec = MakePhaseASpec();
    const CurveCalibrationResult_ result = CalibrateAnalytic(spec);
    ASSERT_LT(result.diagnostics_.maxAbsResidual_, 1.0e-7);

    const Matrix_<>& J = result.diagnostics_.jacobian_;
    ASSERT_EQ(J.Rows(), 5);
    ASSERT_EQ(J.Cols(), 5);

    // The first instrument is a 3M swap (start=2022-1-1, end=2022-4-1). Its only cashflow lands
    // at solver column 0. Row 0 must be EXACTLY zero at columns 1..4 (AAD structural zeros).
    for (int c = 1; c < 5; ++c)
        ASSERT_EQ(J(0, c), 0.0) << "row 0 col " << c << " = " << J(0, c) << " (expected exactly zero)";
}

// Category 3: Solve convergence -- the AAD-tape Jacobian drives the solver to a fit
// We do NOT assert iteration count (linesearch varies), only that the residual converges.

TEST(AnalyticJacobianTest, TestSolveConvergesLogLinear) {
    auto specAAD = MakePhaseASpec();

    const auto rAAD = CalibrateYieldCurve(specAAD);

    ASSERT_LT(rAAD.diagnostics_.maxAbsResidual_, 1.0e-7);

    const auto* cAAD = dynamic_cast<const DiscountLogDF_*>(rAAD.curve_.get());
    ASSERT_NE(cAAD, nullptr);
    ASSERT_EQ(static_cast<int>(cAAD->NodeLogDF().size()), 6);
}

// Category 4: forecast-target calibration (calibrateDiscountCurve_ == false) is ineligible
// A forecast-target calibration slots the calibrated curve into forwardCurves_ (a distinct code
// path from the discount-target case in YieldCurveWith). EligibleForAnalyticJacobian rejects it,
// so the byproduct diagnostics_.jacobian_ is EMPTY. Observed here via the multi-curve flow: a
// PWLF discount stage solves and seeds a PWLF forward stage. (A LOG_DISCOUNT forward stage does
// not solve with the Phase A instrument set -- the forward-curve log-DF spread on the base
// discount curve produces a singular J^T W J at the flat guess -- so the forecast-target branch
// of EligibleForAnalyticJacobian, which fires only for LOG_DISCOUNT forecast-target calibrations,
// is not directly exercisable via the calibration byproduct. The NOTICE at calibration.cpp:425
// still fires in production for that shape; this test covers the forecast-target calibration FLOW
// end-to-end and asserts its empty byproduct.)

TEST(AnalyticJacobianTest, TestIneligibleForecastTargetFallsBack) {
    const Date_ today(2024, 1, 15);
    const DayBasis_ basis("ACT_360");
    const Vector_<Date_> knotDates = {
        Date::AddMonths(today, 3),
        Date::AddMonths(today, 6),
        Date::AddMonths(today, 12),
        Date::AddMonths(today, 24),
    };

    // Pre-built market curves with consistent discount + forward, so the instrument market rates
    // derived below are exactly reproducible and the calibrations solve.
    const Handle_<DiscountCurve_> ois(
        NewDiscountPWLF("ois_mkt", "USD", PiecewiseLinear_(knotDates, Vector_<>(knotDates.size(), 0.01), Vector_<>(knotDates.size(), 0.01))));
    const Handle_<DiscountCurve_> libor3m(NewDiscountPWLF(
        "libor3m_mkt", "USD", PiecewiseLinear_(knotDates, Vector_<>(knotDates.size(), 0.02), Vector_<>(knotDates.size(), 0.02)), ois));
    const CurveBlock_ marketCurve("market", "USD", {{CollateralType_(CollateralType_::Value_::OIS), ois}}, {{PeriodLength_("3M"), libor3m}}, basis);

    RateLegConvention_ fixedLeg;
    fixedLeg.paymentFrequency_ = PeriodLength_("6M");
    fixedLeg.dayBasis_ = basis;
    fixedLeg.accrualHolidays_ = Holidays::None();
    fixedLeg.paymentHolidays_ = Holidays::None();
    fixedLeg.businessDayConvention_ = BizDayConvention_("Unadjusted");
    fixedLeg.paymentConvention_ = BizDayConvention_("Unadjusted");
    RateLegConvention_ floatLeg = fixedLeg;
    floatLeg.paymentFrequency_ = PeriodLength_("3M");

    RateIndexConvention_ ibor3m;
    ibor3m.dayBasis_ = basis;
    ibor3m.useProjectionCurve_ = true;
    ibor3m.forecastTenor_ = PeriodLength_("3M");
    ibor3m.fixingLag_ = 0;
    ibor3m.spotLag_ = 0;
    ibor3m.fixingHolidays_ = Holidays::None();
    ibor3m.accrualHolidays_ = Holidays::None();
    ibor3m.businessDayConvention_ = BizDayConvention_("Unadjusted");
    ibor3m.collateral_ = CollateralType_(CollateralType_::Value_::OIS);

    // Derive consistent market rates for the forward-stage instruments by pricing against the
    // pre-built market curves.
    const auto fra = Handle_<YCInstrument_>(new FRA_(today, Date::AddMonths(today, 3), Date::AddMonths(today, 6), 0.0, ibor3m));
    const auto irs = Handle_<YCInstrument_>(new Swap_(today, today, Date::AddMonths(today, 24), 0.0, fixedLeg, ibor3m, floatLeg));
    const double fraRate = (*fra->Precompute(Handle_<YieldCurve_>()))(marketCurve);
    const double irsRate = (*irs->Precompute(Handle_<YieldCurve_>()))(marketCurve);

    CurveCalibrationSpec_ discountStage;
    discountStage.today_ = today;
    discountStage.ccy_ = "USD";
    discountStage.curveName_ = "ois";
    discountStage.targetCollateral_ = CollateralType_(CollateralType_::Value_::OIS);
    discountStage.liborBasis_ = basis;
    discountStage.instruments_ = {
        Handle_<YCInstrument_>(new Deposit_(today, Date::AddMonths(today, 3), 0.01, basis)),
        Handle_<YCInstrument_>(new Swap_(today, Date::AddMonths(today, 24), 0.01, 6, basis)),
    };
    discountStage.knotDates_ = knotDates;

    CurveCalibrationSpec_ forwardStage;
    forwardStage.today_ = today;
    forwardStage.ccy_ = "USD";
    forwardStage.curveName_ = "libor3m";
    forwardStage.calibrateDiscountCurve_ = false;
    forwardStage.targetTenor_ = PeriodLength_("3M");
    forwardStage.targetCollateral_ = CollateralType_(CollateralType_::Value_::OIS);
    forwardStage.liborBasis_ = basis;
    forwardStage.instruments_ = {
        Handle_<YCInstrument_>(new FRA_(today, Date::AddMonths(today, 3), Date::AddMonths(today, 6), fraRate, ibor3m)),
        Handle_<YCInstrument_>(new Swap_(today, today, Date::AddMonths(today, 24), irsRate, fixedLeg, ibor3m, floatLeg)),
    };
    forwardStage.knotDates_ = knotDates;

    MultiCurveCalibrationSpec_ multi;
    multi.name_ = "forecast_ineligible";
    multi.ccy_ = "USD";
    multi.liborBasis_ = basis;
    multi.stages_ = {discountStage, forwardStage};

    const MultiCurveCalibrationResult_ result = CalibrateMultiCurve(multi);
    ASSERT_EQ(result.diagnostics_.size(), 2u);
    ASSERT_EQ(result.forwardCurves_.count(PeriodLength_("3M")), 1u);
    // The forecast-target forward stage is ineligible for the AAD Jacobian -> empty byproduct J.
    ASSERT_TRUE(result.diagnostics_[1].jacobian_.Empty());
}

// Category 5: tradeDate != start must be rejected (regression for the gate bug)
// Phase A's templated rates read DF(tradeDate_, p) (see ycinstrument.cpp), so eligibility must be
// checked against the real trade date, not the effective/spot start that TimeSpan().first returns.
// A spot-started instrument has tradeDate strictly before start (the typical spotLag-business-days
// gap). Before the TradeDate() accessor, the gate checked TimeSpan().first (== start_) instead, so
// a swap with start == anchor but tradeDate != anchor was wrongly admitted and its residual row was
// silently mispriced on the tape. After the fix the gate rejects it, the solver dense-bumps, and
// diagnostics_.jacobian_ is EMPTY. We construct one such swap alongside an eligible one to confirm
// the whole calibration falls back when ANY instrument is ineligible.

TEST(AnalyticJacobianTest, TestTradeDateNotStartRejected) {
    auto spec = MakePhaseASpec();
    const auto fixedLeg = AnnualLeg();
    const auto floatIdx = AnnualIndex();
    const auto floatLeg = AnnualLeg();
    // Spot-started swap: tradeDate is two days before the anchor start. start_ stays at the
    // anchor so TimeSpan().first == anchor -- the exact shape the old (buggy) gate admitted.
    spec.instruments_ = {
        // Eligible swap: tradeDate == start == anchor.
        Handle_<YCInstrument_>(new Swap_(spec.today_, spec.today_, Date_(2022, 4, 1), 0.010, fixedLeg, floatIdx, floatLeg)),
        // Ineligible swap: tradeDate (2021-12-30) != start (2022-01-01 == anchor).
        Handle_<YCInstrument_>(new Swap_(Date_(2021, 12, 30), Date_(2022, 1, 1), Date_(2023, 1, 1), 0.012, fixedLeg, floatIdx, floatLeg)),
    };
    const CurveCalibrationResult_ result = CalibrateAnalytic(spec);
    ASSERT_NE(result.curve_, nullptr);
    ASSERT_TRUE(result.diagnostics_.jacobian_.Empty()); // empty -> ineligible, solver dense-bumps
}

// Sanity check the symmetric case: when tradeDate == start == anchor the same shape is still
// admitted (non-empty Jacobian), guarding against an over-broad rejection. Runs on every backend
// now that the analytic path is backend-neutral. The ladder is the full 5-swap Phase A set so the
// underdetermined single-swap shape is not exercised here; admission is observed via a populated
// diagnostics_.jacobian_.
TEST(AnalyticJacobianTest, TestTradeDateEqualsStartStillAdmitted) {
    auto spec = MakePhaseASpec();
    // Every Phase A swap has tradeDate == start == anchor, so the whole ladder is admitted and the
    // byproduct J is populated with the expected 5 x 5 shape.
    const CurveCalibrationResult_ result = CalibrateAnalytic(spec);
    ASSERT_LT(result.diagnostics_.maxAbsResidual_, 1.0e-7);
    const Matrix_<>& J = result.diagnostics_.jacobian_;
    ASSERT_FALSE(J.Empty()); // admitted -> non-empty analytic Jacobian
    ASSERT_EQ(J.Rows(), 5);
    ASSERT_EQ(J.Cols(), 5);
}

// Category 6: Tape isolation -- two consecutive calibrations do not leak state
// If the TapeGuard_ leaks adjoints, the second calibration's Jacobian would inherit the first
// call's residuals and produce wrong numbers. We assert the second byproduct J reproduces the
// first exactly.

TEST(AnalyticJacobianTest, TestTapeIsolationAcrossCalls) {
    auto spec = MakePhaseASpec();
    const CurveCalibrationResult_ r1 = CalibrateAnalytic(spec);
    const CurveCalibrationResult_ r2 = CalibrateAnalytic(spec);
    ASSERT_LT(r1.diagnostics_.maxAbsResidual_, 1.0e-7);
    ASSERT_LT(r2.diagnostics_.maxAbsResidual_, 1.0e-7);

    const Matrix_<>& J1 = r1.diagnostics_.jacobian_;
    const Matrix_<>& J2 = r2.diagnostics_.jacobian_;
    ASSERT_FALSE(J1.Empty());
    ASSERT_EQ(J1.Rows(), J2.Rows());
    ASSERT_EQ(J1.Cols(), J2.Cols());
    for (int r = 0; r < J1.Rows(); ++r)
        for (int c = 0; c < J1.Cols(); ++c)
            ASSERT_NEAR(J1(r, c), J2(r, c), 1e-12) << "row=" << r << " col=" << c;
}

// Category 7: All three LogDfScheme_ values must match central differences
// Phase A eligibility is scheme-agnostic -- the templated DiscountLogDF_<T_> dispatches on scheme
// inside the tape. The TestMatchesCentralDifferenceLogLinear test above covers LOG_LINEAR; these
// two cover LOG_CUBIC_NATURAL and MIXED, exercising the natural-cubic and mixed-cutoff spline
// branches of the AAD path.

TEST(AnalyticJacobianTest, TestMatchesCentralDifferenceLogCubicNatural) {
    auto spec = MakePhaseASpec(LogDfScheme_::Value_::LOG_CUBIC_NATURAL);
    AssertJacobianMatchesCentralDifferenceAtSolution(spec, 1.0e-6, 1.0e-9);
}

TEST(AnalyticJacobianTest, TestMatchesCentralDifferenceMixed) {
    auto spec = MakePhaseASpec(LogDfScheme_::Value_::MIXED);
    AssertJacobianMatchesCentralDifferenceAtSolution(spec, 1.0e-6, 1.0e-9);
}

// Category 8: Single-instrument tape canary (Deposit)
// A single Deposit isolates the tape from multi-row sparsity. The Jacobian is 1 x N: one reverse
// sweep over one residual. If the tape mis-computes dResidual/d(logDF_node), this test catches it
// directly -- there is no confounding with structural-zero assembly across rows. We assert the
// byproduct J matches a central difference for every node column, and that the columns the deposit
// does NOT touch (beyond its maturity) are EXACTLY zero (AAD structural zero, not noise).
//
// Note: a single Deposit with 5 free knots is underdetermined (1 equation, 5 unknowns); the
// smoothing weight regularizes the solve so it still converges, and the byproduct analytic J at
// the solution is well-defined regardless of underdeterminacy (it is dResidual/d(logDF), not the
// inverse map).

TEST(AnalyticJacobianTest, TestSingleDepositTapeMatchesCentralDifference) {
    CurveCalibrationSpec_ spec;
    spec.today_ = Date_(2022, 1, 1);
    spec.ccy_ = "USD";
    spec.curveName_ = "phase_a_deposit_canary";
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

    // One 3M deposit starting at the anchor. Its cashflow lands at 2022-04-01 (knot column 0).
    RateIndexConvention_ idx = AnnualIndex();
    spec.instruments_ = {Handle_<YCInstrument_>(new Deposit_(spec.today_, spec.today_, Date_(2022, 4, 1), 0.011, idx))};

    CurveCalibrationResult_ result;
    AssertJacobianMatchesCentralDifferenceAtSolution(spec, 1.0e-6, 1.0e-9, &result);

    // The deposit's only cashflow is at 2022-04-01 -- solver column 0 under LOG_LINEAR. Columns
    // 1..4 must be EXACTLY zero (AAD structural zero, no FD noise). The helper above already proved
    // FD agreement and hands back its byproduct J for these structural-zero checks.
    const Matrix_<>& J = result.diagnostics_.jacobian_;
    ASSERT_EQ(J.Rows(), 1);
    ASSERT_EQ(J.Cols(), 5);
    ASSERT_NE(J(0, 0), 0.0) << "deposit sensitivity at its own maturity column must be nonzero";
    for (int c = 1; c < 5; ++c)
        ASSERT_EQ(J(0, c), 0.0) << "deposit row col " << c << " = " << J(0, c) << " (expected exactly zero)";
}

// Category 9: Mixed instrument types in one calibration (Deposit + FRA + Swap)
// Phase A is eligible for vanilla Swap, Deposit, FRA, and Future. A calibration mixing all three
// primary cash instrument types exercises the per-instrument dispatch in PhaseAJacobian_
// (Tape::DepositRate_ + Tape::ForwardRate_ + Tape::SwapRate_) in a single recording. The byproduct
// AAD Jacobian must still match central differences row by row, and structural zeros must appear
// for instruments whose cashflows end before later nodes.

TEST(AnalyticJacobianTest, TestMixedInstrumentCalibrationMatchesCentralDifference) {
    CurveCalibrationSpec_ spec;
    spec.today_ = Date_(2022, 1, 1);
    spec.ccy_ = "USD";
    spec.curveName_ = "phase_a_mixed";
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

    // All three instruments start at the anchor (eligibility requires span.first == anchor).
    const RateIndexConvention_ idx = AnnualIndex();
    const auto fixedLeg = AnnualLeg();
    const auto floatLeg = AnnualLeg();
    spec.instruments_ = {
        // Deposit 3M -> cashflow at 2022-04-01 (column 0).
        Handle_<YCInstrument_>(new Deposit_(spec.today_, spec.today_, Date_(2022, 4, 1), 0.011, idx)),
        // FRA at anchor -> 6M fixing, cashflow at 2022-07-01 (column 1).
        Handle_<YCInstrument_>(new FRA_(spec.today_, spec.today_, Date_(2022, 7, 1), 0.012, idx)),
        // Swap 3Y -> cashflows through 2025-01-01 (columns 0..4).
        Handle_<YCInstrument_>(new Swap_(spec.today_, spec.today_, Date_(2025, 1, 1), 0.018, fixedLeg, idx, floatLeg)),
    };

    CurveCalibrationResult_ result;
    AssertJacobianMatchesCentralDifferenceAtSolution(spec, 1.0e-6, 1.0e-9, &result);

    // Structural zeros: the deposit (row 0) ends at 2022-04-01, so columns 1..4 must be exactly
    // zero. The FRA (row 1) ends at 2022-07-01, so columns 2..4 must be exactly zero.
    const Matrix_<>& J = result.diagnostics_.jacobian_;
    ASSERT_EQ(J.Rows(), 3);
    ASSERT_EQ(J.Cols(), 5);
    for (int c = 1; c < 5; ++c)
        ASSERT_EQ(J(0, c), 0.0) << "deposit row col " << c << " = " << J(0, c);
    for (int c = 2; c < 5; ++c)
        ASSERT_EQ(J(1, c), 0.0) << "fra row col " << c << " = " << J(1, c);
}

// Category 10: B2 sentinel -- every row has a non-trivial Jacobian
// The signature failure mode for a missed registerInput / broken recording window (the B2 class)
// is an ALL-ZERO Jacobian row: the tape never learned the input is an independent, so the reverse
// sweep propagates nothing and the harvested row is zero. This invariant trips that failure on the
// byproduct AAD J alone, before any FD comparison: for every row i, at least one column j must
// satisfy |jac(i, j)| > 1e-6. Runs on every backend; the FD oracle above is the deeper check.

TEST(AnalyticJacobianTest, TestEveryRowHasNonTrivialJacobian) {
    auto spec = MakePhaseASpec();
    const CurveCalibrationResult_ result = CalibrateAnalytic(spec);
    ASSERT_LT(result.diagnostics_.maxAbsResidual_, 1.0e-7);

    const Matrix_<>& J = result.diagnostics_.jacobian_;
    ASSERT_EQ(J.Rows(), 5);
    ASSERT_EQ(J.Cols(), 5);
    for (int r = 0; r < 5; ++r) {
        double maxAbs = 0.0;
        for (int c = 0; c < 5; ++c)
            maxAbs = std::max(maxAbs, std::abs(J(r, c)));
        ASSERT_GT(maxAbs, 1.0e-6) << "row " << r << " is all-zero (B2 sentinel: missed registerInput?)";
    }
}

// Category 11: B1 sentinel -- later rows stay clean of earlier rows' residue
// On Adept the compute_adjoint override zeroes only each consumed statement's LHS and accumulates
// into operands whose gradients are never cleared between single-result sweeps. If ZeroAdjoints
// were a no-op (the B1 bug), row 1's seed would leave operand residue that row 2's sweep inherits,
// and row 2's harvested Jacobian would be wrong -- specifically its structural zeros would no
// longer be exactly zero, and its non-zero entries would disagree with a finite difference. This
// test runs the full 5-row ladder (each row's cashflow support is a strict prefix of the columns)
// and asserts BOTH that every entry matches a central difference AND that the structural zeros in
// LATER rows stay exactly zero despite EARLIER rows having populated those same columns. A
// ZeroAdjoints leak makes the later-row zeros non-zero.

TEST(AnalyticJacobianTest, TestLaterRowsCleanOfEarlierResidue) {
    auto spec = MakePhaseASpec();
    const CurveCalibrationResult_ result = CalibrateAnalytic(spec);
    ASSERT_LT(result.diagnostics_.maxAbsResidual_, 1.0e-7);

    const Matrix_<>& J = result.diagnostics_.jacobian_;
    ASSERT_EQ(J.Rows(), 5);
    ASSERT_EQ(J.Cols(), 5);

    // Row 0 is the 3M swap (cashflow at column 0 only): J(0, 1..4) must be EXACTLY zero. Row 1 is
    // the 6M swap (cashflow at columns 0..1): J(1, 2..4) must be exactly zero. These zeros are the
    // B1 falsifier: row 1 sweeps AFTER row 0, and if row 0's residue leaked, row 1's structural
    // zeros would be non-zero.
    for (int c = 1; c < 5; ++c)
        ASSERT_EQ(J(0, c), 0.0) << "row 0 col " << c << " = " << J(0, c) << " (B1 sentinel: residue from no prior row, must be structural zero)";
    for (int c = 2; c < 5; ++c)
        ASSERT_EQ(J(1, c), 0.0) << "row 1 col " << c << " = " << J(1, c) << " (B1 sentinel: row-0 residue leaked into row-1 structural zero?)";

    // Every non-structural entry must match a central difference at the solution. A residue leak
    // would push a later row off its FD value.
    const Vector_<> x = SolvedFreeParams(*result.curve_);
    const double h = 1.0e-6;
    for (int c = 0; c < 5; ++c) {
        Vector_<> xUp = x;
        Vector_<> xDn = x;
        xUp[c] += h;
        xDn[c] -= h;
        const Vector_<> fUp = EvalResiduals(spec, xUp);
        const Vector_<> fDn = EvalResiduals(spec, xDn);
        for (int r = 0; r < 5; ++r) {
            const double fd = (fUp[r] - fDn[r]) / (2.0 * h);
            const double an = J(r, c);
            if (std::abs(fd) < 1e-9) {
                ASSERT_NEAR(an, 0.0, 1e-9) << "row=" << r << " col=" << c;
            } else {
                ASSERT_NEAR(an, fd, 1e-9 * std::max(1.0, std::abs(fd))) << "row=" << r << " col=" << c;
            }
        }
    }
}

// Category 12: Structural asymmetry guard (defense against a future Jacobian-layout transpose)
// A multi-result fast path (e.g. Adept stack.jacobian(), deferred) could transpose the Jacobian
// layout if its dep/indep offset bookkeeping is wrong. The LOG_DISCOUNT swap ladder is provably
// non-symmetric: swap i (maturing at knot i+1) has cashflow support over columns 0..i, so
// J(i, j>i) == 0 structurally while J(j, i) for j>i can be non-zero. This test names an explicit
// asymmetric pair -- J(2,0) non-zero, J(0,2) exactly zero -- so a transposed layout is falsifiable
// even without re-running the full FD oracle.

TEST(AnalyticJacobianTest, TestNonSymmetricLayoutAsymmetricPair) {
    auto spec = MakePhaseASpec();
    CurveCalibrationResult_ result;
    AssertJacobianMatchesCentralDifferenceAtSolution(spec, 1.0e-6, 1.0e-9, &result);
    const Matrix_<>& J = result.diagnostics_.jacobian_;
    ASSERT_EQ(J.Rows(), 5);
    ASSERT_EQ(J.Cols(), 5);
    // Annual-coupon swaps only touch the 2023/2024/2025 nodes (columns 2, 3, 4); the 2022-04 and
    // 2022-07 nodes (columns 0, 1) are touched only by the shorter swaps. Row 4 is the 5Y swap
    // (matures 2025-01-01): it HAS exposure to column 2 (the 2023 node its first coupon lands on).
    ASSERT_NE(J(4, 2), 0.0) << "5Y swap must have exposure to the 2023 node (col 2)";
    // Row 2 is the 1Y swap (matures 2023-01-01): its only coupon is at column 2, so it has NO
    // exposure to column 4 (the 2025 node). A transposed layout would swap these and the FD oracle
    // above would fail first; this assertion names the asymmetry directly.
    ASSERT_EQ(J(2, 4), 0.0) << "1Y swap must have no exposure to the 2025 node (col 4)";
}

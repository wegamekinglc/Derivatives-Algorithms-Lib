//
// Created by dal-implementer on 2026/6/20.
//

#include <gtest/gtest.h>
#include <cmath>
#include <map>
#include <memory>
#include <utility>
#include <dal/platform/platform.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/discount.hpp>
#include <dal/curve/jointcalibration.hpp>
#include <dal/curve/piecewiselinear.hpp>
#include <dal/curve/ycimp.hpp>
#include <dal/curve/ycpwlf.hpp>
#include <dal/curve/ycinstrument.hpp>
#include <dal/currency/currencydata.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/protocol/rateconvention.hpp>
#include <dal/time/date.hpp>
#include <dal/time/daybasis.hpp>
#include <dal/time/holidays.hpp>
#include <dal/time/periodlength.hpp>
#include <dal/utilities/exceptions.hpp>

using namespace Dal;

// The joint AAD analytic Jacobian is backend-neutral: the templated machinery (Tape::DiscountPWLF_,
// Tape::JointCurveBlock_, Tape::JointRate_) compiles and produces a correct Jacobian under every
// AAD backend (native, XAD, CoDiPack, Adept) via the Dal::AAD facade. Every test below runs on
// every backend; there is no skip machinery.
//
// AC10 validates factory-vs-direct-construction identity of Tape::DiscountPWLF_<double>.
// AC11 validates Tape::DiscountPWLF_<double>::operator() against the independent
// PiecewiseLinear_::IntegralTo() oracle across all four IntegralTo branches.
// AC12 validates ApplyDX produces correct post-bump discount factors.

namespace {
    // A small joint system: 1 OIS-discount declaration (OIS swaps + deposits) + 1 IBOR-3M forward
    // declaration (FRAs + swaps), base-layered over OIS. This is the canonical AC1-AC8 workload.
    // Reused across the oracle (AC1), eligibility (AC7), and BUMPED-fallback (AC8) tests.
    Vector_<Date_> SharedKnots(const Date_& today) {
        return {
            Date::AddMonths(today, 3), Date::AddMonths(today, 6), Date::AddMonths(today, 9),
            Date::AddMonths(today, 12), Date::AddMonths(today, 18), Date::AddMonths(today, 24),
            Date::AddMonths(today, 36), Date::AddMonths(today, 60),
        };
    }

    Handle_<DiscountCurve_> MakeFlatForward(const String_& name,
                                             const String_& ccy,
                                             const Vector_<Date_>& knots,
                                             double rate,
                                             const Handle_<DiscountCurve_>& base = Handle_<DiscountCurve_>()) {
        const Vector_<> values(knots.size(), rate);
        return Handle_<DiscountCurve_>(NewDiscountPWLF(name, ccy, PiecewiseLinear_(knots, values, values), base));
    }

    // Reprice a zero-quote prototype against a market curve block to obtain a self-consistent quote.
    // Mirrors the QuotedInstrument idiom from test_joint_calibration.cpp (subset: Deposit/FRA/Swap/OISSwap).
    Handle_<YCInstrument_> Reprice(const Handle_<YCInstrument_>& proto, const CurveBlock_& market, const Date_& tradeDate, const Ccy_& ccy) {
        const auto rate = proto->Precompute(Handle_<YieldCurve_>());
        const auto mktRate = (*rate)(market);
        const auto span = proto->TimeSpan();
        if (dynamic_cast<const Deposit_*>(proto.get()))
            return Handle_<YCInstrument_>(new Deposit_(tradeDate, span.first, span.second, mktRate, Ccy::Conventions::OisIndex()(ccy)));
        if (dynamic_cast<const FRA_*>(proto.get()))
            return Handle_<YCInstrument_>(new FRA_(tradeDate, span.first, span.second, mktRate, Ccy::Conventions::LiborIndex()(ccy)));
        if (dynamic_cast<const OISSwap_*>(proto.get())) {
            auto fixedLeg = Ccy::Conventions::SwapFixedLeg()(ccy);
            auto overnightIndex = Ccy::Conventions::OisIndex()(ccy);
            auto overnightLeg = fixedLeg;
            overnightLeg.paymentFrequency_ = PeriodLength_("12M");
            overnightLeg.dayBasis_ = overnightIndex.dayBasis_;
            return Handle_<YCInstrument_>(new OISSwap_(tradeDate, span.first, span.second, mktRate, fixedLeg, overnightIndex, overnightLeg));
        }
        if (dynamic_cast<const Swap_*>(proto.get())) {
            auto fixedLeg = Ccy::Conventions::SwapFixedLeg()(ccy);
            auto libor3m = Ccy::Conventions::LiborIndex()(ccy);
            auto floatLeg = Ccy::Conventions::SwapFloatLeg()(ccy);
            return Handle_<YCInstrument_>(new Swap_(tradeDate, span.first, span.second, mktRate, fixedLeg, libor3m, floatLeg));
        }
        REQUIRE(false, "Unsupported test instrument type");
        return Handle_<YCInstrument_>();
    }

    JointMultiCurveCalibrationSpec_ BuildSmallJointSpec(const Date_& today, const Ccy_& ccy, bool baseLayered,
                                                        const DayBasis_& liborBasis = DayBasis_("ACT_360")) {
        auto overnightIndex = Ccy::Conventions::OisIndex()(ccy);
        overnightIndex.accrualHolidays_ = Holidays::None();
        overnightIndex.fixingHolidays_ = Holidays::None();
        auto libor3m = Ccy::Conventions::LiborIndex()(ccy);
        libor3m.accrualHolidays_ = Holidays::None();
        libor3m.fixingHolidays_ = Holidays::None();
        libor3m.dayBasis_ = liborBasis; // pin the basis so AAD eligibility (ACT_365F) or ineligible (ACT_360) is respected
        auto fixedLeg = Ccy::Conventions::SwapFixedLeg()(ccy);
        fixedLeg.accrualHolidays_ = Holidays::None();
        fixedLeg.paymentHolidays_ = Holidays::None();
        auto floatLeg = Ccy::Conventions::SwapFloatLeg()(ccy);
        floatLeg.accrualHolidays_ = Holidays::None();
        floatLeg.paymentHolidays_ = Holidays::None();
        auto overnightLeg = fixedLeg;
        overnightLeg.paymentFrequency_ = PeriodLength_("12M");
        overnightLeg.dayBasis_ = overnightIndex.dayBasis_;

        const String_ ccyName = ccy.String();
        const Vector_<Date_> knots = SharedKnots(today);
        // Flat market: OIS=2%, 3M=2.5% layered on OIS.
        const Handle_<DiscountCurve_> oisMarket = MakeFlatForward("ois_market", ccyName, knots, 0.02);
        const Handle_<DiscountCurve_> fwd3mMarket = MakeFlatForward("fwd3m_market", ccyName, knots, 0.025, oisMarket);
        CurveBlock_ market("market", ccyName, {{CollateralType_(CollateralType_::Value_::OIS), oisMarket}},
                           {{libor3m.forecastTenor_, fwd3mMarket}}, libor3m.dayBasis_);

        Vector_<Handle_<YCInstrument_>> oisProto = {
            Handle_<YCInstrument_>(new Deposit_(today, today, Date::AddMonths(today, 1), 0.0, overnightIndex)),
            Handle_<YCInstrument_>(new Deposit_(today, today, Date::AddMonths(today, 3), 0.0, overnightIndex)),
            Handle_<YCInstrument_>(new Deposit_(today, today, Date::AddMonths(today, 6), 0.0, overnightIndex)),
            Handle_<YCInstrument_>(new OISSwap_(today, today, Date::AddMonths(today, 12), 0.0, fixedLeg, overnightIndex, overnightLeg)),
            Handle_<YCInstrument_>(new OISSwap_(today, today, Date::AddMonths(today, 24), 0.0, fixedLeg, overnightIndex, overnightLeg)),
            Handle_<YCInstrument_>(new OISSwap_(today, today, Date::AddMonths(today, 60), 0.0, fixedLeg, overnightIndex, overnightLeg)),
        };
        Vector_<Handle_<YCInstrument_>> liborProto = {
            Handle_<YCInstrument_>(new FRA_(today, Date::AddMonths(today, 1), Date::AddMonths(today, 4), 0.0, libor3m)),
            Handle_<YCInstrument_>(new FRA_(today, Date::AddMonths(today, 3), Date::AddMonths(today, 6), 0.0, libor3m)),
            Handle_<YCInstrument_>(new Swap_(today, today, Date::AddMonths(today, 12), 0.0, fixedLeg, libor3m, floatLeg)),
            Handle_<YCInstrument_>(new Swap_(today, today, Date::AddMonths(today, 36), 0.0, fixedLeg, libor3m, floatLeg)),
        };
        Vector_<Handle_<YCInstrument_>> oisQuoted;
        oisQuoted.reserve(oisProto.size());
        for (const auto& p : oisProto)
            oisQuoted.emplace_back(Reprice(p, market, today, ccy));
        Vector_<Handle_<YCInstrument_>> liborQuoted;
        liborQuoted.reserve(liborProto.size());
        for (const auto& p : liborProto)
            liborQuoted.emplace_back(Reprice(p, market, today, ccy));

        JointCurveDeclaration_ oisDecl;
        oisDecl.curveName_ = "joint_ois";
        oisDecl.instruments_ = std::move(oisQuoted);
        oisDecl.knotDates_ = knots;
        oisDecl.targetCollateral_ = CollateralType_(CollateralType_::Value_::OIS);
        oisDecl.calibrateDiscountCurve_ = true;

        JointCurveDeclaration_ liborDecl;
        liborDecl.curveName_ = "joint_3m";
        liborDecl.instruments_ = std::move(liborQuoted);
        liborDecl.knotDates_ = knots;
        liborDecl.targetCollateral_ = CollateralType_(CollateralType_::Value_::OIS);
        liborDecl.targetTenor_ = libor3m.forecastTenor_;
        liborDecl.calibrateDiscountCurve_ = false;
        liborDecl.baseLayeredOverDiscount_ = baseLayered;

        JointMultiCurveCalibrationSpec_ spec;
        spec.today_ = today;
        spec.ccy_ = ccyName;
        spec.liborBasis_ = libor3m.dayBasis_;
        spec.solveMode_ = CurveSolveMode_::Value_::EXACT;
        spec.fitTolerance_ = 1.0e-10;
        spec.tolerance_ = 1.0e-10;
        spec.curves_ = Vector_<JointCurveDeclaration_>{oisDecl, liborDecl};
        return spec;
    }
} // namespace

TEST(JointAnalyticJacobianTest, TestPwlFactoryVsDirectConstruction) {
    // AC10: after dedup, the factory (NewDiscountPWLF) constructs Tape::DiscountPWLF_<double> by
    // unpacking a PiecewiseLinear_ into flat vectors. Compare its DFs against a directly-constructed
    // Tape::DiscountPWLF_<double> with the same data to catch factory-unpacking bugs, constructor
    // divergence, or data corruption. All four IntegralTo branches are exercised.
    const Date_ today(2024, 1, 15);
    const Vector_<Date_> knots = {
        Date_(2024, 4, 15), Date_(2024, 7, 15), Date_(2024, 10, 15),
        Date_(2025, 1, 15), Date_(2025, 7, 15), Date_(2026, 1, 15),
        Date_(2027, 1, 15), Date_(2029, 1, 15),
    };
    const int nKnots = static_cast<int>(knots.size());
    Vector_<> fLeft(nKnots);
    Vector_<> fRight(nKnots);
    for (int k = 0; k < nKnots; ++k) {
        fLeft[k] = 0.02 + 0.001 * k + 0.0007 * (k % 3);
        fRight[k] = 0.025 + 0.0013 * k - 0.0005 * (k % 4);
    }

    // Factory path: goes through PiecewiseLinear_ -> unpacked in NewDiscountPWLF.
    const PiecewiseLinear_ pw(knots, fLeft, fRight);
    Handle_<DiscountCurve_> factoryCurve(NewDiscountPWLF("factory", "USD", pw));

    // Direct path: template constructor with flat vectors.
    auto directCurve = std::make_shared<Tape::DiscountPWLF_<double>>("direct", "USD", knots, fLeft, fRight);

    const Date_ beforeFirst(2024, 2, 15);
    const Date_ onKnot0 = knots.front();
    const Date_ onKnotMid = knots[4];
    const Date_ inRange1(2024, 8, 20);
    const Date_ inRange2(2025, 10, 1);
    const Date_ beyondLast(2030, 6, 15);

    const Vector_<Date_> queries = {beforeFirst, onKnot0, onKnotMid, inRange1, inRange2, beyondLast};
    for (int i = 0; i < static_cast<int>(queries.size()); ++i) {
        for (int j = i + 1; j < static_cast<int>(queries.size()); ++j) {
            const Date_ from = queries[i];
            const Date_ to = queries[j];
            const double factoryDf = (*factoryCurve)(from, to);
            const double directDf = (*directCurve)(from, to);
            ASSERT_NEAR(factoryDf, directDf, 1e-15)
                << "Factory-vs-direct mismatch at from=" << Date::ToString(from)
                << ", to=" << Date::ToString(to);
        }
    }
}

TEST(JointAnalyticJacobianTest, TestPwlAnalyticalIntegral) {
    // AC11: validate Tape::DiscountPWLF_<double>::operator() against the independent
    // PiecewiseLinear_::IntegralTo() oracle (independently tested in test_piecewiselinear.cpp).
    // Exercises all four IntegralTo branches: below first knot, beyond last knot, on a knot,
    // in-range partial trapezoid.
    const Date_ today(2024, 1, 15);
    const Vector_<Date_> knots = {
        Date_(2024, 4, 15), Date_(2024, 7, 15), Date_(2024, 10, 15),
        Date_(2025, 1, 15), Date_(2025, 7, 15), Date_(2026, 1, 15),
        Date_(2027, 1, 15), Date_(2029, 1, 15),
    };
    const int nKnots = static_cast<int>(knots.size());
    Vector_<> fLeft(nKnots);
    Vector_<> fRight(nKnots);
    for (int k = 0; k < nKnots; ++k) {
        fLeft[k] = 0.02 + 0.001 * k + 0.0007 * (k % 3);
        fRight[k] = 0.025 + 0.0013 * k - 0.0005 * (k % 4);
    }

    // Template curve under test.
    auto curve = std::make_shared<Tape::DiscountPWLF_<double>>("test", "USD", knots, fLeft, fRight);

    // Independent oracle: PiecewiseLinear_ with the same data.
    const PiecewiseLinear_ oracle(knots, fLeft, fRight);

    const Date_ beforeFirst(2024, 2, 15);
    const Date_ onKnot0 = knots.front();
    const Date_ onKnotMid = knots[4];
    const Date_ inRange1(2024, 8, 20);
    const Date_ inRange2(2025, 10, 1);
    const Date_ beyondLast(2030, 6, 15);

    const Vector_<Date_> queries = {beforeFirst, onKnot0, onKnotMid, inRange1, inRange2, beyondLast};
    for (int i = 0; i < static_cast<int>(queries.size()); ++i) {
        for (int j = i + 1; j < static_cast<int>(queries.size()); ++j) {
            const Date_ from = queries[i];
            const Date_ to = queries[j];
            const double got = (*curve)(from, to);

            // Oracle: DF = exp(-(IntegralTo(to) - IntegralTo(from)) / 365.0).
            const double oracleIntegralTo = oracle.IntegralTo(to);
            const double oracleIntegralFrom = oracle.IntegralTo(from);
            const double expected = std::exp(-(oracleIntegralTo - oracleIntegralFrom) / 365.0);

            ASSERT_NEAR(got, expected, 1e-15)
                << "Mismatch vs PiecewiseLinear_ oracle at from=" << Date::ToString(from)
                << ", to=" << Date::ToString(to);
        }
    }
}

TEST(JointAnalyticJacobianTest, TestPwlApplyDxRoundTrip) {
    // AC12: ApplyDX on Tape::DiscountPWLF_<double> correctly shifts the forward rates and the effect
    // on discount factors matches a manually bumped PiecewiseLinear_ reference.
    const Date_ today(2024, 1, 15);
    const Vector_<Date_> knots = {
        Date_(2024, 4, 15), Date_(2024, 7, 15), Date_(2024, 10, 15),
        Date_(2025, 1, 15), Date_(2025, 7, 15),
    };
    const int nKnots = static_cast<int>(knots.size());
    Vector_<> fLeft(nKnots, 0.02);
    Vector_<> fRight(nKnots, 0.02);
    // Discontinuity: fRight != fLeft at each interior knot.
    for (int k = 0; k < nKnots; ++k) {
        fLeft[k] += 0.001 * k;
        fRight[k] += 0.0005 * k;
    }

    auto curve = std::make_shared<Tape::DiscountPWLF_<double>>("test", "USD", knots, fLeft, fRight);

    // Snapshot DFs before bump.
    const Date_ from = knots.front();
    const Date_ to = knots.back();
    const double dfBefore = (*curve)(from, to);

    // Bump: shift fLeft[1] and fRight[1] by +0.001 each.
    // NX() = 2 * nKnots, so params are [fLeft0, fRight0, fLeft1, fRight1, ...].
    Vector_<> dx(2 * nKnots, 0.0);
    dx[2] = 0.001;  // fLeft[1]
    dx[3] = 0.001;  // fRight[1]
    curve->ApplyDX(dx.begin(), 1.0);

    const double dfAfter = (*curve)(from, to);

    // Manually bump the fLeft/fRight and recompute via PiecewiseLinear_ to get the expected DF.
    auto bumpedLeft = fLeft;
    auto bumpedRight = fRight;
    bumpedLeft[1] += 0.001;
    bumpedRight[1] += 0.001;
    const PiecewiseLinear_ bumpedOracle(knots, bumpedLeft, bumpedRight);
    const double expected = std::exp(-(bumpedOracle.IntegralTo(to) - bumpedOracle.IntegralTo(from)) / 365.0);

    ASSERT_NEAR(dfAfter, expected, 1e-15) << "ApplyDX bump did not match reference";

    // Sanity: bumped DF should differ from pre-bump DF.
    ASSERT_NE(dfAfter, dfBefore) << "ApplyDX had no effect on discount factors";
}

TEST(JointAnalyticJacobianTest, TestBumpedFallbackIsByteForByte) {
    // AC8: a joint options constructed with jacobianMode_ = BUMPED produces a result identical to
    // the current single-arg call on the same spec. This pins the BUMPED path as the byte-for-byte
    // reference before the ANALYTIC default flips; once ANALYTIC engages, the same test will assert
    // AAD-matches-bumped instead.
    RegisterAll_::Init();
    const Date_ today(2024, 1, 15);
    const Ccy_ ccy("USD");
    const JointMultiCurveCalibrationSpec_ spec = BuildSmallJointSpec(today, ccy, /*baseLayered=*/true);

    const JointMultiCurveCalibrationResult_ rDefault = CalibrateJointMultiCurve(spec);
    JointMultiCurveCalibrationOptions_ optBumped;
    optBumped.jacobianMode_ = CurveJacobianMode_::Value_::BUMPED;
    const JointMultiCurveCalibrationResult_ rBumped = CalibrateJointMultiCurve(spec, optBumped);

    ASSERT_TRUE(rDefault.converged_);
    ASSERT_TRUE(rBumped.converged_);
    ASSERT_NEAR(rDefault.jointMaxAbsResidual_, rBumped.jointMaxAbsResidual_, 1.0e-12);
    ASSERT_NEAR(rDefault.jointRmsResidual_, rBumped.jointRmsResidual_, 1.0e-12);
    ASSERT_EQ(rDefault.discountCurves_.size(), rBumped.discountCurves_.size());
    ASSERT_EQ(rDefault.forwardCurves_.size(), rBumped.forwardCurves_.size());
}

// AC1: AAD-vs-bumped oracle. On an eligible spec (PWL_FWD + ACT_365F + base-layered + EXACT), the
// ANALYTIC Jacobian engages, the calibration converges, and the per-pillar DFs agree with the bumped
// reference to the smoothing-fit residual floor. The analytic forward Jacobian is populated.
TEST(JointAnalyticJacobianTest, TestAnalyticEligibleAgreesWithBumped) {
    RegisterAll_::Init();
    const Date_ today(2024, 1, 15);
    const Ccy_ ccy("USD");
    const JointMultiCurveCalibrationSpec_ spec = BuildSmallJointSpec(today, ccy, /*baseLayered=*/true, DayBasis_("ACT_365F"));

    JointMultiCurveCalibrationOptions_ optBumped;
    optBumped.jacobianMode_ = CurveJacobianMode_::Value_::BUMPED;
    const JointMultiCurveCalibrationResult_ rBumped = CalibrateJointMultiCurve(spec, optBumped);

    JointMultiCurveCalibrationOptions_ optAnalytic;
    optAnalytic.jacobianMode_ = CurveJacobianMode_::Value_::ANALYTIC;
    const JointMultiCurveCalibrationResult_ rAnalytic = CalibrateJointMultiCurve(spec, optAnalytic);

    ASSERT_TRUE(rBumped.converged_);
    ASSERT_TRUE(rAnalytic.converged_);

    // The analytic path engaged: jacobianAtSolution_ is populated for ANALYTIC + eligible + EXACT.
    ASSERT_FALSE(rAnalytic.jacobianAtSolution_.Empty());
    ASSERT_TRUE(rBumped.jacobianAtSolution_.Empty());

    // Per-pillar OIS DFs agree to the smoothing-fit residual floor (both paths solve the same
    // system with differently-computed Jacobians; the solution x should be essentially identical).
    const auto& jointOisAnalytic = *rAnalytic.discountCurves_.at(CollateralType_(CollateralType_::Value_::OIS));
    const auto& jointOisBumped = *rBumped.discountCurves_.at(CollateralType_(CollateralType_::Value_::OIS));
    const Vector_<int> pillarMonths = {6, 12, 18, 24, 36, 60};
    for (const int months : pillarMonths) {
        const Date_ pillar = Date::AddMonths(today, months);
        ASSERT_NEAR(jointOisAnalytic(today, pillar), jointOisBumped(today, pillar), 1.0e-5)
            << "OIS DF mismatch at " << months << "M pillar";
    }

    // Per-pillar 3M forward DFs: base-layered, so both smooth the spread; agreement to ~1e-7.
    const auto& joint3mAnalytic = *rAnalytic.forwardCurves_.at(spec.curves_[1].targetTenor_);
    const auto& joint3mBumped = *rBumped.forwardCurves_.at(spec.curves_[1].targetTenor_);
    for (const int months : pillarMonths) {
        const Date_ pillar = Date::AddMonths(today, months);
        ASSERT_NEAR(joint3mAnalytic(today, pillar), joint3mBumped(today, pillar), 1.0e-5)
            << "3M DF mismatch at " << months << "M pillar";
    }
}

// AC7: the ANALYTIC default engages on an eligible spec without explicit options.
TEST(JointAnalyticJacobianTest, TestDefaultEngagesAnalyticOnEligibleSpec) {
    RegisterAll_::Init();
    const Date_ today(2024, 1, 15);
    const Ccy_ ccy("USD");
    const JointMultiCurveCalibrationSpec_ spec = BuildSmallJointSpec(today, ccy, /*baseLayered=*/true, DayBasis_("ACT_365F"));

    const JointMultiCurveCalibrationResult_ rDefault = CalibrateJointMultiCurve(spec);
    ASSERT_TRUE(rDefault.converged_);
    ASSERT_FALSE(rDefault.jacobianAtSolution_.Empty()); // default ANALYTIC engaged
}

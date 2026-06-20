//
// Created by dal-implementer on 2026/6/20.
//

#include <gtest/gtest.h>
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
// AC11 is the cheapest falsifier for the templated PWL arithmetic (critique S8): the double
// specialization Tape::DiscountPWLF_<double>::operator() must match the existing anonymous-namespace
// double DiscountPWLF_ (ycimp.cpp:63-66) element-wise across all four IntegralTo branches.

namespace {
    // Build a PWL forward with a DISCONTINUITY at every knot (fLeft != fRight), so the
    // fLeftT_[ii] + fRightT_[ii-1] segment indexing from PiecewiseLinear_::Sofar is exercised.
    // Returns the templated curve and a parallel double DiscountPWLF_ for byte-for-byte comparison.
    struct PwlPair_ {
        Handle_<DiscountCurve_> refCurve;                 // anonymous-namespace double DiscountPWLF_
        std::shared_ptr<Tape::DiscountPWLF_<double>> tCurve; // templated double specialization
        Vector_<Date_> knots;
    };

    PwlPair_ BuildDiscontinuousPwlPair() {
        const Date_ today(2024, 1, 15);
        const Vector_<Date_> knots = {
            Date_(2024, 4, 15), Date_(2024, 7, 15), Date_(2024, 10, 15),
            Date_(2025, 1, 15), Date_(2025, 7, 15), Date_(2026, 1, 15),
            Date_(2027, 1, 15), Date_(2029, 1, 15),
        };
        const int nKnots = static_cast<int>(knots.size());
        Vector_<> fLeft(nKnots);
        Vector_<> fRight(nKnots);
        // Distinct fLeft/fRight at every knot so the discontinuity is non-trivial.
        for (int k = 0; k < nKnots; ++k) {
            fLeft[k] = 0.02 + 0.001 * k + 0.0007 * (k % 3);
            fRight[k] = 0.025 + 0.0013 * k - 0.0005 * (k % 4);
        }
        const PiecewiseLinear_ pw(knots, fLeft, fRight);
        PwlPair_ retval;
        retval.knots = knots;
        retval.refCurve = Handle_<DiscountCurve_>(NewDiscountPWLF("ref", "USD", pw));
        retval.tCurve = std::make_shared<Tape::DiscountPWLF_<double>>("templ", "USD", knots, fLeft, fRight);
        return retval;
    }

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

    JointMultiCurveCalibrationSpec_ BuildSmallJointSpec(const Date_& today, const Ccy_& ccy, bool baseLayered) {
        auto overnightIndex = Ccy::Conventions::OisIndex()(ccy);
        overnightIndex.accrualHolidays_ = Holidays::None();
        overnightIndex.fixingHolidays_ = Holidays::None();
        auto libor3m = Ccy::Conventions::LiborIndex()(ccy);
        libor3m.accrualHolidays_ = Holidays::None();
        libor3m.fixingHolidays_ = Holidays::None();
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

TEST(JointAnalyticJacobianTest, TestTemplatedPwlByteForByte) {
    // AC11: the templated Tape::DiscountPWLF_<double>::operator()(from, to) matches the existing
    // anonymous-namespace double DiscountPWLF_ (ycimp.cpp:63-66) element-wise to 1e-15 across query
    // intervals that hit all four IntegralTo branches: below first knot, beyond last knot, on a
    // knot, in-range partial trapezoid.
    const PwlPair_ p = BuildDiscontinuousPwlPair();
    const Date_ beforeFirst = Date_(2024, 2, 15);    // branch 1: below first knot
    const Date_ onKnot0 = p.knots.front();           // branch 3: exactly on knot 0
    const Date_ onKnotMid = p.knots[4];              // branch 3: exactly on a middle knot
    const Date_ inRange1 = Date_(2024, 8, 20);       // branch 4: interior partial trapezoid
    const Date_ inRange2 = Date_(2025, 10, 1);       // branch 4: interior partial trapezoid (later segment)
    const Date_ beyondLast = Date_(2030, 6, 15);     // branch 2: flat-forward extrapolation past last knot

    const Vector_<Date_> queries = {beforeFirst, onKnot0, onKnotMid, inRange1, inRange2, beyondLast};
    for (int i = 0; i < static_cast<int>(queries.size()); ++i) {
        for (int j = i + 1; j < static_cast<int>(queries.size()); ++j) {
            const Date_ from = queries[i];
            const Date_ to = queries[j];
            const double ref = (*p.refCurve)(from, to);
            const double got = (*p.tCurve)(from, to);
            ASSERT_NEAR(got, ref, 1e-15) << "Mismatch at from=" << Date::ToString(from) << ", to=" << Date::ToString(to)
                                         << " (ref=" << ref << ", got=" << got << ")";
        }
    }
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

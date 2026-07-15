//
// Created by dal-implementer on 2026/6/20.
//

#include <gtest/gtest.h>
#include <string>
#include <dal/platform/platform.hpp>
#include <dal/curve/calibration.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/curveparameterization.hpp>
#include <dal/curve/discount.hpp>
#include <dal/curve/jointcalibration.hpp>
#include <dal/curve/piecewiselinear.hpp>
#include <dal/curve/ycimp.hpp>
#include <dal/curve/ycinstrument.hpp>
#include <dal/curve/yczerorate.hpp>
#include <dal/currency/currencydata.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/protocol/rateconvention.hpp>
#include <dal/time/date.hpp>
// daybasis.hpp + periodlength.hpp are load-bearing under the AAD backend builds (Adept/CoDiPack/XAD):
// schedules.hpp (pulled in transitively via rateconvention.hpp) and MG_DayBasis_enum.hpp use Handle_
// and assert without including them, and the AAD backend changes the transitive include graph so
// the platform.hpp definitions are not in scope on the transitive path. Including daybasis.hpp
// explicitly after platform.hpp makes the build backend-neutral. Do not auto-strip as "unused".
#include <dal/time/daybasis.hpp>
#include <dal/time/holidays.hpp>
#include <dal/time/periodlength.hpp>

using namespace Dal;

namespace {
    Vector_<Date_> SharedKnots(const Date_& today) {
        return Vector_<Date_>{
            Date::AddMonths(today, 1),  Date::AddMonths(today, 3),  Date::AddMonths(today, 6),
            Date::AddMonths(today, 12), Date::AddMonths(today, 24), Date::AddMonths(today, 36),
            Date::AddMonths(today, 60), Date::AddMonths(today, 84), Date::AddMonths(today, 120),
        };
    }

    Handle_<DiscountCurve_> MakeFlatDiscountCurve(
        const String_& name, const String_& ccy, const Date_& today, double rate, const Handle_<DiscountCurve_>& base = Handle_<DiscountCurve_>()) {
        const Vector_<Date_> knotDates = SharedKnots(today);
        const Vector_<> values(knotDates.size(), rate);
        return Handle_<DiscountCurve_>(NewDiscountPWLF(name, ccy, PiecewiseLinear_(knotDates, values, values), base));
    }

    // Reprice a zero-quote prototype against a market curve block to obtain a self-consistent quote.
    // Mirrors the QuotedInstrument idiom from curve_calibration.cpp:46-87 (subset: Deposit/FRA/Swap/OISSwap).
    Handle_<YCInstrument_>
    QuotedInstrument(const Handle_<YCInstrument_>& prototype, const YieldCurve_& marketCurve, const Date_& tradeDate, const Ccy_& ccy) {
        const auto rate = prototype->Precompute(Handle_<YieldCurve_>());
        if (const auto* deposit = dynamic_cast<const Deposit_*>(prototype.get())) {
            const auto span = deposit->TimeSpan();
            return Handle_<YCInstrument_>(new Deposit_(tradeDate, span.first, span.second, (*rate)(marketCurve), Ccy::Conventions::OisIndex()(ccy)));
        }
        if (const auto* fra = dynamic_cast<const FRA_*>(prototype.get())) {
            const auto span = fra->TimeSpan();
            return Handle_<YCInstrument_>(new FRA_(tradeDate, span.first, span.second, (*rate)(marketCurve), Ccy::Conventions::LiborIndex()(ccy)));
        }
        if (const auto* swap = dynamic_cast<const OISSwap_*>(prototype.get())) {
            const auto span = swap->TimeSpan();
            auto fixedLeg = Ccy::Conventions::SwapFixedLeg()(ccy);
            auto overnightIndex = Ccy::Conventions::OisIndex()(ccy);
            auto floatLeg = fixedLeg;
            floatLeg.paymentFrequency_ = PeriodLength_("12M");
            floatLeg.dayBasis_ = overnightIndex.dayBasis_;
            return Handle_<YCInstrument_>(new OISSwap_(tradeDate, span.first, span.second, (*rate)(marketCurve), fixedLeg, overnightIndex, floatLeg));
        }
        if (const auto* swap = dynamic_cast<const Swap_*>(prototype.get())) {
            const auto span = swap->TimeSpan();
            return Handle_<YCInstrument_>(new Swap_(tradeDate, span.first, span.second, (*rate)(marketCurve), Ccy::Conventions::SwapFixedLeg()(ccy),
                                                    Ccy::Conventions::LiborIndex()(ccy), Ccy::Conventions::SwapFloatLeg()(ccy)));
        }
        REQUIRE(false, "Unsupported test instrument type");
        return Handle_<YCInstrument_>();
    }

    // 12 OIS + 12 IBOR-3M prototypes with zero quotes, repriced against a flat market (OIS=1%,
    // 3M=3% layered on OIS) to obtain self-consistent par rates. The market handles are returned
    // so callers can build a CurveBlock_ for cross-validation (CurveBlock_ is noncopyable).
    struct PrototypeSet_ {
        Vector_<Handle_<YCInstrument_>> ois;
        Vector_<Handle_<YCInstrument_>> libor;
        Handle_<DiscountCurve_> oisMarket;
        Handle_<DiscountCurve_> forward3mMarket;
        DayBasis_ liborBasis;
        PeriodLength_ forecastTenor;
    };

    PrototypeSet_ BuildPrototypes(const Date_& today, const Ccy_& ccy) {
        const String_ ccyName = ccy.String();
        auto fixedLeg = Ccy::Conventions::SwapFixedLeg()(ccy);
        fixedLeg.accrualHolidays_ = Holidays::None();
        fixedLeg.paymentHolidays_ = Holidays::None();

        auto overnightIndex = Ccy::Conventions::OisIndex()(ccy);
        overnightIndex.accrualHolidays_ = Holidays::None();
        overnightIndex.fixingHolidays_ = Holidays::None();

        auto libor3m = Ccy::Conventions::LiborIndex()(ccy);
        libor3m.accrualHolidays_ = Holidays::None();
        libor3m.fixingHolidays_ = Holidays::None();

        auto floatLeg = Ccy::Conventions::SwapFloatLeg()(ccy);
        floatLeg.accrualHolidays_ = Holidays::None();
        floatLeg.paymentHolidays_ = Holidays::None();

        auto overnightLeg = fixedLeg;
        overnightLeg.paymentFrequency_ = PeriodLength_("12M");
        overnightLeg.dayBasis_ = overnightIndex.dayBasis_;

        const Handle_<DiscountCurve_> ois = MakeFlatDiscountCurve("ois_market", ccyName, today, 0.01);
        const Handle_<DiscountCurve_> forward3m = MakeFlatDiscountCurve("libor3m_market", ccyName, today, 0.03, ois);

        Vector_<Handle_<YCInstrument_>> oisProto = {
            Handle_<YCInstrument_>(new Deposit_(today, today, Date::AddMonths(today, 1), 0.0, overnightIndex)),
            Handle_<YCInstrument_>(new Deposit_(today, today, Date::AddMonths(today, 2), 0.0, overnightIndex)),
            Handle_<YCInstrument_>(new Deposit_(today, today, Date::AddMonths(today, 3), 0.0, overnightIndex)),
            Handle_<YCInstrument_>(new Deposit_(today, today, Date::AddMonths(today, 6), 0.0, overnightIndex)),
            Handle_<YCInstrument_>(new Deposit_(today, today, Date::AddMonths(today, 9), 0.0, overnightIndex)),
            Handle_<YCInstrument_>(new Deposit_(today, today, Date::AddMonths(today, 12), 0.0, overnightIndex)),
            Handle_<YCInstrument_>(new OISSwap_(today, today, Date::AddMonths(today, 24), 0.0, fixedLeg, overnightIndex, overnightLeg)),
            Handle_<YCInstrument_>(new OISSwap_(today, today, Date::AddMonths(today, 36), 0.0, fixedLeg, overnightIndex, overnightLeg)),
            Handle_<YCInstrument_>(new OISSwap_(today, today, Date::AddMonths(today, 48), 0.0, fixedLeg, overnightIndex, overnightLeg)),
            Handle_<YCInstrument_>(new OISSwap_(today, today, Date::AddMonths(today, 60), 0.0, fixedLeg, overnightIndex, overnightLeg)),
            Handle_<YCInstrument_>(new OISSwap_(today, today, Date::AddMonths(today, 84), 0.0, fixedLeg, overnightIndex, overnightLeg)),
            Handle_<YCInstrument_>(new OISSwap_(today, today, Date::AddMonths(today, 120), 0.0, fixedLeg, overnightIndex, overnightLeg)),
        };

        Vector_<Handle_<YCInstrument_>> liborProto = {
            Handle_<YCInstrument_>(new FRA_(today, Date::AddMonths(today, 1), Date::AddMonths(today, 4), 0.0, libor3m)),
            Handle_<YCInstrument_>(new FRA_(today, Date::AddMonths(today, 2), Date::AddMonths(today, 5), 0.0, libor3m)),
            Handle_<YCInstrument_>(new FRA_(today, Date::AddMonths(today, 3), Date::AddMonths(today, 6), 0.0, libor3m)),
            Handle_<YCInstrument_>(new FRA_(today, Date::AddMonths(today, 6), Date::AddMonths(today, 9), 0.0, libor3m)),
            Handle_<YCInstrument_>(new FRA_(today, Date::AddMonths(today, 9), Date::AddMonths(today, 12), 0.0, libor3m)),
            Handle_<YCInstrument_>(new FRA_(today, Date::AddMonths(today, 12), Date::AddMonths(today, 15), 0.0, libor3m)),
            Handle_<YCInstrument_>(new Swap_(today, today, Date::AddMonths(today, 24), 0.0, fixedLeg, libor3m, floatLeg)),
            Handle_<YCInstrument_>(new Swap_(today, today, Date::AddMonths(today, 36), 0.0, fixedLeg, libor3m, floatLeg)),
            Handle_<YCInstrument_>(new Swap_(today, today, Date::AddMonths(today, 48), 0.0, fixedLeg, libor3m, floatLeg)),
            Handle_<YCInstrument_>(new Swap_(today, today, Date::AddMonths(today, 60), 0.0, fixedLeg, libor3m, floatLeg)),
            Handle_<YCInstrument_>(new Swap_(today, today, Date::AddMonths(today, 84), 0.0, fixedLeg, libor3m, floatLeg)),
            Handle_<YCInstrument_>(new Swap_(today, today, Date::AddMonths(today, 120), 0.0, fixedLeg, libor3m, floatLeg)),
        };

        const CurveBlock_ market("market", ccyName, {{CollateralType_(CollateralType_::Value_::OIS), ois}}, {{libor3m.forecastTenor_, forward3m}},
                                 libor3m.dayBasis_);

        Vector_<Handle_<YCInstrument_>> oisQuoted;
        oisQuoted.reserve(oisProto.size());
        for (const auto& p : oisProto)
            oisQuoted.push_back(QuotedInstrument(p, market, today, ccy));

        Vector_<Handle_<YCInstrument_>> liborQuoted;
        liborQuoted.reserve(liborProto.size());
        for (const auto& p : liborProto)
            liborQuoted.push_back(QuotedInstrument(p, market, today, ccy));

        return {std::move(oisQuoted), std::move(liborQuoted), ois, forward3m, libor3m.dayBasis_, libor3m.forecastTenor_};
    }

    JointMultiCurveCalibrationSpec_ BuildCanonicalJointSpec(const Date_& today, const Ccy_& ccy, const PrototypeSet_& proto) {
        JointMultiCurveCalibrationSpec_ spec;
        spec.today_ = today;
        spec.ccy_ = ccy.String();
        spec.liborBasis_ = proto.liborBasis;
        spec.solveMode_ = CurveSolveMode_::Value_::APPROXIMATE;
        spec.fitTolerance_ = 1.0e-8;
        spec.tolerance_ = 1.0e-8;
        spec.smoothingWeight_ = 1.0;

        const Vector_<Date_> knots = SharedKnots(today);

        JointCurveDeclaration_ oisDecl;
        oisDecl.curveName_ = "joint_ois";
        oisDecl.instruments_ = proto.ois;
        oisDecl.knotDates_ = knots;
        oisDecl.targetCollateral_ = CollateralType_(CollateralType_::Value_::OIS);
        oisDecl.calibrateDiscountCurve_ = true;
        oisDecl.parameterization_ = CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD;

        JointCurveDeclaration_ liborDecl;
        liborDecl.curveName_ = "joint_3m";
        liborDecl.instruments_ = proto.libor;
        liborDecl.knotDates_ = knots;
        liborDecl.targetCollateral_ = CollateralType_(CollateralType_::Value_::OIS);
        liborDecl.targetTenor_ = proto.forecastTenor;
        liborDecl.calibrateDiscountCurve_ = false;
        liborDecl.parameterization_ = CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD;

        spec.curves_ = Vector_<JointCurveDeclaration_>{oisDecl, liborDecl};
        return spec;
    }

    CurveBlock_ BuildMarketBlock(const Date_& /*today*/, const Ccy_& ccy, const PrototypeSet_& proto) {
        return CurveBlock_("market", ccy.String(), {{CollateralType_(CollateralType_::Value_::OIS), proto.oisMarket}},
                           {{proto.forecastTenor, proto.forward3mMarket}}, proto.liborBasis);
    }
} // namespace

TEST(JointCalibrationTest, TestValidatorRejectsForwardDeclarationWithDiscountOnlyInstrument) {
    RegisterAll_::Init();
    const Date_ today(2024, 1, 15);
    const Ccy_ ccy("USD");

    JointMultiCurveCalibrationSpec_ spec;
    spec.today_ = today;
    spec.ccy_ = ccy.String();
    spec.liborBasis_ = DayBasis_("ACT_360");
    spec.solveMode_ = CurveSolveMode_::Value_::APPROXIMATE;

    JointCurveDeclaration_ oisDecl;
    oisDecl.curveName_ = "ois";
    oisDecl.instruments_ =
        Vector_<Handle_<YCInstrument_>>{Handle_<YCInstrument_>(new Deposit_(today, Date::AddMonths(today, 3), 0.01, DayBasis_("ACT_360")))};
    oisDecl.knotDates_ = SharedKnots(today);
    oisDecl.calibrateDiscountCurve_ = true;

    JointCurveDeclaration_ fwdDecl;
    fwdDecl.curveName_ = "fwd3m";
    // Deliberately construct an IBOR FRA with useProjectionCurve_ == false (the struct default).
    RateIndexConvention_ badConvention;
    badConvention.useProjectionCurve_ = false;
    badConvention.forecastTenor_ = PeriodLength_("3M");
    badConvention.dayBasis_ = DayBasis_("ACT_360");
    fwdDecl.instruments_ = Vector_<Handle_<YCInstrument_>>{
        Handle_<YCInstrument_>(new FRA_(today, Date::AddMonths(today, 1), Date::AddMonths(today, 4), 0.03, badConvention))};
    fwdDecl.knotDates_ = SharedKnots(today);
    fwdDecl.calibrateDiscountCurve_ = false;
    fwdDecl.targetTenor_ = PeriodLength_("3M");

    spec.curves_ = Vector_<JointCurveDeclaration_>{oisDecl, fwdDecl};

    try {
        const auto result = CalibrateJointMultiCurve(spec);
        FAIL() << "Expected Dal::Exception_ for forward declaration with useProjectionCurve_ == false";
    } catch (const Dal::Exception_& e) {
        ASSERT_TRUE(std::string(e.what()).find("useProjectionCurve_ = false") != std::string::npos)
            << "Message should name the offending flag, got: " << e.what();
        ASSERT_TRUE(std::string(e.what()).find("leaves the forward curve unconstrained") != std::string::npos)
            << "Message should explain the consequence, got: " << e.what();
    }
}

TEST(JointCalibrationTest, TestValidatorRejectsForwardDeclarationWithUnproducedCollateral) {
    RegisterAll_::Init();
    const Date_ today(2024, 1, 15);
    const Ccy_ ccy("USD");

    JointMultiCurveCalibrationSpec_ spec;
    spec.today_ = today;
    spec.ccy_ = ccy.String();
    spec.liborBasis_ = DayBasis_("ACT_360");
    spec.solveMode_ = CurveSolveMode_::Value_::APPROXIMATE;

    JointCurveDeclaration_ oisDecl;
    oisDecl.curveName_ = "ois";
    oisDecl.instruments_ =
        Vector_<Handle_<YCInstrument_>>{Handle_<YCInstrument_>(new Deposit_(today, Date::AddMonths(today, 3), 0.01, DayBasis_("ACT_360")))};
    oisDecl.knotDates_ = SharedKnots(today);
    oisDecl.calibrateDiscountCurve_ = true;
    oisDecl.targetCollateral_ = CollateralType_(CollateralType_::Value_::OIS);

    JointCurveDeclaration_ fwdDecl;
    fwdDecl.curveName_ = "fwd3m";
    RateIndexConvention_ okConvention;
    okConvention.useProjectionCurve_ = true;
    okConvention.forecastTenor_ = PeriodLength_("3M");
    okConvention.dayBasis_ = DayBasis_("ACT_360");
    okConvention.collateral_ = CollateralType_(CollateralType_::Value_::GC); // NOT produced by any discount decl
    fwdDecl.instruments_ = Vector_<Handle_<YCInstrument_>>{
        Handle_<YCInstrument_>(new FRA_(today, Date::AddMonths(today, 1), Date::AddMonths(today, 4), 0.03, okConvention))};
    fwdDecl.knotDates_ = SharedKnots(today);
    fwdDecl.calibrateDiscountCurve_ = false;
    fwdDecl.targetTenor_ = PeriodLength_("3M");
    fwdDecl.targetCollateral_ = CollateralType_(CollateralType_::Value_::GC); // not produced

    spec.curves_ = Vector_<JointCurveDeclaration_>{oisDecl, fwdDecl};

    try {
        const auto result = CalibrateJointMultiCurve(spec);
        FAIL() << "Expected Dal::Exception_ for forward declaration with unproduced targetCollateral_";
    } catch (const Dal::Exception_& e) {
        ASSERT_TRUE(std::string(e.what()).find("is not produced by any discount-curve declaration") != std::string::npos)
            << "Message should name the missing discount collateral, got: " << e.what();
    }
}

TEST(JointCalibrationTest, TestValidatorRejectsEmptyCurves) {
    RegisterAll_::Init();
    JointMultiCurveCalibrationSpec_ spec;
    spec.today_ = Date_(2024, 1, 15);
    spec.ccy_ = "USD";
    ASSERT_THROW(static_cast<void>(CalibrateJointMultiCurve(spec)), Dal::Exception_);
}

TEST(JointCalibrationTest, TestValidatorRejectsNoDiscountDeclaration) {
    RegisterAll_::Init();
    const Date_ today(2024, 1, 15);
    JointMultiCurveCalibrationSpec_ spec;
    spec.today_ = today;
    spec.ccy_ = "USD";
    JointCurveDeclaration_ fwd;
    fwd.instruments_ =
        Vector_<Handle_<YCInstrument_>>{Handle_<YCInstrument_>(new Deposit_(today, Date::AddMonths(today, 3), 0.01, DayBasis_("ACT_360")))};
    fwd.knotDates_ = SharedKnots(today);
    fwd.calibrateDiscountCurve_ = false;
    fwd.targetTenor_ = PeriodLength_("3M");
    spec.curves_ = Vector_<JointCurveDeclaration_>{fwd};
    ASSERT_THROW(static_cast<void>(CalibrateJointMultiCurve(spec)), Dal::Exception_);
}

TEST(JointCalibrationTest, TestJointCalibrationConvergesAndFitsInstruments) {
    RegisterAll_::Init();
    const Date_ today(2024, 1, 15);
    const Ccy_ ccy("USD");
    const PrototypeSet_ proto = BuildPrototypes(today, ccy);
    const JointMultiCurveCalibrationSpec_ spec = BuildCanonicalJointSpec(today, ccy, proto);

    const JointMultiCurveCalibrationResult_ result = CalibrateJointMultiCurve(spec);

    ASSERT_TRUE(result.converged_);
    ASSERT_EQ(result.diagnostics_.size(), 2);
    ASSERT_EQ(result.discountCurves_.size(), 1);
    ASSERT_EQ(result.forwardCurves_.size(), 1);
    ASSERT_NEAR(result.diagnostics_[0].maxAbsResidual_, 0.0, 1.0e-7);
    ASSERT_NEAR(result.diagnostics_[1].maxAbsResidual_, 0.0, 1.0e-7);
    ASSERT_NEAR(result.jointMaxAbsResidual_, 0.0, 1.0e-7);
    ASSERT_TRUE(result.solverEvaluations_ > 0);
}

TEST(JointCalibrationTest, TestSharedInternalExtractionPreservesDuplicateDisplayNamesAcrossDistinctSlots) {
    RegisterAll_::Init();
    const Date_ today(2024, 1, 15);
    const Ccy_ ccy("USD");
    const PrototypeSet_ proto = BuildPrototypes(today, ccy);
    JointMultiCurveCalibrationSpec_ spec = BuildCanonicalJointSpec(today, ccy, proto);
    spec.curves_[1].curveName_ = spec.curves_[0].curveName_;

    const JointMultiCurveCalibrationResult_ result = CalibrateJointMultiCurve(spec);

    ASSERT_TRUE(result.converged_);
    ASSERT_EQ(result.diagnostics_.size(), 2);
    ASSERT_EQ(result.diagnostics_[0].curveName_, result.diagnostics_[1].curveName_);
    ASSERT_LE(result.jointMaxAbsResidual_, 1.0e-7);
}

TEST(JointCalibrationTest, TestSharedInternalExtractionPreservesEmptyLegacyDisplayName) {
    RegisterAll_::Init();
    const Date_ today(2024, 1, 15);
    const Ccy_ ccy("USD");
    const PrototypeSet_ proto = BuildPrototypes(today, ccy);
    JointMultiCurveCalibrationSpec_ spec = BuildCanonicalJointSpec(today, ccy, proto);
    spec.curves_[0].curveName_.clear();

    const JointMultiCurveCalibrationResult_ result = CalibrateJointMultiCurve(spec);

    ASSERT_TRUE(result.converged_);
    ASSERT_EQ(result.diagnostics_.size(), 2);
    ASSERT_TRUE(result.diagnostics_[0].curveName_.empty());
    ASSERT_LE(result.jointMaxAbsResidual_, 1.0e-7);
}

TEST(JointCalibrationTest, TestHomogeneousZeroRateCalibrationPreservesDeclarationAndKnotOrder) {
    RegisterAll_::Init();
    const Date_ today(2024, 1, 15);
    const Ccy_ ccy("USD");
    const PrototypeSet_ proto = BuildPrototypes(today, ccy);
    JointMultiCurveCalibrationSpec_ spec = BuildCanonicalJointSpec(today, ccy, proto);
    spec.curves_[0].parameterization_ = CurveParameterization_::Value_::ZERO_RATE;
    spec.curves_[0].initialGuessPerNode_ = Vector_<>(spec.curves_[0].knotDates_.size(), 0.01);
    spec.curves_[1].parameterization_ = CurveParameterization_::Value_::ZERO_RATE;
    spec.curves_[1].baseLayeredOverDiscount_ = true;
    spec.curves_[1].initialGuessPerNode_ = Vector_<>(spec.curves_[1].knotDates_.size(), 0.03);

    JointMultiCurveCalibrationOptions_ options;
    options.jacobianMode_ = CurveJacobianMode_::Value_::BUMPED;
    options.computeJacobianAtSolution_ = false;
    const JointMultiCurveCalibrationResult_ result = CalibrateJointMultiCurve(spec, options);

    ASSERT_TRUE(result.converged_);
    ASSERT_EQ(result.diagnostics_.size(), 2);
    ASSERT_EQ(result.diagnostics_[0].curveIndex_, 0);
    ASSERT_EQ(result.diagnostics_[0].curveName_, spec.curves_[0].curveName_);
    ASSERT_EQ(result.diagnostics_[1].curveIndex_, 1);
    ASSERT_EQ(result.diagnostics_[1].curveName_, spec.curves_[1].curveName_);
    ASSERT_LT(result.jointMaxAbsResidual_, 1.0e-7);

    const auto* discount = dynamic_cast<const DiscountZeroRate_*>(result.discountCurves_.at(spec.curves_[0].targetCollateral_).get());
    const auto* forward = dynamic_cast<const DiscountZeroRate_*>(result.forwardCurves_.at(spec.curves_[1].targetTenor_).get());
    ASSERT_NE(discount, nullptr);
    ASSERT_NE(forward, nullptr);
    ASSERT_EQ(discount->AnchorDate(), today);
    ASSERT_EQ(forward->AnchorDate(), today);
    ASSERT_EQ(discount->NodeDates(), spec.curves_[0].knotDates_);
    ASSERT_EQ(forward->NodeDates(), spec.curves_[1].knotDates_);
    ASSERT_EQ(discount->NodeZeroRates().size(), spec.curves_[0].knotDates_.size());
    ASSERT_EQ(forward->NodeZeroRates().size(), spec.curves_[1].knotDates_.size());

    const CurveDefinition_ discountDefinition =
        MakeCurveDefinition(spec.curves_[0].curveName_, spec.ccy_, spec.curves_[0].parameterization_, spec.curves_[0].logDfScheme_,
                            spec.curves_[0].knotDates_, spec.today_, spec.liborBasis_);
    const CurveDefinition_ forwardDefinition =
        MakeCurveDefinition(spec.curves_[1].curveName_, spec.ccy_, spec.curves_[1].parameterization_, spec.curves_[1].logDfScheme_,
                            spec.curves_[1].knotDates_, spec.today_, spec.liborBasis_);
    ASSERT_EQ(BuildCurveParameterLayout(discountDefinition).parameterCount_, static_cast<int>(discount->NodeZeroRates().size()));
    ASSERT_EQ(BuildCurveParameterLayout(forwardDefinition).parameterCount_, static_cast<int>(forward->NodeZeroRates().size()));
}

TEST(JointCalibrationTest, TestJointOisCurveAgreesWithStagedOis) {
    RegisterAll_::Init();
    const Date_ today(2024, 1, 15);
    const Ccy_ ccy("USD");
    const PrototypeSet_ proto = BuildPrototypes(today, ccy);
    const JointMultiCurveCalibrationSpec_ jointSpec = BuildCanonicalJointSpec(today, ccy, proto);
    // This regression measures joint-vs-staged construction drift, so both underdetermined
    // solves must use the same local-linearization method. BUMPED is the historical PWL path.
    JointMultiCurveCalibrationOptions_ jointOptions;
    jointOptions.jacobianMode_ = CurveJacobianMode_::Value_::BUMPED;
    jointOptions.computeJacobianAtSolution_ = false;
    const JointMultiCurveCalibrationResult_ jointResult = CalibrateJointMultiCurve(jointSpec, jointOptions);

    CurveCalibrationSpec_ oisStage;
    oisStage.today_ = today;
    oisStage.ccy_ = ccy.String();
    oisStage.curveName_ = "staged_ois";
    oisStage.instruments_ = proto.ois;
    oisStage.knotDates_ = SharedKnots(today);
    oisStage.targetCollateral_ = CollateralType_(CollateralType_::Value_::OIS);
    oisStage.solveMode_ = CurveSolveMode_::Value_::APPROXIMATE;
    oisStage.fitTolerance_ = 1.0e-8;
    oisStage.liborBasis_ = proto.liborBasis;
    CurveCalibrationOptions_ stagedOptions;
    stagedOptions.jacobianMode_ = CurveJacobianMode_::Value_::BUMPED;
    stagedOptions.computeEffJacobianInverse_ = false;
    stagedOptions.computeForwardJacobian_ = false;
    const CurveCalibrationResult_ stagedResult = CalibrateYieldCurve(oisStage, stagedOptions);

    const Handle_<DiscountCurve_>& jointOis = jointResult.discountCurves_.at(CollateralType_(CollateralType_::Value_::OIS));
    const DiscountCurve_& stagedOis = *stagedResult.curve_;
    const Vector_<Date_> pillars = {
        Date::AddMonths(today, 12), Date::AddMonths(today, 24), Date::AddMonths(today, 36),
        Date::AddMonths(today, 60), Date::AddMonths(today, 84), Date::AddMonths(today, 120),
    };
    double maxDiff = 0.0;
    for (const auto& pillar : pillars) {
        const double dfJoint = (*jointOis)(today, pillar);
        const double dfStaged = stagedOis(today, pillar);
        ASSERT_GT(dfJoint, 0.0);
        ASSERT_LE(dfJoint, 1.0);
        maxDiff = std::max(maxDiff, std::fabs(dfJoint - dfStaged));
    }
    // 1e-5: joint co-optimization drifts OIS DFs by ~7e-6; a mis-routed OIS would show ~1e-2.
    ASSERT_LE(maxDiff, 1.0e-5) << "Joint-vs-staged OIS DF diff " << maxDiff << " exceeds 1e-5";
}

TEST(JointCalibrationTest, TestJointForwardCurveAgreesWithStagedMultiCurve) {
    RegisterAll_::Init();
    const Date_ today(2024, 1, 15);
    const Ccy_ ccy("USD");
    const PrototypeSet_ proto = BuildPrototypes(today, ccy);
    const JointMultiCurveCalibrationSpec_ jointSpec = BuildCanonicalJointSpec(today, ccy, proto);
    const JointMultiCurveCalibrationResult_ jointResult = CalibrateJointMultiCurve(jointSpec);

    CurveCalibrationSpec_ oisStage;
    oisStage.today_ = today;
    oisStage.ccy_ = ccy.String();
    oisStage.curveName_ = "staged_ois";
    oisStage.instruments_ = proto.ois;
    oisStage.knotDates_ = SharedKnots(today);
    oisStage.targetCollateral_ = CollateralType_(CollateralType_::Value_::OIS);
    oisStage.solveMode_ = CurveSolveMode_::Value_::APPROXIMATE;
    oisStage.fitTolerance_ = 1.0e-8;
    oisStage.liborBasis_ = proto.liborBasis;

    CurveCalibrationSpec_ liborStage;
    liborStage.today_ = today;
    liborStage.ccy_ = ccy.String();
    liborStage.curveName_ = "staged_3m";
    liborStage.instruments_ = proto.libor;
    liborStage.knotDates_ = SharedKnots(today);
    liborStage.targetCollateral_ = CollateralType_(CollateralType_::Value_::OIS);
    liborStage.targetTenor_ = proto.forecastTenor;
    liborStage.calibrateDiscountCurve_ = false;
    liborStage.solveMode_ = CurveSolveMode_::Value_::APPROXIMATE;
    liborStage.fitTolerance_ = 1.0e-8;
    liborStage.liborBasis_ = proto.liborBasis;

    MultiCurveCalibrationSpec_ multi;
    multi.ccy_ = ccy.String();
    multi.liborBasis_ = proto.liborBasis;
    multi.stages_ = Vector_<CurveCalibrationSpec_>{oisStage, liborStage};
    const MultiCurveCalibrationResult_ stagedResult = CalibrateMultiCurve(multi);

    const Handle_<DiscountCurve_>& joint3m = jointResult.forwardCurves_.at(proto.forecastTenor);
    const Handle_<DiscountCurve_>& staged3m = stagedResult.forwardCurves_.at(proto.forecastTenor);

    const Vector_<Date_> pillars = {
        Date::AddMonths(today, 12), Date::AddMonths(today, 24), Date::AddMonths(today, 36),
        Date::AddMonths(today, 60), Date::AddMonths(today, 84), Date::AddMonths(today, 120),
    };
    double maxDiff = 0.0;
    for (const auto& pillar : pillars) {
        const double dfJoint = (*joint3m)(today, pillar);
        const double dfStaged = (*staged3m)(today, pillar);
        ASSERT_GT(dfJoint, 0.0);
        ASSERT_LE(dfJoint, 1.0);
        maxDiff = std::max(maxDiff, std::fabs(dfJoint - dfStaged));
    }
    // 5e-3: short-end drift ~1.2e-3 from co-optimization; a mis-routed 3M forecast would show ~1e-2.
    ASSERT_LE(maxDiff, 5.0e-3) << "Joint-vs-staged 3M DF diff " << maxDiff << " exceeds 5e-3";
}

TEST(JointCalibrationTest, TestJointForwardCurveHasNoBaseHandle) {
    RegisterAll_::Init();
    const Date_ today(2024, 1, 15);
    const Ccy_ ccy("USD");
    const PrototypeSet_ proto = BuildPrototypes(today, ccy);
    const JointMultiCurveCalibrationSpec_ jointSpec = BuildCanonicalJointSpec(today, ccy, proto);
    const JointMultiCurveCalibrationResult_ jointResult = CalibrateJointMultiCurve(jointSpec);

    // Staged path for the structural comparison.
    CurveCalibrationSpec_ oisStage;
    oisStage.today_ = today;
    oisStage.ccy_ = ccy.String();
    oisStage.curveName_ = "staged_ois";
    oisStage.instruments_ = proto.ois;
    oisStage.knotDates_ = SharedKnots(today);
    oisStage.targetCollateral_ = CollateralType_(CollateralType_::Value_::OIS);
    oisStage.solveMode_ = CurveSolveMode_::Value_::APPROXIMATE;
    oisStage.fitTolerance_ = 1.0e-8;
    oisStage.liborBasis_ = proto.liborBasis;
    CurveCalibrationSpec_ liborStage;
    liborStage.today_ = today;
    liborStage.ccy_ = ccy.String();
    liborStage.curveName_ = "staged_3m";
    liborStage.instruments_ = proto.libor;
    liborStage.knotDates_ = SharedKnots(today);
    liborStage.targetCollateral_ = CollateralType_(CollateralType_::Value_::OIS);
    liborStage.targetTenor_ = proto.forecastTenor;
    liborStage.calibrateDiscountCurve_ = false;
    liborStage.solveMode_ = CurveSolveMode_::Value_::APPROXIMATE;
    liborStage.fitTolerance_ = 1.0e-8;
    liborStage.liborBasis_ = proto.liborBasis;
    MultiCurveCalibrationSpec_ multi;
    multi.ccy_ = ccy.String();
    multi.liborBasis_ = proto.liborBasis;
    multi.stages_ = Vector_<CurveCalibrationSpec_>{oisStage, liborStage};
    const MultiCurveCalibrationResult_ stagedResult = CalibrateMultiCurve(multi);

    const Handle_<DiscountCurve_>& joint3m = jointResult.forwardCurves_.at(proto.forecastTenor);
    const Handle_<DiscountCurve_>& staged3m = stagedResult.forwardCurves_.at(proto.forecastTenor);

    Vector_<const YCComponent_*> jointComponents;
    joint3m->Poll(&jointComponents);
    Vector_<const YCComponent_*> stagedComponents;
    staged3m->Poll(&stagedComponents);

    // Joint 3M: raw PWL, no base -> exactly 1 component (itself).
    ASSERT_EQ(jointComponents.size(), 1u) << "Joint 3M curve polls " << jointComponents.size() << " components -- expected 1 (no base handle)";
    // Staged 3M: DiscountPWLF_ with base_ = OIS -> at least 2 components (itself + the OIS curve).
    ASSERT_GE(stagedComponents.size(), 2u) << "Staged 3M curve polls " << stagedComponents.size()
                                           << " components -- expected >= 2 (base handle = OIS)";

    const Date_ pillar = Date::AddMonths(today, 60);
    ASSERT_NEAR((*joint3m)(today, pillar), (*staged3m)(today, pillar), 5.0e-3);
}

JointMultiCurveCalibrationSpec_ BuildBaseLayeredJointSpec(const Date_& today, const Ccy_& ccy, const PrototypeSet_& proto) {
    JointMultiCurveCalibrationSpec_ spec = BuildCanonicalJointSpec(today, ccy, proto);
    // Match the example's EXACT mode for a fair joint-vs-staged comparison (the canonical helper
    // defaults to APPROXIMATE; EXACT resolves the cross-curve coupling tightly).
    spec.solveMode_ = CurveSolveMode_::Value_::EXACT;
    // Flip the 3M declaration to base-layer-over-OIS (the example's Option 1 representation).
    JointCurveDeclaration_& liborDecl = spec.curves_[1];
    REQUIRE(liborDecl.curveName_ == "joint_3m", "Test setup: second declaration must be the 3M forward curve");
    liborDecl.baseLayeredOverDiscount_ = true;
    return spec;
}

TEST(JointCalibrationTest, TestBaseLayeredForwardCurveCarriesBaseHandle) {
    RegisterAll_::Init();
    const Date_ today(2024, 1, 15);
    const Ccy_ ccy("USD");
    const PrototypeSet_ proto = BuildPrototypes(today, ccy);
    const JointMultiCurveCalibrationSpec_ spec = BuildBaseLayeredJointSpec(today, ccy, proto);
    const JointMultiCurveCalibrationResult_ result = CalibrateJointMultiCurve(spec);

    ASSERT_TRUE(result.converged_);
    const Handle_<DiscountCurve_>& joint3m = result.forwardCurves_.at(proto.forecastTenor);
    Vector_<const YCComponent_*> components;
    joint3m->Poll(&components);
    ASSERT_GE(components.size(), 2u) << "Base-layered joint 3M should poll >= 2 components (itself + OIS base), got " << components.size();
}

TEST(JointCalibrationTest, TestBaseLayeredJointForwardAgreesWithStaged) {
    // With base layering, both the joint and staged 3M curves smooth the OIS spread (the staged path
    // base-layers via ApplyStageDefaults, the joint path via baseLayeredOverDiscount_). The stored
    // curves are structurally identical (DiscountPWLF_ with base = OIS), so their DF outputs agree
    // far tighter than the baseless joint 3M (whose short-end drift was ~1e-3 under APPROXIMATE).
    // The joint co-optimization still lands a different OIS slice than staged's standalone OIS, so
    // agreement is bounded by that OIS drift rather than round-off; 1e-3 retains wide margin over
    // the percent-level drift a mis-routing (B-new-1) would produce while confirming the base wiring.
    RegisterAll_::Init();
    const Date_ today(2024, 1, 15);
    const Ccy_ ccy("USD");
    const PrototypeSet_ proto = BuildPrototypes(today, ccy);
    const JointMultiCurveCalibrationSpec_ jointSpec = BuildBaseLayeredJointSpec(today, ccy, proto);
    const JointMultiCurveCalibrationResult_ jointResult = CalibrateJointMultiCurve(jointSpec);

    CurveCalibrationSpec_ oisStage;
    oisStage.today_ = today;
    oisStage.ccy_ = ccy.String();
    oisStage.curveName_ = "staged_ois";
    oisStage.instruments_ = proto.ois;
    oisStage.knotDates_ = SharedKnots(today);
    oisStage.targetCollateral_ = CollateralType_(CollateralType_::Value_::OIS);
    oisStage.solveMode_ = CurveSolveMode_::Value_::EXACT;
    oisStage.fitTolerance_ = 1.0e-8;
    oisStage.liborBasis_ = proto.liborBasis;
    CurveCalibrationSpec_ liborStage;
    liborStage.today_ = today;
    liborStage.ccy_ = ccy.String();
    liborStage.curveName_ = "staged_3m";
    liborStage.instruments_ = proto.libor;
    liborStage.knotDates_ = SharedKnots(today);
    liborStage.targetCollateral_ = CollateralType_(CollateralType_::Value_::OIS);
    liborStage.targetTenor_ = proto.forecastTenor;
    liborStage.calibrateDiscountCurve_ = false;
    liborStage.solveMode_ = CurveSolveMode_::Value_::EXACT;
    liborStage.fitTolerance_ = 1.0e-8;
    liborStage.liborBasis_ = proto.liborBasis;
    MultiCurveCalibrationSpec_ multi;
    multi.ccy_ = ccy.String();
    multi.liborBasis_ = proto.liborBasis;
    multi.stages_ = Vector_<CurveCalibrationSpec_>{oisStage, liborStage};
    const MultiCurveCalibrationResult_ stagedResult = CalibrateMultiCurve(multi);

    const Handle_<DiscountCurve_>& joint3m = jointResult.forwardCurves_.at(proto.forecastTenor);
    const Handle_<DiscountCurve_>& staged3m = stagedResult.forwardCurves_.at(proto.forecastTenor);
    const Vector_<Date_> pillars = {
        Date::AddMonths(today, 12), Date::AddMonths(today, 24), Date::AddMonths(today, 36),
        Date::AddMonths(today, 60), Date::AddMonths(today, 84), Date::AddMonths(today, 120),
    };
    double maxDiff = 0.0;
    for (const auto& pillar : pillars)
        maxDiff = std::max(maxDiff, std::fabs((*joint3m)(today, pillar) - (*staged3m)(today, pillar)));
    // EXACT measured max drift ~2.4e-5 (boundary-dominated; core 2Y-7Y at ~1e-7). The joint solve
    // co-optimizes OIS and 3M-spread, so its OIS slice lands a few e-7 off staged's standalone OIS
    // and that propagates through the base handle. 1e-3 retains wide margin over the percent-level
    // drift a mis-routing (B-new-1) would produce, while confirming the base wiring is correct.
    ASSERT_LE(maxDiff, 1.0e-3) << "Base-layered joint-vs-staged 3M DF diff " << maxDiff << " exceeds 1e-3";
}

TEST(JointCalibrationTest, TestValidatorRejectsBaseLayeringOnDiscountDeclaration) {
    RegisterAll_::Init();
    const Date_ today(2024, 1, 15);
    const Ccy_ ccy("USD");
    const PrototypeSet_ proto = BuildPrototypes(today, ccy);
    JointMultiCurveCalibrationSpec_ spec = BuildCanonicalJointSpec(today, ccy, proto);
    // Erroneously set base layering on the OIS discount declaration.
    spec.curves_[0].baseLayeredOverDiscount_ = true;
    ASSERT_THROW(static_cast<void>(CalibrateJointMultiCurve(spec)), Dal::Exception_);
}

TEST(JointCalibrationTest, TestValidatorAcceptsBaseLayeringOnPWC) {
    RegisterAll_::Init();
    const Date_ today(2024, 1, 15);
    const Ccy_ ccy("USD");
    const PrototypeSet_ proto = BuildPrototypes(today, ccy);
    JointMultiCurveCalibrationSpec_ spec = BuildCanonicalJointSpec(today, ccy, proto);
    JointCurveDeclaration_& liborDecl = spec.curves_[1];
    liborDecl.baseLayeredOverDiscount_ = true;
    liborDecl.parameterization_ = CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD;
    liborDecl.initialGuessPerNode_ = Vector_<>(liborDecl.knotDates_.size(), 0.005);
    const JointMultiCurveCalibrationResult_ result = CalibrateJointMultiCurve(spec);
    ASSERT_TRUE(result.converged_);
    const Handle_<DiscountCurve_>& forward = result.forwardCurves_.at(liborDecl.targetTenor_);
    Vector_<const YCComponent_*> components;
    forward->Poll(&components);
    ASSERT_GE(components.size(), 2u);
}

TEST(JointCalibrationTest, TestDiagnosticsFieldsArePopulated) {
    RegisterAll_::Init();
    const Date_ today(2024, 1, 15);
    const Ccy_ ccy("USD");
    const PrototypeSet_ proto = BuildPrototypes(today, ccy);
    const JointMultiCurveCalibrationSpec_ spec = BuildCanonicalJointSpec(today, ccy, proto);
    const JointMultiCurveCalibrationResult_ result = CalibrateJointMultiCurve(spec);

    ASSERT_EQ(result.diagnostics_.size(), spec.curves_.size());
    for (int i = 0; i < static_cast<int>(result.diagnostics_.size()); ++i) {
        const JointCurveCalibrationDiagnostics_& diag = result.diagnostics_[i];
        ASSERT_EQ(diag.curveIndex_, i);
        ASSERT_EQ(diag.curveName_, spec.curves_[i].curveName_);
        ASSERT_EQ(static_cast<int>(diag.instrumentNames_.size()), static_cast<int>(spec.curves_[i].instruments_.size()));
        ASSERT_EQ(static_cast<int>(diag.marketRates_.size()), static_cast<int>(spec.curves_[i].instruments_.size()));
        ASSERT_EQ(static_cast<int>(diag.modelRates_.size()), static_cast<int>(spec.curves_[i].instruments_.size()));
        ASSERT_EQ(static_cast<int>(diag.residuals_.size()), static_cast<int>(spec.curves_[i].instruments_.size()));
        // APPROXIMATE solve mode -> usedApproximateFit_ == true.
        ASSERT_TRUE(diag.usedApproximateFit_);
        // rmsResidual_ must be <= maxAbsResidual_ (RMS <= max for any residual vector).
        ASSERT_LE(diag.rmsResidual_, diag.maxAbsResidual_ + 1.0e-15);
    }
    // jointRmsResidual_ must be <= jointMaxAbsResidual_.
    ASSERT_LE(result.jointRmsResidual_, result.jointMaxAbsResidual_ + 1.0e-15);
}

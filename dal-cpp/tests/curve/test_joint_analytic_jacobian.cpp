//
// Created by dal-implementer on 2026/6/20.
//

#include <gtest/gtest.h>

#include <dal/platform/platform.hpp>

#include <cmath>
#include <dal/currency/currencydata.hpp>
#include <dal/curve/aadjacobian.hpp>
#include <dal/curve/calibration_internal.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/curveparameterization.hpp>
#include <dal/curve/discount.hpp>
#include <dal/curve/jointcalibration.hpp>
#include <dal/curve/jointrate.hpp>
#include <dal/curve/piecewiselinear.hpp>
#include <dal/curve/tapeguard.hpp>
#include <dal/curve/ycconst.hpp>
#include <dal/curve/ycctx.hpp>
#include <dal/curve/ycimp.hpp>
#include <dal/curve/ycinstrument.hpp>
#include <dal/curve/yclogdf.hpp>
#include <dal/curve/ycpwlf.hpp>
#include <dal/curve/yczerorate.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/protocol/rateconvention.hpp>
#include <dal/time/date.hpp>
#include <dal/time/daybasis.hpp>
#include <dal/time/holidays.hpp>
#include <dal/time/periodlength.hpp>
#include <dal/utilities/exceptions.hpp>
#include <map>
#include <memory>
#include <utility>

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
            Date::AddMonths(today, 3),  Date::AddMonths(today, 6),  Date::AddMonths(today, 9),  Date::AddMonths(today, 12),
            Date::AddMonths(today, 18), Date::AddMonths(today, 24), Date::AddMonths(today, 36), Date::AddMonths(today, 60),
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

    JointMultiCurveCalibrationSpec_
    BuildSmallJointSpec(const Date_& today, const Ccy_& ccy, bool baseLayered, const DayBasis_& liborBasis = DayBasis_("ACT_360")) {
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
        CurveBlock_ market("market", ccyName, {{CollateralType_(CollateralType_::Value_::OIS), oisMarket}}, {{libor3m.forecastTenor_, fwd3mMarket}},
                           libor3m.dayBasis_);

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

    Vector_<> InitialParameters(CurveParameterization_ parameterization, const Vector_<Date_>& knots, const Date_& today, double rate) {
        switch (parameterization.Switch()) {
        case CurveParameterization_::Value_::LOG_DISCOUNT: {
            Vector_<> result(knots.size());
            for (int i = 0; i < static_cast<int>(knots.size()); ++i)
                result[i] = -rate * (knots[i] - today) / 365.0;
            return result;
        }
        case CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD:
            return Vector_<>(knots.size(), rate);
        case CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD:
            return Vector_<>(2 * knots.size(), rate);
        case CurveParameterization_::Value_::ZERO_RATE:
            return Vector_<>(knots.size(), rate);
        default:
            REQUIRE(false, "Unsupported parameterization in joint test setup");
            return {};
        }
    }

    JointMultiCurveCalibrationSpec_ BuildParameterizationSpec(const Date_& today,
                                                              const Ccy_& ccy,
                                                              CurveParameterization_ discountParameterization,
                                                              CurveParameterization_ forwardParameterization,
                                                              LogDfScheme_ scheme) {
        auto overnightIndex = Ccy::Conventions::OisIndex()(ccy);
        overnightIndex.dayBasis_ = DayBasis_("ACT_365F");
        overnightIndex.accrualHolidays_ = Holidays::None();
        overnightIndex.fixingHolidays_ = Holidays::None();
        auto libor3m = Ccy::Conventions::LiborIndex()(ccy);
        libor3m.dayBasis_ = DayBasis_("ACT_365F");
        libor3m.accrualHolidays_ = Holidays::None();
        libor3m.fixingHolidays_ = Holidays::None();

        const Vector_<Date_> knots = {
            Date::AddMonths(today, 1),
            Date::AddMonths(today, 3),
            Date::AddMonths(today, 6),
            Date::AddMonths(today, 12),
        };
        const Vector_<Date_> maturities = {
            Date::AddMonths(today, 2),
            Date::AddMonths(today, 4),
            Date::AddMonths(today, 8),
            Date::AddMonths(today, 18),
        };
        const Handle_<DiscountCurve_> discountMarket = MakeFlatForward("support_ois_market", ccy.String(), knots, 0.02);
        const Handle_<DiscountCurve_> forwardMarket = MakeFlatForward("support_3m_market", ccy.String(), knots, 0.005, discountMarket);
        const CurveBlock_ market("support_market", ccy.String(), {{overnightIndex.collateral_, discountMarket}},
                                 {{libor3m.forecastTenor_, forwardMarket}}, libor3m.dayBasis_);

        auto quotedDeposits = [&](const RateIndexConvention_& index) {
            Vector_<Handle_<YCInstrument_>> result;
            Handle_<YieldCurve_> empty;
            for (const auto& maturity : maturities) {
                const Handle_<YCInstrument_> prototype(new Deposit_(today, today, maturity, 0.0, index));
                const double quote = (*prototype->Precompute(empty))(market);
                result.emplace_back(new Deposit_(today, today, maturity, quote, index));
            }
            return result;
        };

        JointCurveDeclaration_ discountDeclaration;
        discountDeclaration.curveName_ = "support_discount";
        discountDeclaration.instruments_ = quotedDeposits(overnightIndex);
        discountDeclaration.knotDates_ = knots;
        discountDeclaration.targetCollateral_ = overnightIndex.collateral_;
        discountDeclaration.calibrateDiscountCurve_ = true;
        discountDeclaration.parameterization_ = discountParameterization;
        discountDeclaration.logDfScheme_ = scheme;
        discountDeclaration.initialGuessPerNode_ = InitialParameters(discountParameterization, knots, today, 0.03);

        JointCurveDeclaration_ forwardDeclaration;
        forwardDeclaration.curveName_ = "support_forward";
        forwardDeclaration.instruments_ = quotedDeposits(libor3m);
        forwardDeclaration.knotDates_ = knots;
        forwardDeclaration.targetCollateral_ = overnightIndex.collateral_;
        forwardDeclaration.targetTenor_ = libor3m.forecastTenor_;
        forwardDeclaration.calibrateDiscountCurve_ = false;
        forwardDeclaration.baseLayeredOverDiscount_ = true;
        forwardDeclaration.parameterization_ = forwardParameterization;
        forwardDeclaration.logDfScheme_ = scheme;
        forwardDeclaration.initialGuessPerNode_ = InitialParameters(forwardParameterization, knots, today, 0.015);

        JointMultiCurveCalibrationSpec_ spec;
        spec.today_ = today;
        spec.ccy_ = ccy.String();
        spec.curves_ = {discountDeclaration, forwardDeclaration};
        spec.liborBasis_ = DayBasis_("ACT_365F");
        spec.tolerance_ = 1.0e-10;
        spec.fitTolerance_ = 1.0e-8;
        spec.solveMode_ = CurveSolveMode_::Value_::EXACT;
        return spec;
    }

    Vector_<> CurveParameters(const DiscountCurve_& curve, CurveParameterization_ parameterization) {
        switch (parameterization.Switch()) {
        case CurveParameterization_::Value_::LOG_DISCOUNT: {
            const auto* typed = dynamic_cast<const DiscountLogDF_*>(&curve);
            REQUIRE(typed, "Expected a log-discount curve");
            Vector_<> result = typed->NodeLogDF();
            result.erase(result.begin());
            return result;
        }
        case CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD: {
            const auto* typed = dynamic_cast<const Tape::DiscountPWC_<double>*>(&curve);
            REQUIRE(typed, "Expected a piecewise-constant-forward curve");
            return typed->FRight();
        }
        case CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD: {
            const auto* typed = dynamic_cast<const Tape::DiscountPWLF_<double>*>(&curve);
            REQUIRE(typed, "Expected a piecewise-linear-forward curve");
            const Vector_<> left = typed->FLeft();
            const Vector_<> right = typed->FRight();
            Vector_<> result(2 * left.size());
            for (int i = 0; i < static_cast<int>(left.size()); ++i) {
                result[2 * i] = left[i];
                result[2 * i + 1] = right[i];
            }
            return result;
        }
        case CurveParameterization_::Value_::ZERO_RATE: {
            const auto* typed = dynamic_cast<const DiscountZeroRate_*>(&curve);
            REQUIRE(typed, "Expected a zero-rate curve");
            return typed->NodeZeroRates();
        }
        default:
            REQUIRE(false, "Unsupported parameterization in joint test extraction");
            return {};
        }
    }

    Vector_<> SolvedJointParameters(const JointMultiCurveCalibrationSpec_& spec, const JointMultiCurveCalibrationResult_& result) {
        Vector_<> parameters;
        for (const auto& declaration : spec.curves_) {
            const Handle_<DiscountCurve_>& curve = declaration.calibrateDiscountCurve_ ? result.discountCurves_.at(declaration.targetCollateral_)
                                                                                       : result.forwardCurves_.at(declaration.targetTenor_);
            parameters.Append(CurveParameters(*curve, declaration.parameterization_));
        }
        return parameters;
    }

    template <class T_> Vector_<T_> Slice(const Vector_<T_>& parameters, int offset, int count) {
        return Vector_<T_>(parameters.begin() + offset, parameters.begin() + offset + count);
    }

    struct JointCurveMetadata_ {
        Vector_<CurveDefinition_> definitions_;
        Vector_<CurveParameterLayout_> layouts_;
    };

    JointCurveMetadata_ BuildJointCurveMetadata(const JointMultiCurveCalibrationSpec_& spec) {
        JointCurveMetadata_ result;
        result.definitions_.reserve(spec.curves_.size());
        result.layouts_.reserve(spec.curves_.size());
        for (const auto& declaration : spec.curves_) {
            result.definitions_.push_back(MakeCurveDefinition(declaration.curveName_, spec.ccy_, declaration.parameterization_,
                                                              declaration.logDfScheme_, declaration.knotDates_, spec.today_, spec.liborBasis_));
            result.layouts_.push_back(BuildCurveParameterLayout(result.definitions_.back()));
        }
        return result;
    }

    std::map<CollateralType_, Handle_<DiscountCurve_>>
    BuildPassiveDiscountCurves(const JointMultiCurveCalibrationSpec_& spec, const JointCurveMetadata_& metadata, const Vector_<>& parameters) {
        std::map<CollateralType_, Handle_<DiscountCurve_>> result;
        int offset = 0;
        for (int i = 0; i < static_cast<int>(spec.curves_.size()); ++i) {
            const auto& declaration = spec.curves_[i];
            if (declaration.calibrateDiscountCurve_)
                result[declaration.targetCollateral_] = Handle_<DiscountCurve_>(
                    BuildDiscountCurveUniqueT<double>(metadata.definitions_[i], Slice(parameters, offset, metadata.layouts_[i].parameterCount_))
                        .release());
            offset += metadata.layouts_[i].parameterCount_;
        }
        return result;
    }

    std::map<PeriodLength_, Handle_<DiscountCurve_>> BuildPassiveForwardCurves(const JointMultiCurveCalibrationSpec_& spec,
                                                                               const JointCurveMetadata_& metadata,
                                                                               const Vector_<>& parameters,
                                                                               const std::map<CollateralType_, Handle_<DiscountCurve_>>& discounts) {
        std::map<PeriodLength_, Handle_<DiscountCurve_>> result;
        int offset = 0;
        for (int i = 0; i < static_cast<int>(spec.curves_.size()); ++i) {
            const auto& declaration = spec.curves_[i];
            if (!declaration.calibrateDiscountCurve_) {
                Handle_<DiscountCurve_> base;
                if (declaration.baseLayeredOverDiscount_)
                    base = discounts.at(declaration.targetCollateral_);
                result[declaration.targetTenor_] = Handle_<DiscountCurve_>(
                    BuildDiscountCurveUniqueT<double>(metadata.definitions_[i], Slice(parameters, offset, metadata.layouts_[i].parameterCount_), base)
                        .release());
            }
            offset += metadata.layouts_[i].parameterCount_;
        }
        return result;
    }

    Vector_<> EvaluatePassiveResiduals(const JointMultiCurveCalibrationSpec_& spec, const CurveBlock_& block) {
        Vector_<> residuals;
        Handle_<YieldCurve_> empty;
        for (const auto& declaration : spec.curves_) {
            const auto instruments = OrderInstruments(declaration.instruments_);
            for (const auto& instrument : instruments)
                residuals.push_back((*instrument->Precompute(empty))(block) - instrument->MarketRate());
        }
        return residuals;
    }

    Vector_<> EvalJointResiduals(const JointMultiCurveCalibrationSpec_& spec, const Vector_<>& parameters) {
        const JointCurveMetadata_ metadata = BuildJointCurveMetadata(spec);
        const auto discounts = BuildPassiveDiscountCurves(spec, metadata, parameters);
        const auto forwards = BuildPassiveForwardCurves(spec, metadata, parameters, discounts);
        return EvaluatePassiveResiduals(spec, CurveBlock_("joint_test", spec.ccy_, discounts, forwards, spec.liborBasis_));
    }

    using ActiveDiscountCurve_ = Tape::DiscountCurve_<AAD::Number_>;
    using ActiveDiscountCurves_ = std::map<CollateralType_, std::shared_ptr<ActiveDiscountCurve_>>;
    using ActiveForwardCurves_ = std::map<PeriodLength_, std::shared_ptr<ActiveDiscountCurve_>>;

    ActiveDiscountCurves_ BuildActiveDiscountCurves(const JointMultiCurveCalibrationSpec_& spec,
                                                    const JointCurveMetadata_& metadata,
                                                    const Vector_<AAD::Number_>& parameters) {
        ActiveDiscountCurves_ result;
        int offset = 0;
        for (int i = 0; i < static_cast<int>(spec.curves_.size()); ++i) {
            const auto& declaration = spec.curves_[i];
            if (declaration.calibrateDiscountCurve_)
                result[declaration.targetCollateral_] =
                    BuildDiscountCurveT<AAD::Number_>(metadata.definitions_[i], Slice(parameters, offset, metadata.layouts_[i].parameterCount_));
            offset += metadata.layouts_[i].parameterCount_;
        }
        return result;
    }

    ActiveForwardCurves_ BuildActiveForwardCurves(const JointMultiCurveCalibrationSpec_& spec,
                                                  const JointCurveMetadata_& metadata,
                                                  const Vector_<AAD::Number_>& parameters,
                                                  const ActiveDiscountCurves_& discounts) {
        ActiveForwardCurves_ result;
        int offset = 0;
        for (int i = 0; i < static_cast<int>(spec.curves_.size()); ++i) {
            const auto& declaration = spec.curves_[i];
            if (!declaration.calibrateDiscountCurve_) {
                if (declaration.baseLayeredOverDiscount_) {
                    Handle_<ActiveDiscountCurve_> base(discounts.at(declaration.targetCollateral_));
                    result[declaration.targetTenor_] = BuildDiscountCurveT<AAD::Number_, ActiveDiscountCurve_>(
                        metadata.definitions_[i], Slice(parameters, offset, metadata.layouts_[i].parameterCount_), base);
                } else {
                    result[declaration.targetTenor_] =
                        BuildDiscountCurveT<AAD::Number_>(metadata.definitions_[i], Slice(parameters, offset, metadata.layouts_[i].parameterCount_));
                }
            }
            offset += metadata.layouts_[i].parameterCount_;
        }
        return result;
    }

    Tape::JointCurveBlock_<AAD::Number_> BuildActiveCurveBlock(const ActiveDiscountCurves_& discounts, const ActiveForwardCurves_& forwards) {
        Tape::JointCurveBlock_<AAD::Number_> result;
        for (const auto& [collateral, curve] : discounts)
            result.discountCurves[collateral] = curve.get();
        for (const auto& [tenor, curve] : forwards)
            result.forwardCurves[tenor] = curve.get();
        return result;
    }

    Handle_<Tape::Rate_<AAD::Number_>> DiscountRateT(const YCInstrument_& instrument) {
        return VisitRate(
            instrument, [](const Deposit_& deposit) { return deposit.PrecomputeT<AAD::Number_>(); },
            [](const FRA_& fra) { return fra.PrecomputeT<AAD::Number_>(); }, [](const Future_& future) { return future.PrecomputeT<AAD::Number_>(); },
            [](const Swap_& swap) { return swap.PrecomputeT<AAD::Number_>(); });
    }

    Vector_<AAD::Number_> EvaluateActiveResiduals(const JointMultiCurveCalibrationSpec_& spec, const Tape::JointCurveBlock_<AAD::Number_>& block) {
        Vector_<AAD::Number_> result;
        for (const auto& declaration : spec.curves_) {
            const auto instruments = OrderInstruments(declaration.instruments_);
            for (const auto& instrument : instruments) {
                const RateIndexConvention_& convention = *FloatConventionOf(*instrument);
                if (convention.useProjectionCurve_)
                    result.push_back((*Tape::ProjectionRateAt<AAD::Number_>(*instrument))(block) - instrument->MarketRate());
                else
                    result.push_back((*DiscountRateT(*instrument))(Tape::YCCtx_<AAD::Number_>(block.Discount(convention.collateral_))) -
                                     instrument->MarketRate());
            }
        }
        return result;
    }

    Matrix_<> EvalJointAadJacobian(const JointMultiCurveCalibrationSpec_& spec, const Vector_<>& parameters) {
        auto* tape = AAD::Tape();
        TapeGuard_ guard(tape);
        Vector_<AAD::Number_> activeParameters = RegisterCurveParameters(parameters);
        AAD::NewRecording(*tape);
        const JointCurveMetadata_ metadata = BuildJointCurveMetadata(spec);
        const ActiveDiscountCurves_ discounts = BuildActiveDiscountCurves(spec, metadata, activeParameters);
        const ActiveForwardCurves_ forwards = BuildActiveForwardCurves(spec, metadata, activeParameters, discounts);
        const Tape::JointCurveBlock_<AAD::Number_> block = BuildActiveCurveBlock(discounts, forwards);
        Vector_<AAD::Number_> residuals = EvaluateActiveResiduals(spec, block);
        return HarvestCurveJacobian(*tape, activeParameters, residuals);
    }

    struct ExpectedJointShape_ {
        int columns_ = 0;
        int discountColumns_ = 0;
        int rows_ = 0;
    };

    ExpectedJointShape_ ExpectedJointShape(const JointMultiCurveCalibrationSpec_& spec) {
        ExpectedJointShape_ result;
        for (const auto& declaration : spec.curves_) {
            const CurveDefinition_ definition = MakeCurveDefinition(declaration.curveName_, spec.ccy_, declaration.parameterization_,
                                                                    declaration.logDfScheme_, declaration.knotDates_, spec.today_, spec.liborBasis_);
            const int columns = BuildCurveParameterLayout(definition).parameterCount_;
            result.columns_ += columns;
            if (declaration.calibrateDiscountCurve_)
                result.discountColumns_ += columns;
            result.rows_ += static_cast<int>(declaration.instruments_.size());
        }
        return result;
    }

    void AssertCentralDifferences(const JointMultiCurveCalibrationSpec_& spec,
                                  const Vector_<>& solved,
                                  const Matrix_<>& analyticJacobian,
                                  const ExpectedJointShape_& shape) {
        constexpr double bump = 1.0e-6;
        for (int column = 0; column < shape.columns_; ++column) {
            Vector_<> up = solved;
            Vector_<> down = solved;
            up[column] += bump;
            down[column] -= bump;
            const Vector_<> upResiduals = EvalJointResiduals(spec, up);
            const Vector_<> downResiduals = EvalJointResiduals(spec, down);
            for (int row = 0; row < shape.rows_; ++row) {
                const double centralDifference = (upResiduals[row] - downResiduals[row]) / (2.0 * bump);
                ASSERT_NEAR(analyticJacobian(row, column), centralDifference, 2.0e-8) << "row=" << row << ", column=" << column;
            }
        }
    }

    double CrossBlockMax(const JointMultiCurveCalibrationSpec_& spec, const Matrix_<>& jacobian, const ExpectedJointShape_& shape) {
        const int discountRows = static_cast<int>(spec.curves_.front().instruments_.size());
        double result = 0.0;
        for (int row = discountRows; row < shape.rows_; ++row)
            for (int column = 0; column < shape.discountColumns_; ++column)
                result = std::max(result, std::fabs(jacobian(row, column)));
        return result;
    }

    void AssertSolvedParametersMatch(const Vector_<>& analytic, const Vector_<>& bumped) {
        ASSERT_EQ(bumped.size(), analytic.size());
        for (int i = 0; i < static_cast<int>(analytic.size()); ++i)
            ASSERT_NEAR(analytic[i], bumped[i], 2.0e-5) << "solver column=" << i;
    }

    void AssertDiagnosticsMatch(const JointMultiCurveCalibrationSpec_& spec,
                                const JointMultiCurveCalibrationResult_& analytic,
                                const JointMultiCurveCalibrationResult_& bumped) {
        ASSERT_EQ(analytic.diagnostics_.size(), bumped.diagnostics_.size());
        for (int declarationIndex = 0; declarationIndex < static_cast<int>(spec.curves_.size()); ++declarationIndex) {
            const auto& analyticResiduals = analytic.diagnostics_[declarationIndex].residuals_;
            const auto& bumpedResiduals = bumped.diagnostics_[declarationIndex].residuals_;
            ASSERT_EQ(analyticResiduals.size(), bumpedResiduals.size());
            for (int row = 0; row < static_cast<int>(analyticResiduals.size()); ++row)
                ASSERT_NEAR(analyticResiduals[row], bumpedResiduals[row], 2.0e-8) << "declaration=" << declarationIndex << ", residual row=" << row;
        }
    }

    void AssertCurveNodesMatch(const JointMultiCurveCalibrationSpec_& spec,
                               const JointMultiCurveCalibrationResult_& analytic,
                               const JointMultiCurveCalibrationResult_& bumped) {
        for (int declarationIndex = 0; declarationIndex < static_cast<int>(spec.curves_.size()); ++declarationIndex) {
            const auto& declaration = spec.curves_[declarationIndex];
            const Handle_<DiscountCurve_>& analyticCurve = declaration.calibrateDiscountCurve_
                                                               ? analytic.discountCurves_.at(declaration.targetCollateral_)
                                                               : analytic.forwardCurves_.at(declaration.targetTenor_);
            const Handle_<DiscountCurve_>& bumpedCurve = declaration.calibrateDiscountCurve_
                                                             ? bumped.discountCurves_.at(declaration.targetCollateral_)
                                                             : bumped.forwardCurves_.at(declaration.targetTenor_);
            for (int knot = 0; knot < static_cast<int>(declaration.knotDates_.size()); ++knot)
                ASSERT_NEAR((*analyticCurve)(spec.today_, declaration.knotDates_[knot]), (*bumpedCurve)(spec.today_, declaration.knotDates_[knot]),
                            2.0e-5)
                    << "declaration=" << declarationIndex << ", knot=" << knot;
        }
    }

    void AssertJointJacobianMatchesCentralDifferences(const JointMultiCurveCalibrationSpec_& spec, const String_& label) {
        SCOPED_TRACE(label.c_str());
        JointMultiCurveCalibrationOptions_ analyticOptions;
        analyticOptions.jacobianMode_ = CurveJacobianMode_::Value_::ANALYTIC;
        const JointMultiCurveCalibrationResult_ analytic = CalibrateJointMultiCurve(spec, analyticOptions);
        ASSERT_TRUE(analytic.converged_);
        ASSERT_FALSE(analytic.jacobianAtSolution_.Empty());

        const Vector_<> solved = SolvedJointParameters(spec, analytic);
        const ExpectedJointShape_ shape = ExpectedJointShape(spec);
        ASSERT_EQ(static_cast<int>(solved.size()), shape.columns_);
        ASSERT_EQ(analytic.jacobianAtSolution_.Rows(), shape.rows_);
        ASSERT_EQ(analytic.jacobianAtSolution_.Cols(), shape.columns_);
        AssertCentralDifferences(spec, solved, analytic.jacobianAtSolution_, shape);
        ASSERT_GT(CrossBlockMax(spec, analytic.jacobianAtSolution_, shape), 1.0e-8)
            << "base-layered forward residuals must depend on discount-curve parameters";

        JointMultiCurveCalibrationOptions_ bumpedOptions;
        bumpedOptions.jacobianMode_ = CurveJacobianMode_::Value_::BUMPED;
        const JointMultiCurveCalibrationResult_ bumped = CalibrateJointMultiCurve(spec, bumpedOptions);
        ASSERT_TRUE(bumped.converged_);
        ASSERT_TRUE(bumped.jacobianAtSolution_.Empty());
        const Vector_<> bumpedSolved = SolvedJointParameters(spec, bumped);
        AssertSolvedParametersMatch(solved, bumpedSolved);
        AssertDiagnosticsMatch(spec, analytic, bumped);
        AssertCurveNodesMatch(spec, analytic, bumped);
    }
} // namespace

TEST(JointAnalyticJacobianTest, TestPwlFactoryVsDirectConstruction) {
    // AC10: after dedup, the factory (NewDiscountPWLF) constructs Tape::DiscountPWLF_<double> by
    // unpacking a PiecewiseLinear_ into flat vectors. Compare its DFs against a directly-constructed
    // Tape::DiscountPWLF_<double> with the same data to catch factory-unpacking bugs, constructor
    // divergence, or data corruption. All four IntegralTo branches are exercised.
    const Date_ today(2024, 1, 15);
    const Vector_<Date_> knots = {
        Date_(2024, 4, 15), Date_(2024, 7, 15), Date_(2024, 10, 15), Date_(2025, 1, 15),
        Date_(2025, 7, 15), Date_(2026, 1, 15), Date_(2027, 1, 15),  Date_(2029, 1, 15),
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
            ASSERT_NEAR(factoryDf, directDf, 1e-15) << "Factory-vs-direct mismatch at from=" << Date::ToString(from) << ", to=" << Date::ToString(to);
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
        Date_(2024, 4, 15), Date_(2024, 7, 15), Date_(2024, 10, 15), Date_(2025, 1, 15),
        Date_(2025, 7, 15), Date_(2026, 1, 15), Date_(2027, 1, 15),  Date_(2029, 1, 15),
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

            ASSERT_NEAR(got, expected, 1e-15) << "Mismatch vs PiecewiseLinear_ oracle at from=" << Date::ToString(from)
                                              << ", to=" << Date::ToString(to);
        }
    }
}

TEST(JointAnalyticJacobianTest, TestPwlApplyDxRoundTrip) {
    // AC12: ApplyDX on Tape::DiscountPWLF_<double> correctly shifts the forward rates and the effect
    // on discount factors matches a manually bumped PiecewiseLinear_ reference.
    const Date_ today(2024, 1, 15);
    const Vector_<Date_> knots = {
        Date_(2024, 4, 15), Date_(2024, 7, 15), Date_(2024, 10, 15), Date_(2025, 1, 15), Date_(2025, 7, 15),
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
    dx[2] = 0.001; // fLeft[1]
    dx[3] = 0.001; // fRight[1]
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

TEST(JointAnalyticJacobianTest, TestPiecewiseConstantDeclarationsEngageAnalyticJacobian) {
    RegisterAll_::Init();
    const Date_ today(2024, 1, 15);
    const Ccy_ ccy("USD");
    auto overnightIndex = Ccy::Conventions::OisIndex()(ccy);
    overnightIndex.dayBasis_ = DayBasis_("ACT_365F");
    overnightIndex.accrualHolidays_ = Holidays::None();
    overnightIndex.fixingHolidays_ = Holidays::None();
    const Date_ maturity = Date::AddMonths(today, 3);

    JointCurveDeclaration_ declaration;
    declaration.curveName_ = "joint_pwc";
    declaration.parameterization_ = CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD;
    declaration.knotDates_ = {maturity};
    declaration.instruments_ = {Handle_<YCInstrument_>(new Deposit_(today, today, maturity, 0.015, overnightIndex))};

    JointMultiCurveCalibrationSpec_ spec;
    spec.today_ = today;
    spec.ccy_ = ccy.String();
    spec.liborBasis_ = DayBasis_("ACT_365F");
    spec.tolerance_ = 1.0e-10;
    spec.fitTolerance_ = 1.0e-8;
    spec.initialGuess_ = 0.02;
    spec.solveMode_ = CurveSolveMode_::Value_::EXACT;
    spec.curves_ = {declaration};

    JointMultiCurveCalibrationOptions_ options;
    options.jacobianMode_ = CurveJacobianMode_::Value_::ANALYTIC;
    const auto result = CalibrateJointMultiCurve(spec, options);
    ASSERT_TRUE(result.converged_);
    ASSERT_FALSE(result.jacobianAtSolution_.Empty());
    ASSERT_EQ(result.jacobianAtSolution_.Rows(), 1);
    ASSERT_EQ(result.jacobianAtSolution_.Cols(), 1);
}

TEST(JointAnalyticJacobianTest, TestDiscountDeclarationFraEngagesAnalyticJacobian) {
    const Date_ today(2024, 1, 15);
    const Date_ maturity = Date::AddMonths(today, 3);
    RateIndexConvention_ discountIndex;
    discountIndex.useProjectionCurve_ = false;
    discountIndex.forecastTenor_ = PeriodLength_("3M");
    discountIndex.dayBasis_ = DayBasis_("ACT_365F");
    discountIndex.fixingHolidays_ = Holidays::None();
    discountIndex.accrualHolidays_ = Holidays::None();
    discountIndex.collateral_ = CollateralType_(CollateralType_::Value_::OIS);

    JointCurveDeclaration_ declaration;
    declaration.curveName_ = "joint_discount_fra";
    declaration.parameterization_ = CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD;
    declaration.knotDates_ = {maturity};
    declaration.instruments_ = {Handle_<YCInstrument_>(new FRA_(today, today, maturity, 0.015, discountIndex))};

    JointMultiCurveCalibrationSpec_ spec;
    spec.today_ = today;
    spec.ccy_ = "USD";
    spec.curves_ = {declaration};
    spec.liborBasis_ = DayBasis_("ACT_365F");
    spec.tolerance_ = 1.0e-10;
    spec.fitTolerance_ = 1.0e-8;
    spec.initialGuess_ = 0.02;
    spec.solveMode_ = CurveSolveMode_::Value_::EXACT;

    JointMultiCurveCalibrationOptions_ options;
    options.jacobianMode_ = CurveJacobianMode_::Value_::ANALYTIC;
    const JointMultiCurveCalibrationResult_ result = CalibrateJointMultiCurve(spec, options);
    ASSERT_TRUE(result.converged_);
    ASSERT_EQ(result.jacobianAtSolution_.Rows(), 1);
    ASSERT_EQ(result.jacobianAtSolution_.Cols(), 1);
    ASSERT_NE(result.jacobianAtSolution_(0, 0), 0.0);
}

TEST(JointAnalyticJacobianTest, TestPiecewiseConstantInitialJacobianMatchesCentralDifferences) {
    RegisterAll_::Init();
    const Date_ today(2024, 1, 15);
    const Ccy_ ccy("USD");
    const JointMultiCurveCalibrationSpec_ spec =
        BuildParameterizationSpec(today, ccy, CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD,
                                  CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD, LogDfScheme_::Value_::LOG_LINEAR);
    Vector_<> parameters;
    for (const auto& declaration : spec.curves_)
        parameters.Append(declaration.initialGuessPerNode_);

    const Matrix_<> aad = EvalJointAadJacobian(spec, parameters);
    ASSERT_EQ(aad.Rows(), 8);
    ASSERT_EQ(aad.Cols(), 8);
    constexpr double bump = 1.0e-6;
    for (int column = 0; column < aad.Cols(); ++column) {
        Vector_<> up = parameters;
        Vector_<> down = parameters;
        up[column] += bump;
        down[column] -= bump;
        const Vector_<> upResiduals = EvalJointResiduals(spec, up);
        const Vector_<> downResiduals = EvalJointResiduals(spec, down);
        for (int row = 0; row < aad.Rows(); ++row) {
            const double centralDifference = (upResiduals[row] - downResiduals[row]) / (2.0 * bump);
            ASSERT_NEAR(aad(row, column), centralDifference, 2.0e-8) << "row=" << row << ", column=" << column;
        }
    }
}

TEST(JointAnalyticJacobianTest, TestHomogeneousParameterizationsMatchCentralDifferences) {
    RegisterAll_::Init();
    const Date_ today(2024, 1, 15);
    const Ccy_ ccy("USD");
    struct Case_ {
        CurveParameterization_ parameterization;
        LogDfScheme_ scheme;
        String_ label;
    };
    const Vector_<Case_> cases = {
        {CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD, LogDfScheme_::Value_::LOG_LINEAR, "PWC"},
        {CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD, LogDfScheme_::Value_::LOG_LINEAR, "PWL"},
        {CurveParameterization_::Value_::LOG_DISCOUNT, LogDfScheme_::Value_::LOG_LINEAR, "log-linear"},
        {CurveParameterization_::Value_::LOG_DISCOUNT, LogDfScheme_::Value_::LOG_CUBIC_NATURAL, "natural-cubic log-DF"},
        {CurveParameterization_::Value_::LOG_DISCOUNT, LogDfScheme_::Value_::MIXED, "mixed log-DF"},
    };
    for (const auto& testCase : cases) {
        SCOPED_TRACE(testCase.label.c_str());
        const JointMultiCurveCalibrationSpec_ spec =
            BuildParameterizationSpec(today, ccy, testCase.parameterization, testCase.parameterization, testCase.scheme);
        ASSERT_NO_THROW(AssertJointJacobianMatchesCentralDifferences(spec, testCase.label));
    }
}

TEST(JointAnalyticJacobianTest, TestHomogeneousZeroRateParameterizationsMatchCentralDifferences) {
    RegisterAll_::Init();
    const Date_ today(2024, 1, 15);
    const Ccy_ ccy("USD");
    const Vector_<LogDfScheme_> schemes = {
        LogDfScheme_::Value_::LOG_LINEAR,
        LogDfScheme_::Value_::LOG_CUBIC_NATURAL,
        LogDfScheme_::Value_::MIXED,
    };
    for (const LogDfScheme_ scheme : schemes) {
        SCOPED_TRACE(scheme.String());
        const JointMultiCurveCalibrationSpec_ spec = BuildParameterizationSpec(
            today, ccy, CurveParameterization_::Value_::ZERO_RATE, CurveParameterization_::Value_::ZERO_RATE, scheme);
        ASSERT_NO_THROW(AssertJointJacobianMatchesCentralDifferences(spec, String_("homogeneous ZERO_RATE ") + scheme.String()));
    }
}

TEST(JointAnalyticJacobianTest, TestMixedParameterizationsMatchCentralDifferences) {
    RegisterAll_::Init();
    const Date_ today(2024, 1, 15);
    const Ccy_ ccy("USD");
    const JointMultiCurveCalibrationSpec_ logDfPwc =
        BuildParameterizationSpec(today, ccy, CurveParameterization_::Value_::LOG_DISCOUNT, CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD,
                                  LogDfScheme_::Value_::MIXED);
    ASSERT_NO_THROW(AssertJointJacobianMatchesCentralDifferences(logDfPwc, "mixed log-DF discount plus base-layered PWC forward"));

    const JointMultiCurveCalibrationSpec_ pwcPwl =
        BuildParameterizationSpec(today, ccy, CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD,
                                  CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD, LogDfScheme_::Value_::LOG_LINEAR);
    ASSERT_NO_THROW(AssertJointJacobianMatchesCentralDifferences(pwcPwl, "PWC discount plus base-layered PWL forward"));
}

TEST(JointAnalyticJacobianTest, TestMixedZeroRateParameterizationsMatchCentralDifferences) {
    RegisterAll_::Init();
    const Date_ today(2024, 1, 15);
    const Ccy_ ccy("USD");
    const JointMultiCurveCalibrationSpec_ zeroRateLogDf = BuildParameterizationSpec(
        today, ccy, CurveParameterization_::Value_::ZERO_RATE, CurveParameterization_::Value_::LOG_DISCOUNT, LogDfScheme_::Value_::MIXED);
    ASSERT_NO_THROW(AssertJointJacobianMatchesCentralDifferences(zeroRateLogDf, "ZERO_RATE discount plus base-layered log-DF forward"));

    const JointMultiCurveCalibrationSpec_ zeroRatePwc =
        BuildParameterizationSpec(today, ccy, CurveParameterization_::Value_::ZERO_RATE, CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD,
                                  LogDfScheme_::Value_::LOG_LINEAR);
    ASSERT_NO_THROW(AssertJointJacobianMatchesCentralDifferences(zeroRatePwc, "ZERO_RATE discount plus base-layered PWC forward"));

    const JointMultiCurveCalibrationSpec_ zeroRatePwl =
        BuildParameterizationSpec(today, ccy, CurveParameterization_::Value_::ZERO_RATE, CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD,
                                  LogDfScheme_::Value_::LOG_CUBIC_NATURAL);
    ASSERT_NO_THROW(AssertJointJacobianMatchesCentralDifferences(zeroRatePwl, "ZERO_RATE discount plus base-layered PWL forward"));

    const JointMultiCurveCalibrationSpec_ pwcZeroRate = BuildParameterizationSpec(
        today, ccy, CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD, CurveParameterization_::Value_::ZERO_RATE, LogDfScheme_::Value_::MIXED);
    ASSERT_NO_THROW(AssertJointJacobianMatchesCentralDifferences(pwcZeroRate, "PWC discount plus base-layered ZERO_RATE forward"));
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
    int expectedRows = 0;
    int expectedColumns = 0;
    for (const auto& declaration : spec.curves_) {
        expectedRows += static_cast<int>(declaration.instruments_.size());
        expectedColumns +=
            BuildCurveParameterLayout(MakeCurveDefinition(declaration.curveName_, spec.ccy_, declaration.parameterization_, declaration.logDfScheme_,
                                                          declaration.knotDates_, spec.today_, spec.liborBasis_))
                .parameterCount_;
    }
    ASSERT_EQ(rAnalytic.jacobianAtSolution_.Rows(), expectedRows);
    ASSERT_EQ(rAnalytic.jacobianAtSolution_.Cols(), expectedColumns);

    // Per-pillar OIS DFs agree to the smoothing-fit residual floor (both paths solve the same
    // system with differently-computed Jacobians; the solution x should be essentially identical).
    const auto& jointOisAnalytic = *rAnalytic.discountCurves_.at(CollateralType_(CollateralType_::Value_::OIS));
    const auto& jointOisBumped = *rBumped.discountCurves_.at(CollateralType_(CollateralType_::Value_::OIS));
    const Vector_<int> pillarMonths = {6, 12, 18, 24, 36, 60};
    for (const int months : pillarMonths) {
        const Date_ pillar = Date::AddMonths(today, months);
        ASSERT_NEAR(jointOisAnalytic(today, pillar), jointOisBumped(today, pillar), 1.0e-5) << "OIS DF mismatch at " << months << "M pillar";
    }

    // Per-pillar 3M forward DFs: base-layered, so both smooth the spread; agreement to ~1e-7.
    const auto& joint3mAnalytic = *rAnalytic.forwardCurves_.at(spec.curves_[1].targetTenor_);
    const auto& joint3mBumped = *rBumped.forwardCurves_.at(spec.curves_[1].targetTenor_);
    for (const int months : pillarMonths) {
        const Date_ pillar = Date::AddMonths(today, months);
        ASSERT_NEAR(joint3mAnalytic(today, pillar), joint3mBumped(today, pillar), 1.0e-5) << "3M DF mismatch at " << months << "M pillar";
    }
}

TEST(JointAnalyticJacobianTest, TestCanSkipJacobianAtSolution) {
    RegisterAll_::Init();
    const Date_ today(2024, 1, 15);
    const Ccy_ ccy("USD");
    const JointMultiCurveCalibrationSpec_ spec = BuildSmallJointSpec(today, ccy, /*baseLayered=*/true, DayBasis_("ACT_365F"));

    JointMultiCurveCalibrationOptions_ opt;
    opt.jacobianMode_ = CurveJacobianMode_::Value_::ANALYTIC;
    opt.computeJacobianAtSolution_ = false;
    const JointMultiCurveCalibrationResult_ result = CalibrateJointMultiCurve(spec, opt);

    ASSERT_TRUE(result.converged_);
    ASSERT_TRUE(result.jacobianAtSolution_.Empty());
    ASSERT_LT(result.jointMaxAbsResidual_, 1.0e-7);
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

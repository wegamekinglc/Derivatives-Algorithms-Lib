//
// Created by dal-implementer on 2026/7/28.
//

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <future>
#include <limits>
#include <stdexcept>
#include <thread>

#include <dal/curve/curveblock.hpp>
#include <dal/curve/curveparameterization.hpp>
#include <dal/curve/piecewiseconstant.hpp>
#include <dal/curve/piecewiselinear.hpp>
#include <dal/curve/ratecashflowpricing.hpp>
#include <dal/curve/ratecashflowpricing_internal.hpp>
#include <dal/curve/xccypricing.hpp>
#include <dal/curve/ycconst.hpp>
#include <dal/curve/ycimp.hpp>
#include <dal/curve/yclogdf.hpp>
#include <dal/curve/yczerorate.hpp>
#include <dal/math/aad/aad.hpp>

namespace {
    Dal::RateIndexConvention_ QuarterlyIndex() {
        Dal::RateIndexConvention_ result;
        result.forecastTenor_ = Dal::PeriodLength_("3M");
        result.dayBasis_ = Dal::DayBasis_("ACT_365F");
        result.collateral_ = Dal::CollateralType_(Dal::CollateralType_::Value_::OIS);
        return result;
    }

    Dal::RateLegConvention_ AnnualLeg() {
        Dal::RateLegConvention_ result;
        result.paymentFrequency_ = Dal::PeriodLength_("12M");
        result.dayBasis_ = Dal::DayBasis_("ACT_365F");
        return result;
    }

    Dal::Handle_<Dal::DiscountCurve_> FlatCurve(const Dal::Date_& maturity, double rate = 0.04, const Dal::String_& ccy = "USD") {
        return Dal::Handle_<Dal::DiscountCurve_>(Dal::NewDiscountPWC("flat", ccy, Dal::PiecewiseConstant_({maturity}, {rate})));
    }

    Dal::RatePricingMarket_ Market(const Dal::Date_& today, const Dal::Handle_<Dal::DiscountCurve_>& curve) {
        Dal::RatePricingMarket_ result;
        result.valuationTime_ = Dal::DateTime_(today, 10, 30);
        result.resultCurrency_ = Dal::Ccy_("USD");
        result.curveComponents_["discount"] = curve;
        result.curveComponents_["forecast"] = curve;
        result.curveComponents_["reference"] = curve;
        result.fixings_ = Dal::Handle_<Dal::MarketFixingSnapshot_>(new Dal::MarketFixingSnapshot_());
        return result;
    }

    Dal::RateTradeDefinition_ Trade(const Dal::RateInstrumentType_& type,
                                    const Dal::Date_& today,
                                    const Dal::Date_& start,
                                    const Dal::Date_& maturity,
                                    const Dal::RateTradeTerms_& terms) {
        return {"trade-1", type, today, start, maturity, Dal::Ccy_("USD"), terms};
    }

    Dal::DepositTradeTerms_ DepositTerms() {
        Dal::DepositTradeTerms_ result;
        result.notional_ = 100.0;
        result.contractRate_ = 0.05;
        result.lend_ = true;
        result.index_ = QuarterlyIndex();
        result.discountComponentKey_ = "discount";
        return result;
    }

    Dal::FraTradeTerms_ FraTerms(bool settleAtStart = true) {
        Dal::FraTradeTerms_ result;
        result.notional_ = 2'000'000.0;
        result.contractRate_ = 0.03;
        result.receiveFloating_ = true;
        result.settleAtStart_ = settleAtStart;
        result.index_ = QuarterlyIndex();
        result.fixingIdentity_ = {"USD-LIBOR-3M", 11, 0};
        result.forecastComponentKey_ = "forecast";
        result.discountComponentKey_ = "discount";
        return result;
    }

    Dal::FutureTradeTerms_ FutureTerms() {
        Dal::FutureTradeTerms_ result;
        result.contractCount_ = 12.0;
        result.long_ = true;
        result.referencePrice_ = 95.0;
        result.contractValuePerPricePoint_ = 25.0;
        result.convexityAdjustment_ = 0.0005;
        result.index_ = QuarterlyIndex();
        result.fixingIdentity_ = {"USD-LIBOR-3M", 11, 0};
        result.forecastComponentKey_ = "forecast";
        return result;
    }

    Dal::FixedFloatTradeTerms_ FixedFloatTerms(bool payFixed = true) {
        Dal::FixedFloatTradeTerms_ result;
        result.notional_ = 1'000'000.0;
        result.contractRate_ = 0.03;
        result.payFixed_ = payFixed;
        result.fixedLeg_ = AnnualLeg();
        result.floatLeg_ = AnnualLeg();
        result.floatIndex_ = QuarterlyIndex();
        result.fixingIdentity_ = {"USD-SOFR", 11, 0};
        result.forecastComponentKey_ = "forecast";
        result.discountComponentKey_ = "discount";
        return result;
    }

    Dal::BasisTradeTerms_ BasisTerms(bool receiveReference = true) {
        Dal::BasisTradeTerms_ result;
        result.notional_ = 1'000'000.0;
        result.contractSpread_ = 0.001;
        result.receiveReferencePaySpread_ = receiveReference;
        result.spreadLeg_ = AnnualLeg();
        result.referenceLeg_ = AnnualLeg();
        result.spreadIndex_ = QuarterlyIndex();
        result.referenceIndex_ = QuarterlyIndex();
        // Different fixing identities per leg: the spread leg fixes on LIBOR, the reference leg on SOFR.
        result.spreadFixingIdentity_ = {"USD-LIBOR-3M", 11, 0};
        result.referenceFixingIdentity_ = {"USD-SOFR", 11, 0};
        result.spreadForecastComponentKey_ = "forecast";
        result.referenceForecastComponentKey_ = "reference";
        result.discountComponentKey_ = "discount";
        return result;
    }

    // Shared wrapper of the fixed-float terms into the family's terms alternative.
    Dal::RateTradeTerms_ AsFamilyTerms(const Dal::RateInstrumentType_& family, const Dal::FixedFloatTradeTerms_& value) {
        REQUIRE(family == Dal::RateInstrumentType_("OIS") || family == Dal::RateInstrumentType_("IRS"),
                "AsFamilyTerms wraps fixed-float terms for OIS/IRS only");
        return family == Dal::RateInstrumentType_("OIS") ? Dal::RateTradeTerms_(Dal::OisTradeTerms_{value})
                                                         : Dal::RateTradeTerms_(Dal::IrsTradeTerms_{value});
    }

    Dal::RatePricingMarket_
    ComponentMarket(const Dal::Date_& today, const Dal::Handle_<Dal::DiscountCurve_>& forecast, const Dal::Handle_<Dal::DiscountCurve_>& discount) {
        Dal::RatePricingMarket_ result;
        result.valuationTime_ = Dal::DateTime_(today, 10, 30);
        result.resultCurrency_ = Dal::Ccy_("USD");
        result.curveComponents_["discount"] = discount;
        result.curveComponents_["forecast"] = forecast;
        result.curveComponents_["reference"] = discount;
        result.fixings_ = Dal::Handle_<Dal::MarketFixingSnapshot_>(new Dal::MarketFixingSnapshot_());
        return result;
    }

    Dal::RatePricingMarket_ ComponentMarket(const Dal::Date_& today,
                                            const Dal::Handle_<Dal::DiscountCurve_>& forecast,
                                            const Dal::Handle_<Dal::DiscountCurve_>& reference,
                                            const Dal::Handle_<Dal::DiscountCurve_>& discount) {
        Dal::RatePricingMarket_ result;
        result.valuationTime_ = Dal::DateTime_(today, 10, 30);
        result.resultCurrency_ = Dal::Ccy_("USD");
        result.curveComponents_["discount"] = discount;
        result.curveComponents_["forecast"] = forecast;
        result.curveComponents_["reference"] = reference;
        result.fixings_ = Dal::Handle_<Dal::MarketFixingSnapshot_>(new Dal::MarketFixingSnapshot_());
        return result;
    }

    class UnsupportedDiscountCurve_ : public Dal::DiscountCurve_ {
    public:
        explicit UnsupportedDiscountCurve_(const Dal::String_& ccy = "USD") : DiscountCurve_("unsupported", ccy) {}

        double operator()(const Dal::Date_&, const Dal::Date_&) const override { return 1.0; }
        void Poll(Dal::Vector_<const Dal::YCComponent_*>* all) const override { all->push_back(this); }
        void Poll(std::map<const Dal::YCComponent_*, Dal::Handle_<Dal::YCComponent_>>*) const override {}
        [[nodiscard]] std::unique_ptr<Dal::YCComponent_> Clone(const Dal::String_&, const Dal::YCComponent_::substitutions_t&) const override {
            return std::make_unique<UnsupportedDiscountCurve_>();
        }
        void Write(Dal::Archive::Store_&) const override {}
    };

    void AssertCanonicalFailure(const Dal::RateTradeNodeSensitivityResult_& result, const Dal::String_& reason) {
        ASSERT_FALSE(result.eligible_);
        ASSERT_DOUBLE_EQ(result.pv_, 0.0);
        ASSERT_TRUE(result.gradient_.empty());
        ASSERT_EQ(result.reason_, reason);
    }

    void
    AssertTradeValidationFailure(const Dal::RateTradeDefinition_& trade, const Dal::RatePricingMarket_& market, const std::string& errorFragment) {
        const auto priced = Dal::PriceRateTrade(trade, market);
        Dal::RateTradeNodeSensitivityResult_ sensitivity;
        ASSERT_NO_THROW(sensitivity = Dal::RateTradeNodeSensitivities(trade, market, "discount"));
        ASSERT_FALSE(priced.succeeded_);
        ASSERT_NE(std::string(priced.error_.c_str()).find(errorFragment), std::string::npos) << priced.error_;
        AssertCanonicalFailure(sensitivity, "TRADE_VALIDATION_FAILED");
    }

    template <class Builder_>
    void AssertRawNodeGradientMatchesCentralBumps(const Builder_& buildCurve,
                                                  const Dal::Vector_<>& parameters,
                                                  int expectedParameterCount,
                                                  double bump,
                                                  double absoluteTolerance,
                                                  double relativeTolerance,
                                                  const Dal::Vector_<int>& structuralZeroColumns) {
        const Dal::Date_ today(2026, 1, 15);
        const Dal::Date_ start(2026, 10, 15);
        const Dal::Date_ maturity(2029, 1, 15);
        const auto terms = DepositTerms();
        const auto trade = Trade(Dal::RateInstrumentType_("DEPOSIT"), today, start, maturity, terms);
        const auto market = Market(today, buildCurve(parameters));
        const auto aad = Dal::RateTradeNodeSensitivities(trade, market, "discount");

        ASSERT_TRUE(aad.eligible_);
        ASSERT_EQ(static_cast<int>(aad.gradient_.size()), expectedParameterCount);
        ASSERT_TRUE(std::isfinite(aad.pv_));
        for (int column = 0; column < expectedParameterCount; ++column) {
            auto plusParameters = parameters;
            auto minusParameters = parameters;
            plusParameters[column] += bump;
            minusParameters[column] -= bump;
            const auto plus = Dal::PriceRateTrade(trade, Market(today, buildCurve(plusParameters)));
            const auto minus = Dal::PriceRateTrade(trade, Market(today, buildCurve(minusParameters)));
            ASSERT_TRUE(plus.succeeded_);
            ASSERT_TRUE(minus.succeeded_);
            const double finiteDifference = (plus.pv_ - minus.pv_) / (2.0 * bump);
            const double tolerance = absoluteTolerance + relativeTolerance * std::max(std::abs(aad.gradient_[column]), std::abs(finiteDifference));
            ASSERT_NEAR(aad.gradient_[column], finiteDifference, tolerance) << "raw native-parameter column " << column;
        }

        auto borrowTerms = terms;
        borrowTerms.lend_ = false;
        const auto borrowTrade = Trade(Dal::RateInstrumentType_("DEPOSIT"), today, start, maturity, borrowTerms);
        const auto borrow = Dal::RateTradeNodeSensitivities(borrowTrade, market, "discount");
        ASSERT_TRUE(borrow.eligible_);
        ASSERT_DOUBLE_EQ(borrow.pv_, -aad.pv_);
        ASSERT_EQ(borrow.gradient_.size(), aad.gradient_.size());
        for (int column = 0; column < expectedParameterCount; ++column)
            ASSERT_DOUBLE_EQ(borrow.gradient_[column], -aad.gradient_[column]);

        const auto shortTrade = Trade(Dal::RateInstrumentType_("DEPOSIT"), today, today, start, terms);
        const auto structural = Dal::RateTradeNodeSensitivities(shortTrade, market, "discount");
        ASSERT_TRUE(structural.eligible_);
        ASSERT_EQ(static_cast<int>(structural.gradient_.size()), expectedParameterCount);
        for (const int column : structuralZeroColumns)
            ASSERT_NEAR(structural.gradient_[column], 0.0, 1.0e-12) << "structural-zero raw column " << column;
    }

    // Family-generic form of the Deposit battery above: the assembled market places the built
    // curve under the target component key and holds every other dependency on a fixed curve,
    // so central bumps perturb the target component only. Dates follow the Deposit battery;
    // the early window falls entirely before the first knot so later columns are structural zeros.
    template <class BuildCurve_, class AssembleMarket_>
    void AssertFamilyNodeGradientMatchesCentralBumps(const Dal::RateInstrumentType_& family,
                                                     const Dal::RateTradeTerms_& terms,
                                                     const Dal::RateTradeTerms_& oppositeTerms,
                                                     const Dal::String_& componentKey,
                                                     const AssembleMarket_& assembleMarket,
                                                     const BuildCurve_& buildCurve,
                                                     const Dal::Vector_<>& parameters,
                                                     int expectedParameterCount,
                                                     double bump,
                                                     double absoluteTolerance,
                                                     double relativeTolerance,
                                                     const Dal::Vector_<int>& structuralZeroColumns) {
        const Dal::Date_ today(2026, 1, 15);
        const Dal::Date_ start(2026, 10, 15);
        const Dal::Date_ maturity(2029, 1, 15);
        const Dal::Date_ earlyStart(2026, 2, 15);
        const Dal::Date_ earlyMaturity(2026, 4, 15);
        const auto trade = Trade(family, today, start, maturity, terms);
        const auto market = assembleMarket(buildCurve(parameters));
        const auto aad = Dal::RateTradeNodeSensitivities(trade, market, componentKey);
        const auto passive = Dal::PriceRateTrade(trade, market);

        ASSERT_TRUE(aad.eligible_);
        ASSERT_EQ(static_cast<int>(aad.gradient_.size()), expectedParameterCount);
        ASSERT_TRUE(passive.succeeded_);
        ASSERT_DOUBLE_EQ(aad.pv_, passive.pv_);
        for (int column = 0; column < expectedParameterCount; ++column) {
            auto plusParameters = parameters;
            auto minusParameters = parameters;
            plusParameters[column] += bump;
            minusParameters[column] -= bump;
            const auto plus = Dal::PriceRateTrade(trade, assembleMarket(buildCurve(plusParameters)));
            const auto minus = Dal::PriceRateTrade(trade, assembleMarket(buildCurve(minusParameters)));
            ASSERT_TRUE(plus.succeeded_);
            ASSERT_TRUE(minus.succeeded_);
            const double finiteDifference = (plus.pv_ - minus.pv_) / (2.0 * bump);
            const double tolerance = absoluteTolerance + relativeTolerance * std::max(std::abs(aad.gradient_[column]), std::abs(finiteDifference));
            ASSERT_NEAR(aad.gradient_[column], finiteDifference, tolerance) << "raw native-parameter column " << column;
        }

        const auto opposite = Trade(family, today, start, maturity, oppositeTerms);
        const auto flipped = Dal::RateTradeNodeSensitivities(opposite, market, componentKey);
        ASSERT_TRUE(flipped.eligible_);
        ASSERT_DOUBLE_EQ(flipped.pv_, -aad.pv_);
        ASSERT_EQ(flipped.gradient_.size(), aad.gradient_.size());
        for (int column = 0; column < expectedParameterCount; ++column)
            ASSERT_DOUBLE_EQ(flipped.gradient_[column], -aad.gradient_[column]);

        const auto early = Trade(family, today, earlyStart, earlyMaturity, terms);
        const auto structural = Dal::RateTradeNodeSensitivities(early, market, componentKey);
        ASSERT_TRUE(structural.eligible_);
        ASSERT_EQ(static_cast<int>(structural.gradient_.size()), expectedParameterCount);
        for (const int column : structuralZeroColumns)
            ASSERT_NEAR(structural.gradient_[column], 0.0, 1.0e-12) << "structural-zero raw column " << column;
    }

    // Family battery driver: one call runs the family gradient helper across all four curve
    // representations for one target component; the assembled market places the built curve under
    // the target component key and holds every other dependency on a distinct flat curve, so
    // central bumps perturb the target component only. The relative tolerance sits one order above
    // the single-period families: multi-period discounting and compounding add curvature that
    // scales the central-difference truncation. curveCcy names the currency of the built curve's
    // slot (XCCY foreign/basis slots are not USD).
    template <class MakeTerms_, class AssembleMarket_>
    void AssertFamilyNodeBatteries(const Dal::RateInstrumentType_& family,
                                   const MakeTerms_& makeTerms,
                                   const Dal::String_& componentKey,
                                   const AssembleMarket_& assembleMarket,
                                   const Dal::String_& curveCcy = "USD") {
        constexpr double NATIVE_PARAMETER_BUMP = 1.0e-6;
        constexpr double RAW_GRADIENT_ABSOLUTE_TOLERANCE = 1.0e-6;
        constexpr double RAW_GRADIENT_RELATIVE_TOLERANCE = 1.0e-7;
        const Dal::Date_ today(2026, 1, 15);
        const Dal::Date_ maturity(2029, 1, 15);
        const Dal::Vector_<Dal::Date_> knots{Dal::Date_(2026, 7, 15), Dal::Date_(2027, 1, 15), Dal::Date_(2028, 1, 15)};
        AssertFamilyNodeGradientMatchesCentralBumps(
            family, makeTerms(true), makeTerms(false), componentKey, assembleMarket,
            [&](const Dal::Vector_<>& parameters) {
                return Dal::Handle_<Dal::DiscountCurve_>(Dal::NewDiscountPWC("pwc", curveCcy, Dal::PiecewiseConstant_(knots, parameters)));
            },
            {0.01, -0.005, 0.03}, 3, NATIVE_PARAMETER_BUMP, RAW_GRADIENT_ABSOLUTE_TOLERANCE, RAW_GRADIENT_RELATIVE_TOLERANCE, {1, 2});
        AssertFamilyNodeGradientMatchesCentralBumps(
            family, makeTerms(true), makeTerms(false), componentKey, assembleMarket,
            [&](const Dal::Vector_<>& parameters) {
                Dal::Vector_<> left(3);
                Dal::Vector_<> right(3);
                for (int node = 0; node < 3; ++node) {
                    left[node] = parameters[2 * node];
                    right[node] = parameters[2 * node + 1];
                }
                return Dal::Handle_<Dal::DiscountCurve_>(Dal::NewDiscountPWLF("pwlf", curveCcy, Dal::PiecewiseLinear_(knots, left, right)));
            },
            {0.01, 0.0, -0.005, 0.02, 0.03, -0.01}, 6, NATIVE_PARAMETER_BUMP, RAW_GRADIENT_ABSOLUTE_TOLERANCE, RAW_GRADIENT_RELATIVE_TOLERANCE,
            {2, 3, 4, 5});
        AssertFamilyNodeGradientMatchesCentralBumps(family, makeTerms(true), makeTerms(false), componentKey, assembleMarket,
                                                    [&](const Dal::Vector_<>& parameters) {
                                                        Dal::Vector_<> stored{0.0};
                                                        stored.Append(parameters);
                                                        return Dal::Handle_<Dal::DiscountCurve_>(Dal::NewDiscountLogDF(
                                                            "logdf", curveCcy, Dal::Vector_<Dal::Date_>{today, knots[0], knots[1], knots[2]}, stored,
                                                            Dal::DayBasis_("ACT_365F"), Dal::LogDfScheme_::Value_::LOG_LINEAR));
                                                    },
                                                    {-0.005, 0.0, -0.06}, 3, NATIVE_PARAMETER_BUMP, RAW_GRADIENT_ABSOLUTE_TOLERANCE,
                                                    RAW_GRADIENT_RELATIVE_TOLERANCE, {2});
        AssertFamilyNodeGradientMatchesCentralBumps(
            family, makeTerms(true), makeTerms(false), componentKey, assembleMarket,
            [&](const Dal::Vector_<>& parameters) {
                return Dal::Handle_<Dal::DiscountCurve_>(Dal::NewDiscountZeroRate("zero", curveCcy, today, knots, parameters,
                                                                                  Dal::DayBasis_("ACT_365F"), Dal::LogDfScheme_::Value_::LOG_LINEAR));
            },
            {0.01, 0.0, -0.005}, 3, NATIVE_PARAMETER_BUMP, RAW_GRADIENT_ABSOLUTE_TOLERANCE, RAW_GRADIENT_RELATIVE_TOLERANCE, {2});
    }

    // OIS/IRS battery: the built curve lands under the target key and the other dependency stays
    // on a distinct flat curve (forecast != discount).
    void AssertFixedFloatNodeBatteries(const Dal::RateInstrumentType_& family, const Dal::String_& componentKey) {
        const Dal::Date_ today(2026, 1, 15);
        const Dal::Date_ maturity(2029, 1, 15);
        const auto assembleMarket = [&](const Dal::Handle_<Dal::DiscountCurve_>& curve) {
            return componentKey == "forecast" ? ComponentMarket(today, curve, FlatCurve(maturity, 0.03))
                                              : ComponentMarket(today, FlatCurve(maturity, 0.04), curve);
        };
        AssertFamilyNodeBatteries(
            family, [&](bool payFixed) { return AsFamilyTerms(family, FixedFloatTerms(payFixed)); }, componentKey, assembleMarket);
    }

    // Basis battery: the built curve lands under the target key and the other two dependencies
    // stay on distinct flat curves (spread forecast != reference forecast != discount).
    void AssertBasisNodeBatteries(const Dal::String_& componentKey) {
        const Dal::Date_ today(2026, 1, 15);
        const Dal::Date_ maturity(2029, 1, 15);
        const auto assembleMarket = [&](const Dal::Handle_<Dal::DiscountCurve_>& curve) {
            if (componentKey == "forecast")
                return ComponentMarket(today, curve, FlatCurve(maturity, 0.025), FlatCurve(maturity, 0.03));
            if (componentKey == "reference")
                return ComponentMarket(today, FlatCurve(maturity, 0.04), curve, FlatCurve(maturity, 0.03));
            return ComponentMarket(today, FlatCurve(maturity, 0.04), FlatCurve(maturity, 0.025), curve);
        };
        AssertFamilyNodeBatteries(
            Dal::RateInstrumentType_("BASIS_SWAP"), [](bool receiveReference) { return Dal::RateTradeTerms_(BasisTerms(receiveReference)); },
            componentKey, assembleMarket);
    }

    constexpr const char* XCCY_DOM_OIS = "domOis";
    constexpr const char* XCCY_DOM_FWD_3M = "domFwd3M";
    constexpr const char* XCCY_DOM_FWD_6M = "domFwd6M";
    constexpr const char* XCCY_FOR_OIS = "forOis";
    constexpr const char* XCCY_FOR_FWD_3M = "forFwd3M";
    constexpr const char* XCCY_BASIS = "basis";

    Dal::RateIndexConvention_ XccyIndex(bool projection) {
        auto result = QuarterlyIndex();
        if (projection) {
            result.useProjectionCurve_ = true;
            result.forecastTenor_ = Dal::PeriodLength_("3M");
        }
        return result;
    }

    Dal::XccyTradeTerms_ XccyTerms(const Dal::Ccy_& domestic,
                                   const Dal::Ccy_& foreign,
                                   bool receiveNonSpread,
                                   bool projection,
                                   const Dal::CollateralType_& domesticCollateral = Dal::CollateralType_(Dal::CollateralType_::Value_::OIS)) {
        Dal::XccyTradeTerms_ result;
        result.positionCount_ = 1.0;
        result.contractSpread_ = 0.001;
        result.spreadOnForeignLeg_ = true;
        result.receiveNonSpreadPaySpread_ = receiveNonSpread;
        result.config_.pair_ = Dal::CurrencyPair_(domestic, foreign);
        result.config_.domesticNotional_ = 1'000'000.0;
        result.config_.foreignNotional_ = 900'000.0;
        result.config_.convention_.domesticLeg_ = AnnualLeg();
        result.config_.convention_.foreignLeg_ = AnnualLeg();
        result.config_.convention_.domesticIndex_ = XccyIndex(projection);
        result.config_.convention_.domesticIndex_.collateral_ = domesticCollateral;
        result.config_.convention_.foreignIndex_ = XccyIndex(projection);
        result.config_.convention_.spreadOnForeignLeg_ = true;
        result.config_.domesticRateFixing_ = {Dal::String_(domestic.String()) + "-INDEX", 11, 0};
        result.config_.foreignRateFixing_ = {Dal::String_(foreign.String()) + "-INDEX", 11, 0};
        return result;
    }

    Dal::XccyTradeTerms_ XccyTerms(bool receiveNonSpread = true, bool projection = true) {
        return XccyTerms(Dal::Ccy_("USD"), Dal::Ccy_("EUR"), receiveNonSpread, projection);
    }

    // Curve inventory of a test XCCY market. Null handles leave the slot out of the block and the
    // key out of curveComponents_, so negative cases can shape addressing precisely.
    struct XccyMarketSpec_ {
        Dal::Handle_<Dal::DiscountCurve_> domesticOis_, domesticGc_, domesticFwd3M_, domesticFwd6M_;
        Dal::Handle_<Dal::DiscountCurve_> foreignOis_, foreignFwd3M_;
        Dal::Handle_<Dal::DiscountCurve_> basis_;
        double fxSpot_ = 1.2;
        Dal::Ccy_ domestic_ = Dal::Ccy_("USD");
        Dal::Ccy_ foreign_ = Dal::Ccy_("EUR");
        Dal::Handle_<Dal::MarketFixingSnapshot_> fixings_;
    };

    XccyMarketSpec_ FlatXccySpec(const Dal::Date_& horizon) {
        XccyMarketSpec_ spec;
        spec.domesticOis_ = FlatCurve(horizon, 0.03, "USD");
        spec.domesticFwd3M_ = FlatCurve(horizon, 0.032, "USD");
        spec.foreignOis_ = FlatCurve(horizon, 0.02, "EUR");
        spec.foreignFwd3M_ = FlatCurve(horizon, 0.022, "EUR");
        spec.basis_ = FlatCurve(horizon, 0.001, "USD");
        return spec;
    }

    // Pointer-identity contract: the block slots and the curveComponents_ entries share the same
    // Handles, so every consumed curve is addressable under a stable key.
    Dal::RatePricingMarket_ BuildXccyMarket(const Dal::Date_& today, const XccyMarketSpec_& spec) {
        std::map<Dal::CollateralType_, Dal::Handle_<Dal::DiscountCurve_>> domesticDiscounts;
        domesticDiscounts[Dal::CollateralType_(Dal::CollateralType_::Value_::OIS)] = spec.domesticOis_;
        if (spec.domesticGc_)
            domesticDiscounts[Dal::CollateralType_(Dal::CollateralType_::Value_::GC)] = spec.domesticGc_;
        std::map<Dal::PeriodLength_, Dal::Handle_<Dal::DiscountCurve_>> domesticForwards;
        if (spec.domesticFwd3M_)
            domesticForwards[Dal::PeriodLength_("3M")] = spec.domesticFwd3M_;
        if (spec.domesticFwd6M_)
            domesticForwards[Dal::PeriodLength_("6M")] = spec.domesticFwd6M_;
        std::map<Dal::CollateralType_, Dal::Handle_<Dal::DiscountCurve_>> foreignDiscounts;
        foreignDiscounts[Dal::CollateralType_(Dal::CollateralType_::Value_::OIS)] = spec.foreignOis_;
        std::map<Dal::PeriodLength_, Dal::Handle_<Dal::DiscountCurve_>> foreignForwards;
        if (spec.foreignFwd3M_)
            foreignForwards[Dal::PeriodLength_("3M")] = spec.foreignFwd3M_;

        const auto domesticBlock = Dal::Handle_<Dal::CurveBlock_>(
            new Dal::CurveBlock_("domestic", spec.domestic_.String(), domesticDiscounts, domesticForwards, Dal::DayBasis_("ACT_365F")));
        const auto foreignBlock = Dal::Handle_<Dal::CurveBlock_>(
            new Dal::CurveBlock_("foreign", spec.foreign_.String(), foreignDiscounts, foreignForwards, Dal::DayBasis_("ACT_365F")));
        const auto fixings = spec.fixings_ ? spec.fixings_ : Dal::Handle_<Dal::MarketFixingSnapshot_>(new Dal::MarketFixingSnapshot_());
        const auto native = std::make_shared<Dal::CrossCurrencyMarket_>(domesticBlock, foreignBlock, spec.fxSpot_, Dal::DateTime_(today, 10, 30),
                                                                        spec.domestic_, fixings);
        if (spec.basis_)
            native->SetBasisCurve(spec.basis_);

        Dal::RatePricingMarket_ result;
        result.valuationTime_ = Dal::DateTime_(today, 10, 30);
        result.resultCurrency_ = spec.domestic_;
        result.xccyMarket_ = native;
        result.fixings_ = fixings;
        const auto registerIf = [&](const char* key, const Dal::Handle_<Dal::DiscountCurve_>& curve) {
            if (curve)
                result.curveComponents_[key] = curve;
        };
        registerIf(XCCY_DOM_OIS, spec.domesticOis_);
        registerIf("domGc", spec.domesticGc_);
        registerIf(XCCY_DOM_FWD_3M, spec.domesticFwd3M_);
        registerIf(XCCY_DOM_FWD_6M, spec.domesticFwd6M_);
        registerIf(XCCY_FOR_OIS, spec.foreignOis_);
        registerIf(XCCY_FOR_FWD_3M, spec.foreignFwd3M_);
        registerIf(XCCY_BASIS, spec.basis_);
        return result;
    }

    Dal::RatePricingMarket_ FlatXccyMarket(const Dal::Date_& today) { return BuildXccyMarket(today, FlatXccySpec(Dal::Date_(2031, 1, 15))); }

    // XCCY battery: the built curve lands in the addressed block slot (registered under its key)
    // while every other consumed curve stays on a distinct flat curve, so central bumps perturb the
    // addressed component only. Foreign slots are EUR curves; the basis stays domestic (USD).
    void AssertXccyNodeBatteries(const Dal::String_& componentKey) {
        const Dal::Date_ today(2026, 1, 15);
        const auto assembleMarket = [&](const Dal::Handle_<Dal::DiscountCurve_>& curve) {
            auto spec = FlatXccySpec(Dal::Date_(2031, 1, 15));
            if (componentKey == XCCY_DOM_OIS)
                spec.domesticOis_ = curve;
            else if (componentKey == XCCY_DOM_FWD_3M)
                spec.domesticFwd3M_ = curve;
            else if (componentKey == XCCY_FOR_OIS)
                spec.foreignOis_ = curve;
            else if (componentKey == XCCY_FOR_FWD_3M)
                spec.foreignFwd3M_ = curve;
            else
                spec.basis_ = curve;
            return BuildXccyMarket(today, spec);
        };
        const Dal::String_ curveCcy = componentKey == XCCY_FOR_OIS || componentKey == XCCY_FOR_FWD_3M ? Dal::String_("EUR") : Dal::String_("USD");
        AssertFamilyNodeBatteries(
            Dal::RateInstrumentType_("XCCY"), [](bool receiveNonSpread) { return Dal::RateTradeTerms_(XccyTerms(receiveNonSpread)); }, componentKey,
            assembleMarket, curveCcy);
    }
} // namespace

TEST(RateCashflowPricingTest, TestRegistryAndDepositCashflows) {
    const auto families = Dal::RateInstrumentTypeListAll();
    ASSERT_EQ(families.size(), 7);
    ASSERT_STREQ(families[0].String(), "DEPOSIT");
    ASSERT_STREQ(families[1].String(), "FRA");
    ASSERT_STREQ(families[2].String(), "FUTURE");
    ASSERT_STREQ(families[3].String(), "OIS");
    ASSERT_STREQ(families[4].String(), "IRS");
    ASSERT_STREQ(families[5].String(), "BASIS_SWAP");
    ASSERT_STREQ(families[6].String(), "XCCY");

    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ maturity(2027, 1, 15);
    const auto curve = FlatCurve(maturity);
    const auto market = Market(today, curve);
    Dal::DepositTradeTerms_ terms;
    terms.notional_ = 100.0;
    terms.contractRate_ = 0.05;
    terms.lend_ = true;
    terms.index_ = QuarterlyIndex();
    terms.discountComponentKey_ = "discount";
    const auto definition = Trade(Dal::RateInstrumentType_("DEPOSIT"), today, today, maturity, terms);

    const auto result = Dal::PriceRateTrade(definition, market);
    const double accrual = terms.index_.dayBasis_(today, maturity, nullptr);
    const double expected = -100.0 + 100.0 * (1.0 + 0.05 * accrual) * (*curve)(today, maturity);

    ASSERT_TRUE(result.succeeded_);
    ASSERT_NEAR(result.pv_, expected, 1.0e-10);
    ASSERT_TRUE(result.requiredHistoricalFixings_.empty());
    ASSERT_EQ(result.dependencyComponentKeys_, Dal::Vector_<Dal::String_>({"discount"}));

    terms.lend_ = false;
    const auto borrow = Trade(Dal::RateInstrumentType_("DEPOSIT"), today, today, maturity, terms);
    ASSERT_NEAR(Dal::PriceRateTrade(borrow, market).pv_, -expected, 1.0e-10);
}

TEST(RateCashflowPricingTest, TestDepositNodeAADMatchesCentralNativeParameterBump) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ maturity(2027, 1, 15);
    const double rate = 0.04;
    const auto market = Market(today, FlatCurve(maturity, rate));
    Dal::DepositTradeTerms_ terms;
    terms.notional_ = 100.0;
    terms.contractRate_ = 0.05;
    terms.lend_ = true;
    terms.index_ = QuarterlyIndex();
    terms.discountComponentKey_ = "discount";
    const auto trade = Trade(Dal::RateInstrumentType_("DEPOSIT"), today, today, maturity, terms);

    const auto aad = Dal::RateTradeNodeSensitivities(trade, market, "discount");
    const double epsilon = 1.0e-6;
    const double plus = Dal::PriceRateTrade(trade, Market(today, FlatCurve(maturity, rate + epsilon))).pv_;
    const double minus = Dal::PriceRateTrade(trade, Market(today, FlatCurve(maturity, rate - epsilon))).pv_;

    ASSERT_TRUE(aad.eligible_);
    ASSERT_EQ(aad.gradient_.size(), 1);
    ASSERT_NEAR(aad.pv_, Dal::PriceRateTrade(trade, market).pv_, 1.0e-12);
    ASSERT_NEAR(aad.gradient_[0], (plus - minus) / (2.0 * epsilon), 1.0e-6);
}

TEST(RateCashflowPricingTest, TestDepositNodeAADRejectsInvalidTradeCanonically) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ maturity(2027, 1, 15);
    const auto market = Market(today, FlatCurve(maturity));
    Dal::DepositTradeTerms_ terms;
    terms.notional_ = std::numeric_limits<double>::quiet_NaN();
    terms.contractRate_ = 0.05;
    terms.lend_ = true;
    terms.index_ = QuarterlyIndex();
    terms.discountComponentKey_ = "discount";
    const auto trade = Trade(Dal::RateInstrumentType_("DEPOSIT"), today, today, maturity, terms);

    const auto priced = Dal::PriceRateTrade(trade, market);
    Dal::RateTradeNodeSensitivityResult_ sensitivity;
    ASSERT_NO_THROW(sensitivity = Dal::RateTradeNodeSensitivities(trade, market, "discount"));

    ASSERT_FALSE(priced.succeeded_);
    ASSERT_NE(priced.error_.find("Deposit notional must be positive and finite"), Dal::String_::npos);
    AssertCanonicalFailure(sensitivity, "TRADE_VALIDATION_FAILED");
}

TEST(RateCashflowPricingTest, TestNodeSensitivityFinalizerPublishesOnlyCompleteFiniteCandidates) {
    using Dal::RateCashflowPricingInternal::FinalizeNodeSensitivityCandidate;
    using Dal::RateCashflowPricingInternal::NodeSensitivityCandidate_;

    const auto success = FinalizeNodeSensitivityCandidate(NodeSensitivityCandidate_{1.25, {2.0, -3.0}}, 2);
    ASSERT_TRUE(success.eligible_);
    ASSERT_DOUBLE_EQ(success.pv_, 1.25);
    ASSERT_EQ(success.gradient_, Dal::Vector_<>({2.0, -3.0}));
    ASSERT_TRUE(success.reason_.empty());

    AssertCanonicalFailure(FinalizeNodeSensitivityCandidate(NodeSensitivityCandidate_{std::numeric_limits<double>::quiet_NaN(), {2.0, -3.0}}, 2),
                           "AAD_EVALUATION_FAILED");
    AssertCanonicalFailure(FinalizeNodeSensitivityCandidate(NodeSensitivityCandidate_{std::numeric_limits<double>::infinity(), {2.0, -3.0}}, 2),
                           "AAD_EVALUATION_FAILED");
    AssertCanonicalFailure(FinalizeNodeSensitivityCandidate(NodeSensitivityCandidate_{-std::numeric_limits<double>::infinity(), {2.0, -3.0}}, 2),
                           "AAD_EVALUATION_FAILED");
    AssertCanonicalFailure(FinalizeNodeSensitivityCandidate(NodeSensitivityCandidate_{1.25, {2.0}}, 2), "AAD_EVALUATION_FAILED");
    AssertCanonicalFailure(FinalizeNodeSensitivityCandidate(NodeSensitivityCandidate_{1.25, {2.0, -3.0, 4.0}}, 2), "AAD_EVALUATION_FAILED");

    for (const double invalid :
         {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()}) {
        AssertCanonicalFailure(FinalizeNodeSensitivityCandidate(NodeSensitivityCandidate_{1.25, {invalid, -3.0}}, 2), "AAD_EVALUATION_FAILED");
        AssertCanonicalFailure(FinalizeNodeSensitivityCandidate(NodeSensitivityCandidate_{1.25, {2.0, invalid}}, 2), "AAD_EVALUATION_FAILED");
    }
}

TEST(RateCashflowPricingTest, TestNodeSensitivityAADStageCanonicalizesExceptionAndRewindsTape) {
    const auto failed = Dal::RateCashflowPricingInternal::RunNodeSensitivityAADStage(1, []() {
        auto parameters = Dal::RegisterCurveParameters(Dal::Vector_<>{0.04});
        Dal::AAD::NewRecording(*Dal::AAD::Tape());
        Dal::AAD::Number_ node = parameters[0] * parameters[0];
        Dal::AAD::Adjoint(node) = 1.0;
        throw std::runtime_error("injected after recording");
        return Dal::RateCashflowPricingInternal::NodeSensitivityCandidate_{};
    });
    AssertCanonicalFailure(failed, "AAD_EVALUATION_FAILED");

    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ maturity(2027, 1, 15);
    Dal::DepositTradeTerms_ terms;
    terms.notional_ = 100.0;
    terms.contractRate_ = 0.05;
    terms.lend_ = true;
    terms.index_ = QuarterlyIndex();
    terms.discountComponentKey_ = "discount";
    const auto trade = Trade(Dal::RateInstrumentType_("DEPOSIT"), today, today, maturity, terms);
    const auto valid = Dal::RateTradeNodeSensitivities(trade, Market(today, FlatCurve(maturity)), "discount");

    ASSERT_TRUE(valid.eligible_);
    ASSERT_EQ(valid.gradient_.size(), 1);
    ASSERT_TRUE(std::isfinite(valid.pv_));
    ASSERT_TRUE(std::isfinite(valid.gradient_[0]));
}

TEST(RateCashflowPricingTest, TestNodeSensitivityCurveClassifierReturnsOnlyRepresentationTagAndBorrowedPointer) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Vector_<Dal::Date_> knots{Dal::Date_(2026, 7, 15), Dal::Date_(2027, 1, 15), Dal::Date_(2028, 1, 15)};
    const Dal::Vector_<> rates{0.01, 0.02, 0.03};
    const Dal::Vector_<> logDf{0.0, -0.005, -0.02, -0.06};
    const Dal::DayBasis_ dayCount("ACT_365F");
    const std::unique_ptr<Dal::DiscountCurve_> pwc(Dal::NewDiscountPWC("pwc", "USD", Dal::PiecewiseConstant_(knots, rates)));
    const std::unique_ptr<Dal::DiscountCurve_> pwlf(Dal::NewDiscountPWLF("pwlf", "USD", Dal::PiecewiseLinear_(knots, rates, rates)));
    const std::unique_ptr<Dal::DiscountCurve_> logDiscount(Dal::NewDiscountLogDF(
        "logdf", "USD", Dal::Vector_<Dal::Date_>{today, knots[0], knots[1], knots[2]}, logDf, dayCount, Dal::LogDfScheme_::Value_::LOG_LINEAR));
    const std::unique_ptr<Dal::DiscountCurve_> zeroRate(
        Dal::NewDiscountZeroRate("zero", "USD", today, knots, rates, dayCount, Dal::LogDfScheme_::Value_::LOG_LINEAR));
    const UnsupportedDiscountCurve_ unsupported;

    const auto classifiedPwc = Dal::RateCashflowPricingInternal::ClassifyNodeSensitivityCurve(*pwc);
    const auto classifiedPwlf = Dal::RateCashflowPricingInternal::ClassifyNodeSensitivityCurve(*pwlf);
    const auto classifiedLogDiscount = Dal::RateCashflowPricingInternal::ClassifyNodeSensitivityCurve(*logDiscount);
    const auto classifiedZeroRate = Dal::RateCashflowPricingInternal::ClassifyNodeSensitivityCurve(*zeroRate);
    ASSERT_EQ(classifiedPwc.index(), 1);
    ASSERT_EQ(std::get<1>(classifiedPwc), pwc.get());
    ASSERT_EQ(classifiedPwlf.index(), 2);
    ASSERT_EQ(std::get<2>(classifiedPwlf), pwlf.get());
    ASSERT_EQ(classifiedLogDiscount.index(), 3);
    ASSERT_EQ(std::get<3>(classifiedLogDiscount), logDiscount.get());
    ASSERT_EQ(classifiedZeroRate.index(), 4);
    ASSERT_EQ(std::get<4>(classifiedZeroRate), zeroRate.get());
    ASSERT_EQ(Dal::RateCashflowPricingInternal::ClassifyNodeSensitivityCurve(unsupported).index(), 0);
}

TEST(RateCashflowPricingTest, TestDepositNodeAADPassiveValidationBoundaryMatrix) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ maturity(2027, 1, 15);
    const auto market = Market(today, FlatCurve(maturity));

    for (const double invalid :
         {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(), 0.0, -1.0}) {
        auto terms = DepositTerms();
        terms.notional_ = invalid;
        AssertTradeValidationFailure(Trade(Dal::RateInstrumentType_("DEPOSIT"), today, today, maturity, terms), market,
                                     "Deposit notional must be positive and finite");
    }
    for (const double invalid :
         {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()}) {
        auto terms = DepositTerms();
        terms.contractRate_ = invalid;
        AssertTradeValidationFailure(Trade(Dal::RateInstrumentType_("DEPOSIT"), today, today, maturity, terms), market,
                                     "Deposit contract rate must be finite");
    }

    const auto terms = DepositTerms();
    AssertTradeValidationFailure(Trade(Dal::RateInstrumentType_("DEPOSIT"), Dal::Date_(), today, maturity, terms), market,
                                 "Rate trade dates must be valid and start before maturity");
    AssertTradeValidationFailure(Trade(Dal::RateInstrumentType_("DEPOSIT"), today, Dal::Date_(), maturity, terms), market,
                                 "Rate trade dates must be valid and start before maturity");
    AssertTradeValidationFailure(Trade(Dal::RateInstrumentType_("DEPOSIT"), today, today, Dal::Date_(), terms), market,
                                 "Rate trade dates must be valid and start before maturity");
    AssertTradeValidationFailure(Trade(Dal::RateInstrumentType_("DEPOSIT"), today, today, today, terms), market,
                                 "Rate trade dates must be valid and start before maturity");

    auto invalidMarket = market;
    invalidMarket.valuationTime_ = Dal::DateTime_();
    AssertTradeValidationFailure(Trade(Dal::RateInstrumentType_("DEPOSIT"), today, today, maturity, terms), invalidMarket,
                                 "Rate cashflow plan requires a valid valuation time");

    for (const double contractRate : {0.0, -0.01}) {
        auto validTerms = terms;
        validTerms.contractRate_ = contractRate;
        const auto trade = Trade(Dal::RateInstrumentType_("DEPOSIT"), today, today, maturity, validTerms);
        const auto priced = Dal::PriceRateTrade(trade, market);
        const auto sensitivity = Dal::RateTradeNodeSensitivities(trade, market, "discount");
        ASSERT_TRUE(priced.succeeded_);
        ASSERT_TRUE(std::isfinite(priced.pv_));
        ASSERT_TRUE(sensitivity.eligible_);
        ASSERT_TRUE(std::isfinite(sensitivity.pv_));
        ASSERT_EQ(sensitivity.gradient_.size(), 1);
        ASSERT_TRUE(std::isfinite(sensitivity.gradient_[0]));
        ASSERT_TRUE(sensitivity.reason_.empty());
    }

    auto zeroPvTerms = terms;
    zeroPvTerms.contractRate_ = 0.0;
    const auto zeroPvTrade = Trade(Dal::RateInstrumentType_("DEPOSIT"), today, today, maturity, zeroPvTerms);
    const auto zeroPvSensitivity = Dal::RateTradeNodeSensitivities(zeroPvTrade, Market(today, FlatCurve(maturity, 0.0)), "discount");
    ASSERT_TRUE(zeroPvSensitivity.eligible_);
    ASSERT_DOUBLE_EQ(zeroPvSensitivity.pv_, 0.0);
    ASSERT_EQ(zeroPvSensitivity.gradient_.size(), 1);
    ASSERT_TRUE(std::isfinite(zeroPvSensitivity.gradient_[0]));
    ASSERT_TRUE(zeroPvSensitivity.reason_.empty());
}

TEST(RateCashflowPricingTest, TestDepositNodeAADReasonPrecedenceForCombinedFailures) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ maturity(2027, 1, 15);
    const auto supportedMarket = Market(today, FlatCurve(maturity));
    auto invalidTerms = DepositTerms();
    invalidTerms.notional_ = std::numeric_limits<double>::quiet_NaN();
    const auto invalidTrade = Trade(Dal::RateInstrumentType_("DEPOSIT"), today, today, maturity, invalidTerms);

    Dal::FraTradeTerms_ fraTerms;
    AssertCanonicalFailure(
        Dal::RateTradeNodeSensitivities(Trade(Dal::RateInstrumentType_("DEPOSIT"), today, today, maturity, fraTerms), supportedMarket, "discount"),
        "TRADE_FAMILY_NOT_AAD_ENABLED");
    AssertCanonicalFailure(
        Dal::RateTradeNodeSensitivities(Trade(Dal::RateInstrumentType_("FRA"), today, today, maturity, DepositTerms()), supportedMarket, "discount"),
        "TRADE_FAMILY_NOT_AAD_ENABLED");
    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(invalidTrade, supportedMarket, "wrong"), "TRADE_DOES_NOT_DEPEND_ON_COMPONENT");

    auto missingMarket = supportedMarket;
    missingMarket.curveComponents_.erase("discount");
    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(invalidTrade, missingMarket, "discount"), "CURVE_COMPONENT_UNAVAILABLE");
    auto nullMarket = supportedMarket;
    nullMarket.curveComponents_["discount"] = Dal::Handle_<Dal::DiscountCurve_>();
    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(invalidTrade, nullMarket, "discount"), "CURVE_COMPONENT_UNAVAILABLE");

    auto unsupportedMarket = supportedMarket;
    unsupportedMarket.curveComponents_["discount"] = Dal::Handle_<Dal::DiscountCurve_>(std::make_shared<const UnsupportedDiscountCurve_>());
    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(invalidTrade, unsupportedMarket, "discount"), "CURVE_REPRESENTATION_NOT_AAD_ENABLED");
    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(invalidTrade, supportedMarket, "discount"), "TRADE_VALIDATION_FAILED");

    const auto validTrade = Trade(Dal::RateInstrumentType_("DEPOSIT"), today, today, maturity, DepositTerms());
    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(validTrade, supportedMarket, "wrong"), "TRADE_DOES_NOT_DEPEND_ON_COMPONENT");
    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(validTrade, missingMarket, "discount"), "CURVE_COMPONENT_UNAVAILABLE");
    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(validTrade, unsupportedMarket, "discount"), "CURVE_REPRESENTATION_NOT_AAD_ENABLED");
}

TEST(RateCashflowPricingTest, TestDepositNodeAADPwcRawColumnsMatchCentralBumps) {
    constexpr double PWC_NATIVE_PARAMETER_BUMP = 1.0e-6;
    constexpr double PWC_RAW_GRADIENT_ABSOLUTE_TOLERANCE = 1.0e-6;
    constexpr double PWC_RAW_GRADIENT_RELATIVE_TOLERANCE = 1.0e-8;
    const Dal::Vector_<Dal::Date_> knots{Dal::Date_(2026, 7, 15), Dal::Date_(2027, 1, 15), Dal::Date_(2028, 1, 15)};
    const auto buildCurve = [&](const Dal::Vector_<>& parameters) {
        return Dal::Handle_<Dal::DiscountCurve_>(Dal::NewDiscountPWC("pwc", "USD", Dal::PiecewiseConstant_(knots, parameters)));
    };
    AssertRawNodeGradientMatchesCentralBumps(buildCurve, {0.01, -0.005, 0.03}, 3, PWC_NATIVE_PARAMETER_BUMP, PWC_RAW_GRADIENT_ABSOLUTE_TOLERANCE,
                                             PWC_RAW_GRADIENT_RELATIVE_TOLERANCE, {1, 2});
}

TEST(RateCashflowPricingTest, TestDepositNodeAADPwlfRawColumnsMatchCentralBumps) {
    constexpr double PWLF_NATIVE_PARAMETER_BUMP = 1.0e-6;
    constexpr double PWLF_RAW_GRADIENT_ABSOLUTE_TOLERANCE = 1.0e-6;
    constexpr double PWLF_RAW_GRADIENT_RELATIVE_TOLERANCE = 1.0e-8;
    const Dal::Vector_<Dal::Date_> knots{Dal::Date_(2026, 7, 15), Dal::Date_(2027, 1, 15), Dal::Date_(2028, 1, 15)};
    const auto buildCurve = [&](const Dal::Vector_<>& parameters) {
        Dal::Vector_<> left(3);
        Dal::Vector_<> right(3);
        for (int node = 0; node < 3; ++node) {
            left[node] = parameters[2 * node];
            right[node] = parameters[2 * node + 1];
        }
        return Dal::Handle_<Dal::DiscountCurve_>(Dal::NewDiscountPWLF("pwlf", "USD", Dal::PiecewiseLinear_(knots, left, right)));
    };
    AssertRawNodeGradientMatchesCentralBumps(buildCurve, {0.01, 0.0, -0.005, 0.02, 0.03, -0.01}, 6, PWLF_NATIVE_PARAMETER_BUMP,
                                             PWLF_RAW_GRADIENT_ABSOLUTE_TOLERANCE, PWLF_RAW_GRADIENT_RELATIVE_TOLERANCE, {3, 4, 5});
}

TEST(RateCashflowPricingTest, TestDepositNodeAADLogDiscountRawColumnsMatchCentralBumps) {
    constexpr double LOG_DISCOUNT_NATIVE_PARAMETER_BUMP = 1.0e-6;
    constexpr double LOG_DISCOUNT_RAW_GRADIENT_ABSOLUTE_TOLERANCE = 1.0e-6;
    constexpr double LOG_DISCOUNT_RAW_GRADIENT_RELATIVE_TOLERANCE = 1.0e-8;
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Vector_<Dal::Date_> knots{Dal::Date_(2026, 7, 15), Dal::Date_(2027, 1, 15), Dal::Date_(2028, 1, 15)};
    const auto buildCurve = [&](const Dal::Vector_<>& parameters) {
        Dal::Vector_<> stored{0.0};
        stored.Append(parameters);
        return Dal::Handle_<Dal::DiscountCurve_>(Dal::NewDiscountLogDF("logdf", "USD", Dal::Vector_<Dal::Date_>{today, knots[0], knots[1], knots[2]},
                                                                       stored, Dal::DayBasis_("ACT_365F"), Dal::LogDfScheme_::Value_::LOG_LINEAR));
    };
    AssertRawNodeGradientMatchesCentralBumps(buildCurve, {-0.005, 0.0, -0.06}, 3, LOG_DISCOUNT_NATIVE_PARAMETER_BUMP,
                                             LOG_DISCOUNT_RAW_GRADIENT_ABSOLUTE_TOLERANCE, LOG_DISCOUNT_RAW_GRADIENT_RELATIVE_TOLERANCE, {2});
}

TEST(RateCashflowPricingTest, TestDepositNodeAADZeroRateRawColumnsMatchCentralBumps) {
    constexpr double ZERO_RATE_NATIVE_PARAMETER_BUMP = 1.0e-6;
    constexpr double ZERO_RATE_RAW_GRADIENT_ABSOLUTE_TOLERANCE = 1.0e-6;
    constexpr double ZERO_RATE_RAW_GRADIENT_RELATIVE_TOLERANCE = 1.0e-8;
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Vector_<Dal::Date_> knots{Dal::Date_(2026, 7, 15), Dal::Date_(2027, 1, 15), Dal::Date_(2028, 1, 15)};
    const auto buildCurve = [&](const Dal::Vector_<>& parameters) {
        return Dal::Handle_<Dal::DiscountCurve_>(
            Dal::NewDiscountZeroRate("zero", "USD", today, knots, parameters, Dal::DayBasis_("ACT_365F"), Dal::LogDfScheme_::Value_::LOG_LINEAR));
    };
    AssertRawNodeGradientMatchesCentralBumps(buildCurve, {0.01, 0.0, -0.005}, 3, ZERO_RATE_NATIVE_PARAMETER_BUMP,
                                             ZERO_RATE_RAW_GRADIENT_ABSOLUTE_TOLERANCE, ZERO_RATE_RAW_GRADIENT_RELATIVE_TOLERANCE, {2});
}

TEST(RateCashflowPricingTest, TestDepositNodeAADRewindsPerTradeAndIsThreadLocal) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ maturity(2027, 1, 15);
    Dal::DepositTradeTerms_ terms;
    terms.notional_ = 100.0;
    terms.contractRate_ = 0.05;
    terms.lend_ = true;
    terms.index_ = QuarterlyIndex();
    terms.discountComponentKey_ = "discount";
    const auto trade = Trade(Dal::RateInstrumentType_("DEPOSIT"), today, today, maturity, terms);
    const auto evaluate = [=](double rate) { return Dal::RateTradeNodeSensitivities(trade, Market(today, FlatCurve(maturity, rate)), "discount"); };

    const auto first = evaluate(0.04);
    auto invalidTerms = terms;
    invalidTerms.notional_ = std::numeric_limits<double>::quiet_NaN();
    const auto invalidTrade = Trade(Dal::RateInstrumentType_("DEPOSIT"), today, today, maturity, invalidTerms);
    const auto invalid = Dal::RateTradeNodeSensitivities(invalidTrade, Market(today, FlatCurve(maturity, 0.04)), "discount");
    const auto repeated = evaluate(0.04);
    const auto repeatedAgain = evaluate(0.04);
    std::atomic<int> waiting{0};
    std::promise<void> release;
    const auto startTogether = release.get_future().share();
    auto left = std::async(std::launch::async, [&]() {
        ++waiting;
        startTogether.wait();
        return Dal::RateTradeNodeSensitivities(invalidTrade, Market(today, FlatCurve(maturity, 0.03)), "discount");
    });
    auto right = std::async(std::launch::async, [&]() {
        ++waiting;
        startTogether.wait();
        return evaluate(0.06);
    });
    while (waiting.load() != 2)
        std::this_thread::yield();
    release.set_value();
    const auto leftResult = left.get();
    const auto rightResult = right.get();

    ASSERT_TRUE(first.eligible_);
    AssertCanonicalFailure(invalid, "TRADE_VALIDATION_FAILED");
    ASSERT_TRUE(repeated.eligible_);
    ASSERT_TRUE(repeatedAgain.eligible_);
    AssertCanonicalFailure(leftResult, "TRADE_VALIDATION_FAILED");
    ASSERT_TRUE(rightResult.eligible_);
    ASSERT_DOUBLE_EQ(first.pv_, repeated.pv_);
    ASSERT_DOUBLE_EQ(first.pv_, repeatedAgain.pv_);
    ASSERT_EQ(first.gradient_, repeated.gradient_);
    ASSERT_EQ(first.gradient_, repeatedAgain.gradient_);
    ASSERT_EQ(first.reason_, repeated.reason_);
    ASSERT_EQ(first.reason_, repeatedAgain.reason_);
    const auto rightControl = evaluate(0.06);
    ASSERT_EQ(rightResult.pv_, rightControl.pv_);
    ASSERT_EQ(rightResult.gradient_, rightControl.gradient_);
}

TEST(RateCashflowPricingTest, TestRegistryTracksAadEnabledFamiliesAndLockedOnesStayGated) {
    const auto enabled = Dal::RateCashflowPricingInternal::AadEnabledRateFamilies();
    const auto families = Dal::RateInstrumentTypeListAll();
    ASSERT_EQ(enabled.size(), 7);
    for (const auto& family : enabled)
        ASSERT_NE(std::find(families.begin(), families.end(), family), families.end());

    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 10, 15);
    const Dal::Date_ maturity(2029, 1, 15);
    const auto market = Market(today, FlatCurve(maturity));

    auto flipFraSide = [](Dal::FraTradeTerms_ terms) {
        terms.receiveFloating_ = !terms.receiveFloating_;
        return terms;
    };
    auto flipFutureSide = [](Dal::FutureTradeTerms_ terms) {
        terms.long_ = !terms.long_;
        return terms;
    };
    const auto fraOpposite = Dal::RateTradeTerms_(flipFraSide(FraTerms()));
    const auto futureOpposite = Dal::RateTradeTerms_(flipFutureSide(FutureTerms()));
    ASSERT_TRUE(
        Dal::RateTradeNodeSensitivities(Trade(Dal::RateInstrumentType_("DEPOSIT"), today, today, maturity, DepositTerms()), market, "discount")
            .eligible_);
    ASSERT_TRUE(
        Dal::RateTradeNodeSensitivities(Trade(Dal::RateInstrumentType_("FRA"), today, start, maturity, FraTerms()), market, "forecast").eligible_);
    ASSERT_TRUE(Dal::RateTradeNodeSensitivities(Trade(Dal::RateInstrumentType_("FUTURE"), today, start, maturity, FutureTerms()), market, "forecast")
                    .eligible_);
    ASSERT_TRUE(
        Dal::RateTradeNodeSensitivities(Trade(Dal::RateInstrumentType_("FRA"), today, start, maturity, fraOpposite), market, "forecast").eligible_);
    ASSERT_TRUE(Dal::RateTradeNodeSensitivities(Trade(Dal::RateInstrumentType_("FUTURE"), today, start, maturity, futureOpposite), market, "forecast")
                    .eligible_);
    ASSERT_TRUE(Dal::RateTradeNodeSensitivities(
                    Trade(Dal::RateInstrumentType_("OIS"), today, start, maturity, Dal::RateTradeTerms_(Dal::OisTradeTerms_{FixedFloatTerms()})),
                    market, "forecast")
                    .eligible_);
    ASSERT_TRUE(Dal::RateTradeNodeSensitivities(
                    Trade(Dal::RateInstrumentType_("OIS"), today, start, maturity, Dal::RateTradeTerms_(Dal::OisTradeTerms_{FixedFloatTerms(false)})),
                    market, "discount")
                    .eligible_);
    ASSERT_TRUE(Dal::RateTradeNodeSensitivities(
                    Trade(Dal::RateInstrumentType_("IRS"), today, start, maturity, Dal::RateTradeTerms_(Dal::IrsTradeTerms_{FixedFloatTerms()})),
                    market, "forecast")
                    .eligible_);
    ASSERT_TRUE(Dal::RateTradeNodeSensitivities(
                    Trade(Dal::RateInstrumentType_("IRS"), today, start, maturity, Dal::RateTradeTerms_(Dal::IrsTradeTerms_{FixedFloatTerms(false)})),
                    market, "discount")
                    .eligible_);

    const auto basisTrade = Trade(Dal::RateInstrumentType_("BASIS_SWAP"), today, today, maturity, BasisTerms());
    ASSERT_EQ(Dal::BuildRateCashflowPlan(basisTrade, market.valuationTime_).dependencyComponentKeys_,
              (Dal::Vector_<Dal::String_>{"forecast", "reference", "discount"}));
    for (const auto& key : {"forecast", "reference", "discount"})
        ASSERT_TRUE(Dal::RateTradeNodeSensitivities(basisTrade, market, key).eligible_);
    // XCCY is open in the registry; that market carries no cross-currency market, so passive
    // pricing fails and the validation gate — not the family gate — answers (an expired XCCY
    // trade would keep the dependency token).
    AssertCanonicalFailure(
        Dal::RateTradeNodeSensitivities(Trade(Dal::RateInstrumentType_("XCCY"), today, start, maturity, Dal::RateTradeTerms_(XccyTerms())), market,
                                        "forecast"),
        "TRADE_VALIDATION_FAILED");

    AssertCanonicalFailure(
        Dal::RateTradeNodeSensitivities(Trade(Dal::RateInstrumentType_("FUTURE"), today, start, maturity, Dal::RateTradeTerms_(FraTerms())), market,
                                        "forecast"),
        "TRADE_FAMILY_NOT_AAD_ENABLED");
    AssertCanonicalFailure(
        Dal::RateTradeNodeSensitivities(Trade(Dal::RateInstrumentType_("FRA"), today, start, maturity, Dal::RateTradeTerms_(DepositTerms())), market,
                                        "forecast"),
        "TRADE_FAMILY_NOT_AAD_ENABLED");
    AssertCanonicalFailure(
        Dal::RateTradeNodeSensitivities(Trade(Dal::RateInstrumentType_("OIS"), today, start, maturity, Dal::RateTradeTerms_(DepositTerms())), market,
                                        "forecast"),
        "TRADE_FAMILY_NOT_AAD_ENABLED");
}

TEST(RateCashflowPricingTest, TestFraNodeAADReasonPrecedenceForCombinedFailures) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 10, 15);
    const Dal::Date_ maturity(2029, 1, 15);
    const auto forecast = FlatCurve(maturity, 0.04);
    const auto discount = FlatCurve(maturity, 0.03);
    const auto supported = ComponentMarket(today, forecast, discount);
    auto invalidTerms = FraTerms();
    invalidTerms.notional_ = std::numeric_limits<double>::quiet_NaN();
    const auto invalidTrade = Trade(Dal::RateInstrumentType_("FRA"), today, start, maturity, invalidTerms);
    const auto validTrade = Trade(Dal::RateInstrumentType_("FRA"), today, start, maturity, FraTerms());

    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(invalidTrade, supported, "reference"), "TRADE_DOES_NOT_DEPEND_ON_COMPONENT");
    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(validTrade, supported, "reference"), "TRADE_DOES_NOT_DEPEND_ON_COMPONENT");
    AssertCanonicalFailure(
        Dal::RateTradeNodeSensitivities(Trade(Dal::RateInstrumentType_("FUTURE"), today, start, maturity, FutureTerms()), supported, "discount"),
        "TRADE_DOES_NOT_DEPEND_ON_COMPONENT");

    auto missingDiscount = supported;
    missingDiscount.curveComponents_.erase("discount");
    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(invalidTrade, missingDiscount, "forecast"), "CURVE_COMPONENT_UNAVAILABLE");
    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(validTrade, missingDiscount, "forecast"), "CURVE_COMPONENT_UNAVAILABLE");
    auto nullDiscount = supported;
    nullDiscount.curveComponents_["discount"] = Dal::Handle_<Dal::DiscountCurve_>();
    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(validTrade, nullDiscount, "forecast"), "CURVE_COMPONENT_UNAVAILABLE");

    auto unsupportedPassive = supported;
    unsupportedPassive.curveComponents_["discount"] = Dal::Handle_<Dal::DiscountCurve_>(std::make_shared<const UnsupportedDiscountCurve_>());
    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(validTrade, unsupportedPassive, "forecast"), "CURVE_REPRESENTATION_NOT_AAD_ENABLED");
    auto unsupportedTarget = supported;
    unsupportedTarget.curveComponents_["forecast"] = Dal::Handle_<Dal::DiscountCurve_>(std::make_shared<const UnsupportedDiscountCurve_>());
    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(validTrade, unsupportedTarget, "forecast"), "CURVE_REPRESENTATION_NOT_AAD_ENABLED");
    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(invalidTrade, unsupportedPassive, "forecast"), "CURVE_REPRESENTATION_NOT_AAD_ENABLED");

    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(invalidTrade, supported, "forecast"), "TRADE_VALIDATION_FAILED");
}

TEST(RateCashflowPricingTest, TestFraNodeAADForecastPwcRawColumnsMatchCentralBumps) {
    constexpr double PWC_NATIVE_PARAMETER_BUMP = 1.0e-6;
    constexpr double PWC_RAW_GRADIENT_ABSOLUTE_TOLERANCE = 1.0e-6;
    constexpr double PWC_RAW_GRADIENT_RELATIVE_TOLERANCE = 1.0e-8;
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 10, 15);
    const Dal::Date_ maturity(2029, 1, 15);
    const Dal::Vector_<Dal::Date_> knots{Dal::Date_(2026, 7, 15), Dal::Date_(2027, 1, 15), Dal::Date_(2028, 1, 15)};
    const auto buildCurve = [&](const Dal::Vector_<>& parameters) {
        return Dal::Handle_<Dal::DiscountCurve_>(Dal::NewDiscountPWC("pwc", "USD", Dal::PiecewiseConstant_(knots, parameters)));
    };
    const auto assembleMarket = [&](const Dal::Handle_<Dal::DiscountCurve_>& curve) {
        return ComponentMarket(today, curve, FlatCurve(maturity, 0.03));
    };
    auto opposite = FraTerms();
    opposite.receiveFloating_ = false;
    AssertFamilyNodeGradientMatchesCentralBumps(Dal::RateInstrumentType_("FRA"), Dal::RateTradeTerms_(FraTerms()), Dal::RateTradeTerms_(opposite),
                                                "forecast", assembleMarket, buildCurve, {0.01, -0.005, 0.03}, 3, PWC_NATIVE_PARAMETER_BUMP,
                                                PWC_RAW_GRADIENT_ABSOLUTE_TOLERANCE, PWC_RAW_GRADIENT_RELATIVE_TOLERANCE, {1, 2});
}

TEST(RateCashflowPricingTest, TestFraNodeAADForecastPwlfRawColumnsMatchCentralBumps) {
    constexpr double PWLF_NATIVE_PARAMETER_BUMP = 1.0e-6;
    constexpr double PWLF_RAW_GRADIENT_ABSOLUTE_TOLERANCE = 1.0e-6;
    constexpr double PWLF_RAW_GRADIENT_RELATIVE_TOLERANCE = 1.0e-8;
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 10, 15);
    const Dal::Date_ maturity(2029, 1, 15);
    const Dal::Vector_<Dal::Date_> knots{Dal::Date_(2026, 7, 15), Dal::Date_(2027, 1, 15), Dal::Date_(2028, 1, 15)};
    const auto buildCurve = [&](const Dal::Vector_<>& parameters) {
        Dal::Vector_<> left(3);
        Dal::Vector_<> right(3);
        for (int node = 0; node < 3; ++node) {
            left[node] = parameters[2 * node];
            right[node] = parameters[2 * node + 1];
        }
        return Dal::Handle_<Dal::DiscountCurve_>(Dal::NewDiscountPWLF("pwlf", "USD", Dal::PiecewiseLinear_(knots, left, right)));
    };
    const auto assembleMarket = [&](const Dal::Handle_<Dal::DiscountCurve_>& curve) {
        return ComponentMarket(today, curve, FlatCurve(maturity, 0.03));
    };
    auto opposite = FraTerms();
    opposite.receiveFloating_ = false;
    AssertFamilyNodeGradientMatchesCentralBumps(Dal::RateInstrumentType_("FRA"), Dal::RateTradeTerms_(FraTerms()), Dal::RateTradeTerms_(opposite),
                                                "forecast", assembleMarket, buildCurve, {0.01, 0.0, -0.005, 0.02, 0.03, -0.01}, 6,
                                                PWLF_NATIVE_PARAMETER_BUMP, PWLF_RAW_GRADIENT_ABSOLUTE_TOLERANCE,
                                                PWLF_RAW_GRADIENT_RELATIVE_TOLERANCE, {2, 3, 4, 5});
}

TEST(RateCashflowPricingTest, TestFraNodeAADForecastLogDiscountRawColumnsMatchCentralBumps) {
    constexpr double LOG_DISCOUNT_NATIVE_PARAMETER_BUMP = 1.0e-6;
    constexpr double LOG_DISCOUNT_RAW_GRADIENT_ABSOLUTE_TOLERANCE = 1.0e-6;
    constexpr double LOG_DISCOUNT_RAW_GRADIENT_RELATIVE_TOLERANCE = 1.0e-8;
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 10, 15);
    const Dal::Date_ maturity(2029, 1, 15);
    const Dal::Vector_<Dal::Date_> knots{Dal::Date_(2026, 7, 15), Dal::Date_(2027, 1, 15), Dal::Date_(2028, 1, 15)};
    const auto buildCurve = [&](const Dal::Vector_<>& parameters) {
        Dal::Vector_<> stored{0.0};
        stored.Append(parameters);
        return Dal::Handle_<Dal::DiscountCurve_>(Dal::NewDiscountLogDF("logdf", "USD", Dal::Vector_<Dal::Date_>{today, knots[0], knots[1], knots[2]},
                                                                       stored, Dal::DayBasis_("ACT_365F"), Dal::LogDfScheme_::Value_::LOG_LINEAR));
    };
    const auto assembleMarket = [&](const Dal::Handle_<Dal::DiscountCurve_>& curve) {
        return ComponentMarket(today, curve, FlatCurve(maturity, 0.03));
    };
    auto opposite = FraTerms();
    opposite.receiveFloating_ = false;
    AssertFamilyNodeGradientMatchesCentralBumps(Dal::RateInstrumentType_("FRA"), Dal::RateTradeTerms_(FraTerms()), Dal::RateTradeTerms_(opposite),
                                                "forecast", assembleMarket, buildCurve, {-0.005, 0.0, -0.06}, 3, LOG_DISCOUNT_NATIVE_PARAMETER_BUMP,
                                                LOG_DISCOUNT_RAW_GRADIENT_ABSOLUTE_TOLERANCE, LOG_DISCOUNT_RAW_GRADIENT_RELATIVE_TOLERANCE, {2});
}

TEST(RateCashflowPricingTest, TestFraNodeAADForecastZeroRateRawColumnsMatchCentralBumps) {
    constexpr double ZERO_RATE_NATIVE_PARAMETER_BUMP = 1.0e-6;
    constexpr double ZERO_RATE_RAW_GRADIENT_ABSOLUTE_TOLERANCE = 1.0e-6;
    constexpr double ZERO_RATE_RAW_GRADIENT_RELATIVE_TOLERANCE = 1.0e-8;
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 10, 15);
    const Dal::Date_ maturity(2029, 1, 15);
    const Dal::Vector_<Dal::Date_> knots{Dal::Date_(2026, 7, 15), Dal::Date_(2027, 1, 15), Dal::Date_(2028, 1, 15)};
    const auto buildCurve = [&](const Dal::Vector_<>& parameters) {
        return Dal::Handle_<Dal::DiscountCurve_>(
            Dal::NewDiscountZeroRate("zero", "USD", today, knots, parameters, Dal::DayBasis_("ACT_365F"), Dal::LogDfScheme_::Value_::LOG_LINEAR));
    };
    const auto assembleMarket = [&](const Dal::Handle_<Dal::DiscountCurve_>& curve) {
        return ComponentMarket(today, curve, FlatCurve(maturity, 0.03));
    };
    auto opposite = FraTerms();
    opposite.receiveFloating_ = false;
    AssertFamilyNodeGradientMatchesCentralBumps(Dal::RateInstrumentType_("FRA"), Dal::RateTradeTerms_(FraTerms()), Dal::RateTradeTerms_(opposite),
                                                "forecast", assembleMarket, buildCurve, {0.01, 0.0, -0.005}, 3, ZERO_RATE_NATIVE_PARAMETER_BUMP,
                                                ZERO_RATE_RAW_GRADIENT_ABSOLUTE_TOLERANCE, ZERO_RATE_RAW_GRADIENT_RELATIVE_TOLERANCE, {2});
}

TEST(RateCashflowPricingTest, TestFraNodeAADDiscountPwcRawColumnsMatchCentralBumps) {
    constexpr double PWC_NATIVE_PARAMETER_BUMP = 1.0e-6;
    constexpr double PWC_RAW_GRADIENT_ABSOLUTE_TOLERANCE = 1.0e-6;
    constexpr double PWC_RAW_GRADIENT_RELATIVE_TOLERANCE = 1.0e-8;
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 10, 15);
    const Dal::Date_ maturity(2029, 1, 15);
    const Dal::Vector_<Dal::Date_> knots{Dal::Date_(2026, 7, 15), Dal::Date_(2027, 1, 15), Dal::Date_(2028, 1, 15)};
    const auto buildCurve = [&](const Dal::Vector_<>& parameters) {
        return Dal::Handle_<Dal::DiscountCurve_>(Dal::NewDiscountPWC("pwc", "USD", Dal::PiecewiseConstant_(knots, parameters)));
    };
    const auto assembleMarket = [&](const Dal::Handle_<Dal::DiscountCurve_>& curve) {
        return ComponentMarket(today, FlatCurve(maturity, 0.04), curve);
    };
    auto terms = FraTerms(false);
    auto opposite = terms;
    opposite.receiveFloating_ = false;
    AssertFamilyNodeGradientMatchesCentralBumps(Dal::RateInstrumentType_("FRA"), Dal::RateTradeTerms_(terms), Dal::RateTradeTerms_(opposite),
                                                "discount", assembleMarket, buildCurve, {0.01, -0.005, 0.03}, 3, PWC_NATIVE_PARAMETER_BUMP,
                                                PWC_RAW_GRADIENT_ABSOLUTE_TOLERANCE, PWC_RAW_GRADIENT_RELATIVE_TOLERANCE, {1, 2});
}

TEST(RateCashflowPricingTest, TestFraNodeAADDiscountPwlfRawColumnsMatchCentralBumps) {
    constexpr double PWLF_NATIVE_PARAMETER_BUMP = 1.0e-6;
    constexpr double PWLF_RAW_GRADIENT_ABSOLUTE_TOLERANCE = 1.0e-6;
    constexpr double PWLF_RAW_GRADIENT_RELATIVE_TOLERANCE = 1.0e-8;
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 10, 15);
    const Dal::Date_ maturity(2029, 1, 15);
    const Dal::Vector_<Dal::Date_> knots{Dal::Date_(2026, 7, 15), Dal::Date_(2027, 1, 15), Dal::Date_(2028, 1, 15)};
    const auto buildCurve = [&](const Dal::Vector_<>& parameters) {
        Dal::Vector_<> left(3);
        Dal::Vector_<> right(3);
        for (int node = 0; node < 3; ++node) {
            left[node] = parameters[2 * node];
            right[node] = parameters[2 * node + 1];
        }
        return Dal::Handle_<Dal::DiscountCurve_>(Dal::NewDiscountPWLF("pwlf", "USD", Dal::PiecewiseLinear_(knots, left, right)));
    };
    const auto assembleMarket = [&](const Dal::Handle_<Dal::DiscountCurve_>& curve) {
        return ComponentMarket(today, FlatCurve(maturity, 0.04), curve);
    };
    auto terms = FraTerms(false);
    auto opposite = terms;
    opposite.receiveFloating_ = false;
    AssertFamilyNodeGradientMatchesCentralBumps(Dal::RateInstrumentType_("FRA"), Dal::RateTradeTerms_(terms), Dal::RateTradeTerms_(opposite),
                                                "discount", assembleMarket, buildCurve, {0.01, 0.0, -0.005, 0.02, 0.03, -0.01}, 6,
                                                PWLF_NATIVE_PARAMETER_BUMP, PWLF_RAW_GRADIENT_ABSOLUTE_TOLERANCE,
                                                PWLF_RAW_GRADIENT_RELATIVE_TOLERANCE, {2, 3, 4, 5});
}

TEST(RateCashflowPricingTest, TestFraNodeAADDiscountLogDiscountRawColumnsMatchCentralBumps) {
    constexpr double LOG_DISCOUNT_NATIVE_PARAMETER_BUMP = 1.0e-6;
    constexpr double LOG_DISCOUNT_RAW_GRADIENT_ABSOLUTE_TOLERANCE = 1.0e-6;
    constexpr double LOG_DISCOUNT_RAW_GRADIENT_RELATIVE_TOLERANCE = 1.0e-8;
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 10, 15);
    const Dal::Date_ maturity(2029, 1, 15);
    const Dal::Vector_<Dal::Date_> knots{Dal::Date_(2026, 7, 15), Dal::Date_(2027, 1, 15), Dal::Date_(2028, 1, 15)};
    const auto buildCurve = [&](const Dal::Vector_<>& parameters) {
        Dal::Vector_<> stored{0.0};
        stored.Append(parameters);
        return Dal::Handle_<Dal::DiscountCurve_>(Dal::NewDiscountLogDF("logdf", "USD", Dal::Vector_<Dal::Date_>{today, knots[0], knots[1], knots[2]},
                                                                       stored, Dal::DayBasis_("ACT_365F"), Dal::LogDfScheme_::Value_::LOG_LINEAR));
    };
    const auto assembleMarket = [&](const Dal::Handle_<Dal::DiscountCurve_>& curve) {
        return ComponentMarket(today, FlatCurve(maturity, 0.04), curve);
    };
    auto terms = FraTerms(false);
    auto opposite = terms;
    opposite.receiveFloating_ = false;
    AssertFamilyNodeGradientMatchesCentralBumps(Dal::RateInstrumentType_("FRA"), Dal::RateTradeTerms_(terms), Dal::RateTradeTerms_(opposite),
                                                "discount", assembleMarket, buildCurve, {-0.005, 0.0, -0.06}, 3, LOG_DISCOUNT_NATIVE_PARAMETER_BUMP,
                                                LOG_DISCOUNT_RAW_GRADIENT_ABSOLUTE_TOLERANCE, LOG_DISCOUNT_RAW_GRADIENT_RELATIVE_TOLERANCE, {2});
}

TEST(RateCashflowPricingTest, TestFraNodeAADDiscountZeroRateRawColumnsMatchCentralBumps) {
    constexpr double ZERO_RATE_NATIVE_PARAMETER_BUMP = 1.0e-6;
    constexpr double ZERO_RATE_RAW_GRADIENT_ABSOLUTE_TOLERANCE = 1.0e-6;
    constexpr double ZERO_RATE_RAW_GRADIENT_RELATIVE_TOLERANCE = 1.0e-8;
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 10, 15);
    const Dal::Date_ maturity(2029, 1, 15);
    const Dal::Vector_<Dal::Date_> knots{Dal::Date_(2026, 7, 15), Dal::Date_(2027, 1, 15), Dal::Date_(2028, 1, 15)};
    const auto buildCurve = [&](const Dal::Vector_<>& parameters) {
        return Dal::Handle_<Dal::DiscountCurve_>(
            Dal::NewDiscountZeroRate("zero", "USD", today, knots, parameters, Dal::DayBasis_("ACT_365F"), Dal::LogDfScheme_::Value_::LOG_LINEAR));
    };
    const auto assembleMarket = [&](const Dal::Handle_<Dal::DiscountCurve_>& curve) {
        return ComponentMarket(today, FlatCurve(maturity, 0.04), curve);
    };
    auto terms = FraTerms(false);
    auto opposite = terms;
    opposite.receiveFloating_ = false;
    AssertFamilyNodeGradientMatchesCentralBumps(Dal::RateInstrumentType_("FRA"), Dal::RateTradeTerms_(terms), Dal::RateTradeTerms_(opposite),
                                                "discount", assembleMarket, buildCurve, {0.01, 0.0, -0.005}, 3, ZERO_RATE_NATIVE_PARAMETER_BUMP,
                                                ZERO_RATE_RAW_GRADIENT_ABSOLUTE_TOLERANCE, ZERO_RATE_RAW_GRADIENT_RELATIVE_TOLERANCE, {2});
}

TEST(RateCashflowPricingTest, TestFutureNodeAADForecastPwcRawColumnsMatchCentralBumps) {
    constexpr double PWC_NATIVE_PARAMETER_BUMP = 1.0e-6;
    constexpr double PWC_RAW_GRADIENT_ABSOLUTE_TOLERANCE = 1.0e-6;
    constexpr double PWC_RAW_GRADIENT_RELATIVE_TOLERANCE = 1.0e-8;
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 10, 15);
    const Dal::Date_ maturity(2029, 1, 15);
    const Dal::Vector_<Dal::Date_> knots{Dal::Date_(2026, 7, 15), Dal::Date_(2027, 1, 15), Dal::Date_(2028, 1, 15)};
    const auto buildCurve = [&](const Dal::Vector_<>& parameters) {
        return Dal::Handle_<Dal::DiscountCurve_>(Dal::NewDiscountPWC("pwc", "USD", Dal::PiecewiseConstant_(knots, parameters)));
    };
    const auto assembleMarket = [&](const Dal::Handle_<Dal::DiscountCurve_>& curve) {
        return ComponentMarket(today, curve, FlatCurve(maturity, 0.03));
    };
    auto opposite = FutureTerms();
    opposite.long_ = false;
    AssertFamilyNodeGradientMatchesCentralBumps(Dal::RateInstrumentType_("FUTURE"), Dal::RateTradeTerms_(FutureTerms()),
                                                Dal::RateTradeTerms_(opposite), "forecast", assembleMarket, buildCurve, {0.01, -0.005, 0.03}, 3,
                                                PWC_NATIVE_PARAMETER_BUMP, PWC_RAW_GRADIENT_ABSOLUTE_TOLERANCE, PWC_RAW_GRADIENT_RELATIVE_TOLERANCE,
                                                {1, 2});
}

TEST(RateCashflowPricingTest, TestFutureNodeAADForecastPwlfRawColumnsMatchCentralBumps) {
    constexpr double PWLF_NATIVE_PARAMETER_BUMP = 1.0e-6;
    constexpr double PWLF_RAW_GRADIENT_ABSOLUTE_TOLERANCE = 1.0e-6;
    constexpr double PWLF_RAW_GRADIENT_RELATIVE_TOLERANCE = 1.0e-8;
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 10, 15);
    const Dal::Date_ maturity(2029, 1, 15);
    const Dal::Vector_<Dal::Date_> knots{Dal::Date_(2026, 7, 15), Dal::Date_(2027, 1, 15), Dal::Date_(2028, 1, 15)};
    const auto buildCurve = [&](const Dal::Vector_<>& parameters) {
        Dal::Vector_<> left(3);
        Dal::Vector_<> right(3);
        for (int node = 0; node < 3; ++node) {
            left[node] = parameters[2 * node];
            right[node] = parameters[2 * node + 1];
        }
        return Dal::Handle_<Dal::DiscountCurve_>(Dal::NewDiscountPWLF("pwlf", "USD", Dal::PiecewiseLinear_(knots, left, right)));
    };
    const auto assembleMarket = [&](const Dal::Handle_<Dal::DiscountCurve_>& curve) {
        return ComponentMarket(today, curve, FlatCurve(maturity, 0.03));
    };
    auto opposite = FutureTerms();
    opposite.long_ = false;
    AssertFamilyNodeGradientMatchesCentralBumps(Dal::RateInstrumentType_("FUTURE"), Dal::RateTradeTerms_(FutureTerms()),
                                                Dal::RateTradeTerms_(opposite), "forecast", assembleMarket, buildCurve,
                                                {0.01, 0.0, -0.005, 0.02, 0.03, -0.01}, 6, PWLF_NATIVE_PARAMETER_BUMP,
                                                PWLF_RAW_GRADIENT_ABSOLUTE_TOLERANCE, PWLF_RAW_GRADIENT_RELATIVE_TOLERANCE, {2, 3, 4, 5});
}

TEST(RateCashflowPricingTest, TestFutureNodeAADForecastLogDiscountRawColumnsMatchCentralBumps) {
    constexpr double LOG_DISCOUNT_NATIVE_PARAMETER_BUMP = 1.0e-6;
    constexpr double LOG_DISCOUNT_RAW_GRADIENT_ABSOLUTE_TOLERANCE = 1.0e-6;
    constexpr double LOG_DISCOUNT_RAW_GRADIENT_RELATIVE_TOLERANCE = 1.0e-8;
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 10, 15);
    const Dal::Date_ maturity(2029, 1, 15);
    const Dal::Vector_<Dal::Date_> knots{Dal::Date_(2026, 7, 15), Dal::Date_(2027, 1, 15), Dal::Date_(2028, 1, 15)};
    const auto buildCurve = [&](const Dal::Vector_<>& parameters) {
        Dal::Vector_<> stored{0.0};
        stored.Append(parameters);
        return Dal::Handle_<Dal::DiscountCurve_>(Dal::NewDiscountLogDF("logdf", "USD", Dal::Vector_<Dal::Date_>{today, knots[0], knots[1], knots[2]},
                                                                       stored, Dal::DayBasis_("ACT_365F"), Dal::LogDfScheme_::Value_::LOG_LINEAR));
    };
    const auto assembleMarket = [&](const Dal::Handle_<Dal::DiscountCurve_>& curve) {
        return ComponentMarket(today, curve, FlatCurve(maturity, 0.03));
    };
    auto opposite = FutureTerms();
    opposite.long_ = false;
    AssertFamilyNodeGradientMatchesCentralBumps(Dal::RateInstrumentType_("FUTURE"), Dal::RateTradeTerms_(FutureTerms()),
                                                Dal::RateTradeTerms_(opposite), "forecast", assembleMarket, buildCurve, {-0.005, 0.0, -0.06}, 3,
                                                LOG_DISCOUNT_NATIVE_PARAMETER_BUMP, LOG_DISCOUNT_RAW_GRADIENT_ABSOLUTE_TOLERANCE,
                                                LOG_DISCOUNT_RAW_GRADIENT_RELATIVE_TOLERANCE, {2});
}

TEST(RateCashflowPricingTest, TestFutureNodeAADForecastZeroRateRawColumnsMatchCentralBumps) {
    constexpr double ZERO_RATE_NATIVE_PARAMETER_BUMP = 1.0e-6;
    constexpr double ZERO_RATE_RAW_GRADIENT_ABSOLUTE_TOLERANCE = 1.0e-6;
    constexpr double ZERO_RATE_RAW_GRADIENT_RELATIVE_TOLERANCE = 1.0e-8;
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 10, 15);
    const Dal::Date_ maturity(2029, 1, 15);
    const Dal::Vector_<Dal::Date_> knots{Dal::Date_(2026, 7, 15), Dal::Date_(2027, 1, 15), Dal::Date_(2028, 1, 15)};
    const auto buildCurve = [&](const Dal::Vector_<>& parameters) {
        return Dal::Handle_<Dal::DiscountCurve_>(
            Dal::NewDiscountZeroRate("zero", "USD", today, knots, parameters, Dal::DayBasis_("ACT_365F"), Dal::LogDfScheme_::Value_::LOG_LINEAR));
    };
    const auto assembleMarket = [&](const Dal::Handle_<Dal::DiscountCurve_>& curve) {
        return ComponentMarket(today, curve, FlatCurve(maturity, 0.03));
    };
    auto opposite = FutureTerms();
    opposite.long_ = false;
    AssertFamilyNodeGradientMatchesCentralBumps(Dal::RateInstrumentType_("FUTURE"), Dal::RateTradeTerms_(FutureTerms()),
                                                Dal::RateTradeTerms_(opposite), "forecast", assembleMarket, buildCurve, {0.01, 0.0, -0.005}, 3,
                                                ZERO_RATE_NATIVE_PARAMETER_BUMP, ZERO_RATE_RAW_GRADIENT_ABSOLUTE_TOLERANCE,
                                                ZERO_RATE_RAW_GRADIENT_RELATIVE_TOLERANCE, {2});
}

TEST(RateCashflowPricingTest, TestOisNodeAADForecastRawColumnsMatchCentralBumpsAllRepresentations) {
    AssertFixedFloatNodeBatteries(Dal::RateInstrumentType_("OIS"), "forecast");
}

TEST(RateCashflowPricingTest, TestOisNodeAADDiscountRawColumnsMatchCentralBumpsAllRepresentations) {
    AssertFixedFloatNodeBatteries(Dal::RateInstrumentType_("OIS"), "discount");
}

TEST(RateCashflowPricingTest, TestIrsNodeAADForecastRawColumnsMatchCentralBumpsAllRepresentations) {
    AssertFixedFloatNodeBatteries(Dal::RateInstrumentType_("IRS"), "forecast");
}

TEST(RateCashflowPricingTest, TestIrsNodeAADDiscountRawColumnsMatchCentralBumpsAllRepresentations) {
    AssertFixedFloatNodeBatteries(Dal::RateInstrumentType_("IRS"), "discount");
}

TEST(RateCashflowPricingTest, TestOisAndIrsNodeAADActivePvEqualsPassiveBitwise) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 2, 15);
    const Dal::Date_ maturity(2026, 4, 15);
    const auto market = ComponentMarket(today, FlatCurve(maturity, 0.04), FlatCurve(maturity, 0.03));

    for (const bool payFixed : {true, false}) {
        const auto ois =
            Trade(Dal::RateInstrumentType_("OIS"), today, start, maturity, Dal::RateTradeTerms_(Dal::OisTradeTerms_{FixedFloatTerms(payFixed)}));
        const auto irs =
            Trade(Dal::RateInstrumentType_("IRS"), today, start, maturity, Dal::RateTradeTerms_(Dal::IrsTradeTerms_{FixedFloatTerms(payFixed)}));
        for (const auto& trade : {ois, irs}) {
            const auto passive = Dal::PriceRateTrade(trade, market);
            ASSERT_TRUE(passive.succeeded_);
            const auto forecastSensitivity = Dal::RateTradeNodeSensitivities(trade, market, "forecast");
            const auto discountSensitivity = Dal::RateTradeNodeSensitivities(trade, market, "discount");
            ASSERT_TRUE(forecastSensitivity.eligible_);
            ASSERT_TRUE(discountSensitivity.eligible_);
            ASSERT_DOUBLE_EQ(forecastSensitivity.pv_, passive.pv_);
            ASSERT_DOUBLE_EQ(discountSensitivity.pv_, passive.pv_);
            ASSERT_NE(forecastSensitivity.gradient_[0], 0.0);
            ASSERT_NE(discountSensitivity.gradient_[0], 0.0);
        }
    }
}

TEST(RateCashflowPricingTest, TestOisDailyCompoundingTapePathMatchesClosedForm) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 3, 16);
    const Dal::Date_ maturity(2026, 5, 15);
    const double forecastRate = 0.04;
    const double discountRate = 0.03;
    const auto terms = FixedFloatTerms();
    const auto trade = Trade(Dal::RateInstrumentType_("OIS"), today, start, maturity, Dal::RateTradeTerms_(Dal::OisTradeTerms_{terms}));
    const auto market = ComponentMarket(today, FlatCurve(maturity, forecastRate), FlatCurve(maturity, discountRate));
    const auto passive = Dal::PriceRateTrade(trade, market);
    ASSERT_TRUE(passive.succeeded_);

    // Business-day window (no schedule adjustment), flat forwards: the daily-compounding loop
    // collapses to prod(1 + f_d*tau_d) = exp(f*tau), so the float leg pays N*(exp(f*tau) - 1)
    // against the fixed leg's N*k*tau, both discounted from the valuation date by exp(-r*t).
    const double tau = terms.floatIndex_.dayBasis_(start, maturity, nullptr);
    const double toPayment = terms.fixedLeg_.dayBasis_(today, maturity, nullptr);
    const double expected = terms.notional_ * (std::exp(forecastRate * tau) - 1.0 - terms.contractRate_ * tau) * std::exp(-discountRate * toPayment);
    ASSERT_NEAR(passive.pv_, expected, 1.0e-8);

    const auto forecastSensitivity = Dal::RateTradeNodeSensitivities(trade, market, "forecast");
    const auto discountSensitivity = Dal::RateTradeNodeSensitivities(trade, market, "discount");
    ASSERT_TRUE(forecastSensitivity.eligible_);
    ASSERT_TRUE(discountSensitivity.eligible_);
    ASSERT_DOUBLE_EQ(forecastSensitivity.pv_, passive.pv_);
    ASSERT_DOUBLE_EQ(discountSensitivity.pv_, passive.pv_);
    ASSERT_NE(forecastSensitivity.gradient_[0], 0.0);
    ASSERT_NE(discountSensitivity.gradient_[0], 0.0);

    constexpr double epsilon = 1.0e-6;
    const auto central = [&](double forecastRateBump, double discountRateBump) {
        return Dal::PriceRateTrade(trade, ComponentMarket(today, FlatCurve(maturity, forecastRate + forecastRateBump),
                                                          FlatCurve(maturity, discountRate + discountRateBump)))
            .pv_;
    };
    // The compounding curvature sets the central-difference truncation; see the battery driver.
    const double tolerance = 1.0e-6 + 1.0e-7 * std::max(std::abs(forecastSensitivity.gradient_[0]),
                                                        std::abs((central(epsilon, 0.0) - central(-epsilon, 0.0)) / (2.0 * epsilon)));
    ASSERT_NEAR(forecastSensitivity.gradient_[0], (central(epsilon, 0.0) - central(-epsilon, 0.0)) / (2.0 * epsilon), tolerance);
    const double discountTolerance = 1.0e-6 + 1.0e-7 * std::max(std::abs(discountSensitivity.gradient_[0]),
                                                                std::abs((central(0.0, epsilon) - central(0.0, -epsilon)) / (2.0 * epsilon)));
    ASSERT_NEAR(discountSensitivity.gradient_[0], (central(0.0, epsilon) - central(0.0, -epsilon)) / (2.0 * epsilon), discountTolerance);
}

TEST(RateCashflowPricingTest, TestOisAndIrsNodeAADFixedAndFloatLegGradientStructure) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 3, 16);
    const Dal::Date_ maturity(2026, 5, 15);
    const double discountRate = 0.03;
    const auto market = ComponentMarket(today, FlatCurve(maturity, 0.04), FlatCurve(maturity, discountRate));
    const auto fullTerms = FixedFloatTerms();
    auto floatOnlyTerms = FixedFloatTerms();
    floatOnlyTerms.contractRate_ = 0.0;
    const double tau = fullTerms.floatIndex_.dayBasis_(start, maturity, nullptr);
    const double toPayment = fullTerms.fixedLeg_.dayBasis_(today, maturity, nullptr);
    const double paymentDf = std::exp(-discountRate * toPayment);
    const double fixedLegPv = -fullTerms.notional_ * fullTerms.contractRate_ * tau * paymentDf;
    const double fixedLegDiscountGradient = fullTerms.notional_ * fullTerms.contractRate_ * tau * toPayment * paymentDf;

    for (const auto& family : {Dal::RateInstrumentType_("OIS"), Dal::RateInstrumentType_("IRS")}) {
        const auto full = Dal::PriceRateTrade(Trade(family, today, start, maturity, AsFamilyTerms(family, fullTerms)), market);
        const auto floatOnly = Dal::PriceRateTrade(Trade(family, today, start, maturity, AsFamilyTerms(family, floatOnlyTerms)), market);
        ASSERT_TRUE(full.succeeded_);
        ASSERT_TRUE(floatOnly.succeeded_);
        ASSERT_NEAR(full.pv_ - floatOnly.pv_, fixedLegPv, 1.0e-8 * std::max(1.0, std::abs(fixedLegPv)));

        const auto fullGradient =
            Dal::RateTradeNodeSensitivities(Trade(family, today, start, maturity, AsFamilyTerms(family, fullTerms)), market, "discount");
        const auto floatOnlyGradient =
            Dal::RateTradeNodeSensitivities(Trade(family, today, start, maturity, AsFamilyTerms(family, floatOnlyTerms)), market, "discount");
        ASSERT_TRUE(fullGradient.eligible_);
        ASSERT_TRUE(floatOnlyGradient.eligible_);
        ASSERT_EQ(fullGradient.gradient_.size(), 1);
        ASSERT_EQ(floatOnlyGradient.gradient_.size(), 1);
        ASSERT_NEAR(fullGradient.gradient_[0] - floatOnlyGradient.gradient_[0], fixedLegDiscountGradient,
                    1.0e-6 + 1.0e-8 * std::abs(fixedLegDiscountGradient));
        ASSERT_NE(floatOnlyGradient.gradient_[0], 0.0);
    }
}

TEST(RateCashflowPricingTest, TestOisAndIrsNodeAADReasonPrecedenceForCombinedFailures) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 10, 15);
    const Dal::Date_ maturity(2029, 1, 15);
    const auto supported = ComponentMarket(today, FlatCurve(maturity, 0.04), FlatCurve(maturity, 0.03));
    auto invalidTerms = FixedFloatTerms();
    invalidTerms.notional_ = std::numeric_limits<double>::quiet_NaN();
    const auto invalidOis = Trade(Dal::RateInstrumentType_("OIS"), today, start, maturity, Dal::RateTradeTerms_(Dal::OisTradeTerms_{invalidTerms}));
    const auto invalidIrs = Trade(Dal::RateInstrumentType_("IRS"), today, start, maturity, Dal::RateTradeTerms_(Dal::IrsTradeTerms_{invalidTerms}));
    const auto validOis =
        Trade(Dal::RateInstrumentType_("OIS"), today, start, maturity, Dal::RateTradeTerms_(Dal::OisTradeTerms_{FixedFloatTerms()}));
    const auto validIrs =
        Trade(Dal::RateInstrumentType_("IRS"), today, start, maturity, Dal::RateTradeTerms_(Dal::IrsTradeTerms_{FixedFloatTerms()}));

    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(invalidOis, supported, "reference"), "TRADE_DOES_NOT_DEPEND_ON_COMPONENT");
    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(validOis, supported, "reference"), "TRADE_DOES_NOT_DEPEND_ON_COMPONENT");
    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(validIrs, supported, "reference"), "TRADE_DOES_NOT_DEPEND_ON_COMPONENT");

    auto missingDiscount = supported;
    missingDiscount.curveComponents_.erase("discount");
    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(validOis, missingDiscount, "forecast"), "CURVE_COMPONENT_UNAVAILABLE");
    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(validIrs, missingDiscount, "discount"), "CURVE_COMPONENT_UNAVAILABLE");
    auto nullDiscount = supported;
    nullDiscount.curveComponents_["discount"] = Dal::Handle_<Dal::DiscountCurve_>();
    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(validOis, nullDiscount, "discount"), "CURVE_COMPONENT_UNAVAILABLE");

    auto unsupportedPassive = supported;
    unsupportedPassive.curveComponents_["discount"] = Dal::Handle_<Dal::DiscountCurve_>(std::make_shared<const UnsupportedDiscountCurve_>());
    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(validOis, unsupportedPassive, "forecast"), "CURVE_REPRESENTATION_NOT_AAD_ENABLED");
    auto unsupportedTarget = supported;
    unsupportedTarget.curveComponents_["forecast"] = Dal::Handle_<Dal::DiscountCurve_>(std::make_shared<const UnsupportedDiscountCurve_>());
    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(validIrs, unsupportedTarget, "forecast"), "CURVE_REPRESENTATION_NOT_AAD_ENABLED");
    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(invalidOis, unsupportedPassive, "forecast"), "CURVE_REPRESENTATION_NOT_AAD_ENABLED");

    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(invalidOis, supported, "forecast"), "TRADE_VALIDATION_FAILED");
    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(invalidIrs, supported, "discount"), "TRADE_VALIDATION_FAILED");
}

TEST(RateCashflowPricingTest, TestOisNodeAADFixingStatesProjectedSuppliedAndMissing) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 3, 16);
    const Dal::Date_ maturity(2026, 5, 15);
    const Dal::Vector_<Dal::Date_> forecastKnots{Dal::Date_(2026, 7, 15), Dal::Date_(2027, 1, 15), Dal::Date_(2028, 1, 15)};
    const auto forecast =
        Dal::Handle_<Dal::DiscountCurve_>(Dal::NewDiscountPWC("pwc", "USD", Dal::PiecewiseConstant_(forecastKnots, {0.01, 0.012, 0.015})));
    const auto discount = FlatCurve(maturity, 0.03);
    const auto trade = Trade(Dal::RateInstrumentType_("OIS"), today, start, maturity, Dal::RateTradeTerms_(Dal::OisTradeTerms_{FixedFloatTerms()}));

    {
        // Multi-period window so the daily loop spans two forecast columns before any fixing.
        const auto spanning = Trade(Dal::RateInstrumentType_("OIS"), today, start, Dal::Date_(2027, 5, 15),
                                    Dal::RateTradeTerms_(Dal::OisTradeTerms_{FixedFloatTerms()}));
        const auto market = ComponentMarket(today, forecast, FlatCurve(Dal::Date_(2027, 5, 15), 0.03));
        const auto projected = Dal::RateTradeNodeSensitivities(spanning, market, "forecast");
        ASSERT_TRUE(projected.eligible_);
        ASSERT_EQ(projected.gradient_.size(), 3);
        ASSERT_NE(projected.gradient_[0], 0.0);
        ASSERT_NE(projected.gradient_[1], 0.0);
        ASSERT_DOUBLE_EQ(projected.gradient_[2], 0.0);
    }

    const Dal::Date_ valuation(2026, 3, 17);
    Dal::MarketFixingSnapshot_::values_t firstDay;
    firstDay["USD-SOFR"][Dal::DateTime_(start, 11, 0)] = 0.0105;
    auto partiallySupplied = ComponentMarket(valuation, forecast, discount);
    partiallySupplied.fixings_ = Dal::Handle_<Dal::MarketFixingSnapshot_>(new Dal::MarketFixingSnapshot_(firstDay));
    {
        // One elapsed daily fixing supplied, every later observation still projected: the tape
        // path mixes a constant leading factor with active daily reads.
        const auto passive = Dal::PriceRateTrade(trade, partiallySupplied);
        ASSERT_TRUE(passive.succeeded_);
        ASSERT_EQ(passive.requiredHistoricalFixings_.size(), 1);
        ASSERT_TRUE(passive.missingHistoricalFixings_.empty());
        const auto forecastSensitivity = Dal::RateTradeNodeSensitivities(trade, partiallySupplied, "forecast");
        ASSERT_TRUE(forecastSensitivity.eligible_);
        ASSERT_DOUBLE_EQ(forecastSensitivity.pv_, passive.pv_);
        ASSERT_NE(forecastSensitivity.gradient_[0], 0.0);
    }

    {
        // Valuation on the payment date with a two-day payment lag: every daily observation is
        // historical and supplied, so the whole float leg is a constant and the forecast
        // gradient is structurally zero while the payment discount stays live.
        Dal::MarketFixingSnapshot_::values_t full;
        for (Dal::Date_ day = start; day < maturity; day = day.AddDays(1))
            full["USD-SOFR"][Dal::DateTime_(day, 11, 0)] = 0.0105;
        auto laggedTerms = FixedFloatTerms();
        laggedTerms.floatLeg_.paymentLag_ = 2;
        const auto lagged = Trade(Dal::RateInstrumentType_("OIS"), today, start, maturity, Dal::RateTradeTerms_(Dal::OisTradeTerms_{laggedTerms}));
        auto supplied = ComponentMarket(maturity, forecast, discount);
        supplied.fixings_ = Dal::Handle_<Dal::MarketFixingSnapshot_>(new Dal::MarketFixingSnapshot_(full));
        const auto passive = Dal::PriceRateTrade(lagged, supplied);
        ASSERT_TRUE(passive.succeeded_);
        const auto forecastSensitivity = Dal::RateTradeNodeSensitivities(lagged, supplied, "forecast");
        const auto discountSensitivity = Dal::RateTradeNodeSensitivities(lagged, supplied, "discount");
        ASSERT_TRUE(forecastSensitivity.eligible_);
        ASSERT_TRUE(discountSensitivity.eligible_);
        ASSERT_DOUBLE_EQ(forecastSensitivity.pv_, passive.pv_);
        ASSERT_DOUBLE_EQ(discountSensitivity.pv_, passive.pv_);
        for (const double column : forecastSensitivity.gradient_)
            ASSERT_DOUBLE_EQ(column, 0.0);
        ASSERT_NE(discountSensitivity.gradient_[0], 0.0);
    }

    auto missing = ComponentMarket(valuation, forecast, discount);
    {
        const auto passive = Dal::PriceRateTrade(trade, missing);
        ASSERT_FALSE(passive.succeeded_);
        ASSERT_EQ(passive.missingHistoricalFixings_.size(), 1);
        AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(trade, missing, "forecast"), "TRADE_VALIDATION_FAILED");
    }
}

TEST(RateCashflowPricingTest, TestIrsNodeAADFixingStatesProjectedSuppliedAndMissing) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 3, 16);
    const Dal::Date_ maturity(2026, 5, 15);
    const auto trade = Trade(Dal::RateInstrumentType_("IRS"), today, start, maturity, Dal::RateTradeTerms_(Dal::IrsTradeTerms_{FixedFloatTerms()}));
    const Dal::Vector_<Dal::Date_> forecastKnots{Dal::Date_(2026, 7, 15), Dal::Date_(2027, 1, 15), Dal::Date_(2028, 1, 15)};
    const auto forecast =
        Dal::Handle_<Dal::DiscountCurve_>(Dal::NewDiscountPWC("pwc", "USD", Dal::PiecewiseConstant_(forecastKnots, {0.01, 0.012, 0.015})));
    const auto discount = FlatCurve(maturity, 0.03);

    {
        const auto market = ComponentMarket(today, forecast, discount);
        const auto projected = Dal::RateTradeNodeSensitivities(trade, market, "forecast");
        ASSERT_TRUE(projected.eligible_);
        ASSERT_EQ(projected.gradient_.size(), 3);
        ASSERT_NE(projected.gradient_[0], 0.0);
        ASSERT_DOUBLE_EQ(projected.gradient_[1], 0.0);
        ASSERT_DOUBLE_EQ(projected.gradient_[2], 0.0);
    }

    const Dal::Date_ valuation(2026, 3, 17);
    Dal::MarketFixingSnapshot_::values_t history;
    history["USD-SOFR"][Dal::DateTime_(start, 11, 0)] = 0.0105;
    auto supplied = ComponentMarket(valuation, forecast, discount);
    supplied.fixings_ = Dal::Handle_<Dal::MarketFixingSnapshot_>(new Dal::MarketFixingSnapshot_(history));
    {
        const auto passive = Dal::PriceRateTrade(trade, supplied);
        ASSERT_TRUE(passive.succeeded_);
        ASSERT_EQ(passive.requiredHistoricalFixings_.size(), 1);
        ASSERT_TRUE(passive.missingHistoricalFixings_.empty());

        const auto forecastSensitivity = Dal::RateTradeNodeSensitivities(trade, supplied, "forecast");
        ASSERT_TRUE(forecastSensitivity.eligible_);
        ASSERT_DOUBLE_EQ(forecastSensitivity.pv_, passive.pv_);
        for (const double column : forecastSensitivity.gradient_)
            ASSERT_DOUBLE_EQ(column, 0.0);

        const auto discountSensitivity = Dal::RateTradeNodeSensitivities(trade, supplied, "discount");
        ASSERT_TRUE(discountSensitivity.eligible_);
        ASSERT_DOUBLE_EQ(discountSensitivity.pv_, passive.pv_);
        ASSERT_NE(discountSensitivity.gradient_[0], 0.0);
    }

    auto missing = ComponentMarket(valuation, forecast, discount);
    {
        const auto passive = Dal::PriceRateTrade(trade, missing);
        ASSERT_FALSE(passive.succeeded_);
        ASSERT_EQ(passive.missingHistoricalFixings_.size(), 1);
        AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(trade, missing, "discount"), "TRADE_VALIDATION_FAILED");
    }
}

TEST(RateCashflowPricingTest, TestOisAndIrsNodeAADBeforeAndAfterMaturityExpiry) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 2, 15);
    const Dal::Date_ maturity(2026, 4, 15);
    const auto expired =
        ComponentMarket(Dal::Date_(maturity.AddDays(1)), FlatCurve(maturity.AddDays(30), 0.04), FlatCurve(maturity.AddDays(30), 0.03));

    for (const auto& family : {Dal::RateInstrumentType_("OIS"), Dal::RateInstrumentType_("IRS")}) {
        const auto trade = Trade(family, today, start, maturity, AsFamilyTerms(family, FixedFloatTerms(true)));
        const auto liveMarket = ComponentMarket(today, FlatCurve(maturity, 0.04), FlatCurve(maturity, 0.03));
        const auto liveForecast = Dal::RateTradeNodeSensitivities(trade, liveMarket, "forecast");
        const auto liveDiscount = Dal::RateTradeNodeSensitivities(trade, liveMarket, "discount");
        ASSERT_TRUE(liveForecast.eligible_);
        ASSERT_TRUE(liveDiscount.eligible_);
        ASSERT_NE(liveForecast.pv_, 0.0);
        ASSERT_NE(liveForecast.gradient_[0], 0.0);
        ASSERT_NE(liveDiscount.gradient_[0], 0.0);

        const auto settledForecast = Dal::RateTradeNodeSensitivities(trade, expired, "forecast");
        const auto settledDiscount = Dal::RateTradeNodeSensitivities(trade, expired, "discount");
        ASSERT_TRUE(settledForecast.eligible_);
        ASSERT_TRUE(settledDiscount.eligible_);
        ASSERT_DOUBLE_EQ(settledForecast.pv_, 0.0);
        ASSERT_DOUBLE_EQ(settledDiscount.pv_, 0.0);
        for (const double column : settledForecast.gradient_)
            ASSERT_DOUBLE_EQ(column, 0.0);
        for (const double column : settledDiscount.gradient_)
            ASSERT_DOUBLE_EQ(column, 0.0);
    }
}

TEST(RateCashflowPricingTest, TestOisAndIrsNodeAADIsolationFromPassiveNodeCount) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 10, 15);
    const Dal::Date_ maturity(2029, 1, 15);
    const Dal::Vector_<Dal::Date_> forecastKnots{Dal::Date_(2026, 7, 15), Dal::Date_(2027, 1, 15), Dal::Date_(2028, 1, 15)};
    const auto forecast =
        Dal::Handle_<Dal::DiscountCurve_>(Dal::NewDiscountPWC("pwc", "USD", Dal::PiecewiseConstant_(forecastKnots, {0.01, -0.005, 0.03})));
    Dal::Vector_<Dal::Date_> denseKnots;
    for (Dal::Date_ knot = Dal::Date_(2026, 2, 15); knot <= Dal::Date_(2029, 12, 15); knot = knot.AddDays(30))
        denseKnots.push_back(knot);
    const Dal::Vector_<> denseForward(denseKnots.size(), 0.03);
    const auto denseDiscount =
        Dal::Handle_<Dal::DiscountCurve_>(Dal::NewDiscountPWC("dense", "USD", Dal::PiecewiseConstant_(denseKnots, denseForward)));

    for (const auto& family : {Dal::RateInstrumentType_("OIS"), Dal::RateInstrumentType_("IRS")}) {
        const auto trade = Trade(family, today, start, maturity, AsFamilyTerms(family, FixedFloatTerms(true)));
        const auto small = Dal::RateTradeNodeSensitivities(trade, ComponentMarket(today, forecast, FlatCurve(maturity, 0.03)), "forecast");
        const auto large = Dal::RateTradeNodeSensitivities(trade, ComponentMarket(today, forecast, denseDiscount), "forecast");

        ASSERT_TRUE(small.eligible_);
        ASSERT_TRUE(large.eligible_);
        ASSERT_EQ(small.gradient_.size(), 3);
        ASSERT_EQ(large.gradient_.size(), 3);
        ASSERT_NEAR(large.pv_, small.pv_, 1.0e-10 * std::max(1.0, std::abs(small.pv_)));
        for (int column = 0; column < 3; ++column)
            ASSERT_NEAR(large.gradient_[column], small.gradient_[column], 1.0e-10 * std::max(1.0, std::abs(small.gradient_[column])))
                << "column " << column;

        const auto discountSensitivity = Dal::RateTradeNodeSensitivities(trade, ComponentMarket(today, forecast, denseDiscount), "discount");
        ASSERT_TRUE(discountSensitivity.eligible_);
        ASSERT_EQ(static_cast<int>(discountSensitivity.gradient_.size()), static_cast<int>(denseKnots.size()));
    }
}

TEST(RateCashflowPricingTest, TestBasisNodeAADSpreadForecastRawColumnsMatchCentralBumpsAllRepresentations) { AssertBasisNodeBatteries("forecast"); }

TEST(RateCashflowPricingTest, TestBasisNodeAADReferenceForecastRawColumnsMatchCentralBumpsAllRepresentations) {
    AssertBasisNodeBatteries("reference");
}

TEST(RateCashflowPricingTest, TestBasisNodeAADDiscountRawColumnsMatchCentralBumpsAllRepresentations) { AssertBasisNodeBatteries("discount"); }

TEST(RateCashflowPricingTest, TestBasisNodeAADActivePvEqualsPassiveBitwise) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 2, 15);
    const Dal::Date_ maturity(2026, 4, 15);
    const auto market = ComponentMarket(today, FlatCurve(maturity, 0.04), FlatCurve(maturity, 0.025), FlatCurve(maturity, 0.03));

    for (const bool receiveReference : {true, false}) {
        const auto trade = Trade(Dal::RateInstrumentType_("BASIS_SWAP"), today, start, maturity, BasisTerms(receiveReference));
        const auto passive = Dal::PriceRateTrade(trade, market);
        ASSERT_TRUE(passive.succeeded_);
        for (const auto& key : {"forecast", "reference", "discount"}) {
            const auto sensitivity = Dal::RateTradeNodeSensitivities(trade, market, key);
            ASSERT_TRUE(sensitivity.eligible_);
            ASSERT_DOUBLE_EQ(sensitivity.pv_, passive.pv_);
            ASSERT_NE(sensitivity.gradient_[0], 0.0);
        }
    }
}

TEST(RateCashflowPricingTest, TestBasisNodeAADThreeComponentIsolation) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 10, 15);
    const Dal::Date_ maturity(2029, 1, 15);
    // The last knot sits inside the accrual span: a PWC parameter is live only from its knot
    // date, so a final knot beyond maturity would leave the last column structurally zero.
    const Dal::Vector_<Dal::Date_> knots{Dal::Date_(2027, 1, 15), Dal::Date_(2028, 1, 15), Dal::Date_(2028, 10, 15)};
    const auto spread = Dal::Handle_<Dal::DiscountCurve_>(Dal::NewDiscountPWC("spread", "USD", Dal::PiecewiseConstant_(knots, {0.01, -0.005, 0.03})));
    const auto reference =
        Dal::Handle_<Dal::DiscountCurve_>(Dal::NewDiscountPWC("reference", "USD", Dal::PiecewiseConstant_(knots, {0.02, 0.015, 0.025})));
    const auto trade = Trade(Dal::RateInstrumentType_("BASIS_SWAP"), today, start, maturity, BasisTerms());

    {
        // Each dependency reports exactly its own parameter columns — the other two components
        // contribute none — and every column is live at these knots.
        const auto market = ComponentMarket(today, spread, reference, FlatCurve(maturity, 0.03));
        const auto passive = Dal::PriceRateTrade(trade, market);
        ASSERT_TRUE(passive.succeeded_);
        for (const auto& [key, expectedSize] : {std::pair<Dal::String_, int>{"forecast", 3}, {"reference", 3}, {"discount", 1}}) {
            const auto sensitivity = Dal::RateTradeNodeSensitivities(trade, market, key);
            ASSERT_TRUE(sensitivity.eligible_);
            ASSERT_EQ(static_cast<int>(sensitivity.gradient_.size()), expectedSize);
            ASSERT_DOUBLE_EQ(sensitivity.pv_, passive.pv_);
            for (const double column : sensitivity.gradient_)
                ASSERT_NE(column, 0.0) << key << " column expected live";
        }
    }

    // Dense passive dependencies with constant forwards equal to the flat curves they replace: the
    // passive components are double curves that never record on the tape, so the target's PV and
    // gradient do not scale with the passive node count.
    Dal::Vector_<Dal::Date_> denseKnots;
    for (Dal::Date_ knot = Dal::Date_(2026, 2, 15); knot <= Dal::Date_(2029, 12, 15); knot = knot.AddDays(30))
        denseKnots.push_back(knot);
    const auto dense = [&](double forward) {
        return Dal::Handle_<Dal::DiscountCurve_>(
            Dal::NewDiscountPWC("dense", "USD", Dal::PiecewiseConstant_(denseKnots, Dal::Vector_<>(denseKnots.size(), forward))));
    };
    const auto assertDensePassivesLeaveTargetUntouched = [&](const Dal::String_& key, const auto& flat, const auto& withDense) {
        const auto small = Dal::RateTradeNodeSensitivities(trade, flat, key);
        const auto large = Dal::RateTradeNodeSensitivities(trade, withDense, key);
        ASSERT_TRUE(small.eligible_);
        ASSERT_TRUE(large.eligible_);
        ASSERT_EQ(small.gradient_.size(), large.gradient_.size());
        ASSERT_NEAR(large.pv_, small.pv_, 1.0e-10 * std::max(1.0, std::abs(small.pv_)));
        for (int column = 0; column < static_cast<int>(small.gradient_.size()); ++column)
            ASSERT_NEAR(large.gradient_[column], small.gradient_[column], 1.0e-10 * std::max(1.0, std::abs(small.gradient_[column])))
                << "column " << column;
    };
    assertDensePassivesLeaveTargetUntouched("forecast", ComponentMarket(today, spread, FlatCurve(maturity, 0.025), FlatCurve(maturity, 0.03)),
                                            ComponentMarket(today, spread, dense(0.025), dense(0.03)));
    assertDensePassivesLeaveTargetUntouched("reference", ComponentMarket(today, FlatCurve(maturity, 0.04), reference, FlatCurve(maturity, 0.03)),
                                            ComponentMarket(today, dense(0.04), reference, dense(0.03)));
    assertDensePassivesLeaveTargetUntouched("discount",
                                            ComponentMarket(today, FlatCurve(maturity, 0.04), FlatCurve(maturity, 0.025), FlatCurve(maturity, 0.03)),
                                            ComponentMarket(today, dense(0.04), dense(0.025), FlatCurve(maturity, 0.03)));

    const auto discountSensitivity =
        Dal::RateTradeNodeSensitivities(trade, ComponentMarket(today, dense(0.04), dense(0.025), dense(0.03)), "discount");
    ASSERT_TRUE(discountSensitivity.eligible_);
    ASSERT_EQ(static_cast<int>(discountSensitivity.gradient_.size()), static_cast<int>(denseKnots.size()));
}

TEST(RateCashflowPricingTest, TestBasisNodeAADPassiveCurvesStayOutOfTheActiveGradient) {
    // Both forecast keys address the same curve object: in each call the leg reading the active key
    // runs on the registered parameters and the other leg on the passive double copy, so the two
    // forecast gradients partition the shared curve's total derivative instead of double-counting
    // it.
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 10, 15);
    const Dal::Date_ maturity(2029, 1, 15);
    const Dal::Vector_<Dal::Date_> knots{Dal::Date_(2027, 1, 15), Dal::Date_(2028, 1, 15), Dal::Date_(2028, 10, 15)};
    const Dal::Vector_<> sharedParameters{0.02, 0.018, 0.022};
    const auto buildShared = [&](const Dal::Vector_<>& parameters) {
        return Dal::Handle_<Dal::DiscountCurve_>(Dal::NewDiscountPWC("shared", "USD", Dal::PiecewiseConstant_(knots, parameters)));
    };
    const auto assemble = [&](const Dal::Handle_<Dal::DiscountCurve_>& forecast, const Dal::Handle_<Dal::DiscountCurve_>& discount) {
        return ComponentMarket(today, forecast, forecast, discount);
    };
    const auto trade = Trade(Dal::RateInstrumentType_("BASIS_SWAP"), today, start, maturity, BasisTerms());
    const auto market = assemble(buildShared(sharedParameters), FlatCurve(maturity, 0.03));
    const auto spreadSensitivity = Dal::RateTradeNodeSensitivities(trade, market, "forecast");
    const auto referenceSensitivity = Dal::RateTradeNodeSensitivities(trade, market, "reference");
    const auto discountSensitivity = Dal::RateTradeNodeSensitivities(trade, market, "discount");
    ASSERT_TRUE(spreadSensitivity.eligible_);
    ASSERT_TRUE(referenceSensitivity.eligible_);
    ASSERT_TRUE(discountSensitivity.eligible_);
    ASSERT_EQ(spreadSensitivity.gradient_.size(), 3);
    ASSERT_EQ(referenceSensitivity.gradient_.size(), 3);
    for (int column = 0; column < 3; ++column) {
        ASSERT_NE(spreadSensitivity.gradient_[column], 0.0) << "spread column " << column;
        ASSERT_NE(referenceSensitivity.gradient_[column], 0.0) << "reference column " << column;
    }

    constexpr double epsilon = 1.0e-6;
    for (int column = 0; column < 3; ++column) {
        auto plusParameters = sharedParameters;
        auto minusParameters = sharedParameters;
        plusParameters[column] += epsilon;
        minusParameters[column] -= epsilon;
        const auto plus = Dal::PriceRateTrade(trade, assemble(buildShared(plusParameters), FlatCurve(maturity, 0.03)));
        const auto minus = Dal::PriceRateTrade(trade, assemble(buildShared(minusParameters), FlatCurve(maturity, 0.03)));
        ASSERT_TRUE(plus.succeeded_);
        ASSERT_TRUE(minus.succeeded_);
        const double total = (plus.pv_ - minus.pv_) / (2.0 * epsilon);
        const double partitioned = spreadSensitivity.gradient_[column] + referenceSensitivity.gradient_[column];
        const double tolerance = 1.0e-6 + 1.0e-7 * std::max(std::abs(partitioned), std::abs(total));
        ASSERT_NEAR(partitioned, total, tolerance) << "shared forecast column " << column;
    }

    const auto discountPlus = Dal::PriceRateTrade(trade, assemble(buildShared(sharedParameters), FlatCurve(maturity, 0.03 + epsilon)));
    const auto discountMinus = Dal::PriceRateTrade(trade, assemble(buildShared(sharedParameters), FlatCurve(maturity, 0.03 - epsilon)));
    ASSERT_TRUE(discountPlus.succeeded_);
    ASSERT_TRUE(discountMinus.succeeded_);
    const double totalDiscount = (discountPlus.pv_ - discountMinus.pv_) / (2.0 * epsilon);
    const double discountTolerance = 1.0e-6 + 1.0e-7 * std::max(std::abs(discountSensitivity.gradient_[0]), std::abs(totalDiscount));
    ASSERT_NEAR(discountSensitivity.gradient_[0], totalDiscount, discountTolerance);
}

TEST(RateCashflowPricingTest, TestBasisNodeAADFixingStatesProjectedSuppliedAndMissing) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 3, 16);
    const Dal::Date_ maturity(2026, 5, 15);
    const Dal::Vector_<Dal::Date_> knots{Dal::Date_(2026, 7, 15), Dal::Date_(2027, 1, 15), Dal::Date_(2028, 1, 15)};
    const auto spreadForecast =
        Dal::Handle_<Dal::DiscountCurve_>(Dal::NewDiscountPWC("spread", "USD", Dal::PiecewiseConstant_(knots, {0.01, 0.012, 0.015})));
    const auto referenceForecast =
        Dal::Handle_<Dal::DiscountCurve_>(Dal::NewDiscountPWC("reference", "USD", Dal::PiecewiseConstant_(knots, {0.02, 0.022, 0.025})));
    const auto discount = FlatCurve(maturity, 0.03);

    auto terms = BasisTerms();
    // The legs' fixing identities differ in name and publication hour; the ComponentMarket
    // valuation time (10:30) falls between the 09:00 and 11:00 fixings, isolating one leg's
    // fixing state from the other's.
    terms.spreadFixingIdentity_ = {"USD-LIBOR-3M", 9, 0};
    terms.referenceFixingIdentity_ = {"USD-SOFR", 11, 0};
    const auto trade = Trade(Dal::RateInstrumentType_("BASIS_SWAP"), today, start, maturity, terms);

    {
        // Before either fixing: both legs projected, both forecasts carry gradient on the live
        // column only.
        const auto market = ComponentMarket(today, spreadForecast, referenceForecast, discount);
        const auto spreadSensitivity = Dal::RateTradeNodeSensitivities(trade, market, "forecast");
        const auto referenceSensitivity = Dal::RateTradeNodeSensitivities(trade, market, "reference");
        ASSERT_TRUE(spreadSensitivity.eligible_);
        ASSERT_TRUE(referenceSensitivity.eligible_);
        ASSERT_NE(spreadSensitivity.gradient_[0], 0.0);
        ASSERT_DOUBLE_EQ(spreadSensitivity.gradient_[1], 0.0);
        ASSERT_DOUBLE_EQ(spreadSensitivity.gradient_[2], 0.0);
        ASSERT_NE(referenceSensitivity.gradient_[0], 0.0);
        ASSERT_DOUBLE_EQ(referenceSensitivity.gradient_[1], 0.0);
        ASSERT_DOUBLE_EQ(referenceSensitivity.gradient_[2], 0.0);
    }

    Dal::MarketFixingSnapshot_::values_t spreadHistory;
    spreadHistory["USD-LIBOR-3M"][Dal::DateTime_(start, 9, 0)] = 0.031;
    {
        // Spread leg fixed and supplied, reference leg still projected: the spread forecast
        // gradient is structurally zero while the reference forecast and discount stay live.
        auto supplied = ComponentMarket(start, spreadForecast, referenceForecast, discount);
        supplied.fixings_ = Dal::Handle_<Dal::MarketFixingSnapshot_>(new Dal::MarketFixingSnapshot_(spreadHistory));
        const auto passive = Dal::PriceRateTrade(trade, supplied);
        ASSERT_TRUE(passive.succeeded_);
        ASSERT_EQ(passive.requiredHistoricalFixings_.size(), 1);
        ASSERT_TRUE(passive.missingHistoricalFixings_.empty());

        const auto spreadSensitivity = Dal::RateTradeNodeSensitivities(trade, supplied, "forecast");
        const auto referenceSensitivity = Dal::RateTradeNodeSensitivities(trade, supplied, "reference");
        const auto discountSensitivity = Dal::RateTradeNodeSensitivities(trade, supplied, "discount");
        ASSERT_TRUE(spreadSensitivity.eligible_);
        ASSERT_TRUE(referenceSensitivity.eligible_);
        ASSERT_TRUE(discountSensitivity.eligible_);
        ASSERT_DOUBLE_EQ(spreadSensitivity.pv_, passive.pv_);
        ASSERT_DOUBLE_EQ(referenceSensitivity.pv_, passive.pv_);
        ASSERT_DOUBLE_EQ(discountSensitivity.pv_, passive.pv_);
        for (const double column : spreadSensitivity.gradient_)
            ASSERT_DOUBLE_EQ(column, 0.0);
        ASSERT_NE(referenceSensitivity.gradient_[0], 0.0);
        ASSERT_NE(discountSensitivity.gradient_[0], 0.0);
    }

    {
        // Mirror with the reference leg fixed and supplied: the fixing identities gate per leg.
        auto mirroredTerms = BasisTerms();
        mirroredTerms.spreadFixingIdentity_ = {"USD-LIBOR-3M", 11, 0};
        mirroredTerms.referenceFixingIdentity_ = {"USD-SOFR", 9, 0};
        const auto mirrored = Trade(Dal::RateInstrumentType_("BASIS_SWAP"), today, start, maturity, mirroredTerms);
        Dal::MarketFixingSnapshot_::values_t referenceHistory;
        referenceHistory["USD-SOFR"][Dal::DateTime_(start, 9, 0)] = 0.0205;
        auto supplied = ComponentMarket(start, spreadForecast, referenceForecast, discount);
        supplied.fixings_ = Dal::Handle_<Dal::MarketFixingSnapshot_>(new Dal::MarketFixingSnapshot_(referenceHistory));
        const auto passive = Dal::PriceRateTrade(mirrored, supplied);
        ASSERT_TRUE(passive.succeeded_);
        ASSERT_EQ(passive.requiredHistoricalFixings_.size(), 1);
        ASSERT_TRUE(passive.missingHistoricalFixings_.empty());

        const auto referenceSensitivity = Dal::RateTradeNodeSensitivities(mirrored, supplied, "reference");
        const auto spreadSensitivity = Dal::RateTradeNodeSensitivities(mirrored, supplied, "forecast");
        ASSERT_TRUE(referenceSensitivity.eligible_);
        ASSERT_TRUE(spreadSensitivity.eligible_);
        ASSERT_DOUBLE_EQ(referenceSensitivity.pv_, passive.pv_);
        for (const double column : referenceSensitivity.gradient_)
            ASSERT_DOUBLE_EQ(column, 0.0);
        ASSERT_NE(spreadSensitivity.gradient_[0], 0.0);
    }

    {
        // Missing spread fixing (reference leg still projected): passive pricing fails on exactly
        // the spread leg's identity and the node stage reports the validation token.
        const auto missing = ComponentMarket(start, spreadForecast, referenceForecast, discount);
        const auto passive = Dal::PriceRateTrade(trade, missing);
        ASSERT_FALSE(passive.succeeded_);
        ASSERT_EQ(passive.requiredHistoricalFixings_.size(), 1);
        ASSERT_EQ(passive.missingHistoricalFixings_.size(), 1);
        AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(trade, missing, "forecast"), "TRADE_VALIDATION_FAILED");
        AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(trade, missing, "discount"), "TRADE_VALIDATION_FAILED");
    }
}

TEST(RateCashflowPricingTest, TestBasisNodeAADBeforeAndAfterMaturityExpiry) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 2, 15);
    const Dal::Date_ maturity(2026, 4, 15);
    const auto expired = ComponentMarket(Dal::Date_(maturity.AddDays(1)), FlatCurve(maturity.AddDays(30), 0.04),
                                         FlatCurve(maturity.AddDays(30), 0.025), FlatCurve(maturity.AddDays(30), 0.03));
    const auto live = ComponentMarket(today, FlatCurve(maturity, 0.04), FlatCurve(maturity, 0.025), FlatCurve(maturity, 0.03));
    const auto trade = Trade(Dal::RateInstrumentType_("BASIS_SWAP"), today, start, maturity, BasisTerms());

    for (const auto& key : {"forecast", "reference", "discount"}) {
        const auto liveSensitivity = Dal::RateTradeNodeSensitivities(trade, live, key);
        ASSERT_TRUE(liveSensitivity.eligible_);
        ASSERT_NE(liveSensitivity.pv_, 0.0);
        ASSERT_NE(liveSensitivity.gradient_[0], 0.0);

        const auto settled = Dal::RateTradeNodeSensitivities(trade, expired, key);
        ASSERT_TRUE(settled.eligible_);
        ASSERT_DOUBLE_EQ(settled.pv_, 0.0);
        for (const double column : settled.gradient_)
            ASSERT_DOUBLE_EQ(column, 0.0);
    }
}

TEST(RateCashflowPricingTest, TestBasisNodeAADReasonPrecedenceForCombinedFailures) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 10, 15);
    const Dal::Date_ maturity(2029, 1, 15);
    const auto supported = ComponentMarket(today, FlatCurve(maturity, 0.04), FlatCurve(maturity, 0.025), FlatCurve(maturity, 0.03));
    auto invalidTerms = BasisTerms();
    invalidTerms.notional_ = std::numeric_limits<double>::quiet_NaN();
    const auto invalidTrade = Trade(Dal::RateInstrumentType_("BASIS_SWAP"), today, start, maturity, invalidTerms);
    const auto validTrade = Trade(Dal::RateInstrumentType_("BASIS_SWAP"), today, start, maturity, BasisTerms());

    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(invalidTrade, supported, "wrong"), "TRADE_DOES_NOT_DEPEND_ON_COMPONENT");
    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(validTrade, supported, "wrong"), "TRADE_DOES_NOT_DEPEND_ON_COMPONENT");

    auto missingReference = supported;
    missingReference.curveComponents_.erase("reference");
    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(validTrade, missingReference, "forecast"), "CURVE_COMPONENT_UNAVAILABLE");
    auto nullDiscount = supported;
    nullDiscount.curveComponents_["discount"] = Dal::Handle_<Dal::DiscountCurve_>();
    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(validTrade, nullDiscount, "forecast"), "CURVE_COMPONENT_UNAVAILABLE");

    auto unsupportedReference = supported;
    unsupportedReference.curveComponents_["reference"] = Dal::Handle_<Dal::DiscountCurve_>(std::make_shared<const UnsupportedDiscountCurve_>());
    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(validTrade, unsupportedReference, "forecast"), "CURVE_REPRESENTATION_NOT_AAD_ENABLED");
    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(invalidTrade, unsupportedReference, "forecast"), "CURVE_REPRESENTATION_NOT_AAD_ENABLED");
    auto unsupportedTarget = supported;
    unsupportedTarget.curveComponents_["forecast"] = Dal::Handle_<Dal::DiscountCurve_>(std::make_shared<const UnsupportedDiscountCurve_>());
    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(validTrade, unsupportedTarget, "forecast"), "CURVE_REPRESENTATION_NOT_AAD_ENABLED");

    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(invalidTrade, supported, "forecast"), "TRADE_VALIDATION_FAILED");
}

TEST(RateCashflowPricingTest, TestXccyNodeAADDomOisRawColumnsMatchCentralBumpsAllRepresentations) { AssertXccyNodeBatteries(XCCY_DOM_OIS); }

TEST(RateCashflowPricingTest, TestXccyNodeAADDomFwd3MRawColumnsMatchCentralBumpsAllRepresentations) { AssertXccyNodeBatteries(XCCY_DOM_FWD_3M); }

TEST(RateCashflowPricingTest, TestXccyNodeAADForOisRawColumnsMatchCentralBumpsAllRepresentations) { AssertXccyNodeBatteries(XCCY_FOR_OIS); }

TEST(RateCashflowPricingTest, TestXccyNodeAADForFwd3MRawColumnsMatchCentralBumpsAllRepresentations) { AssertXccyNodeBatteries(XCCY_FOR_FWD_3M); }

TEST(RateCashflowPricingTest, TestXccyNodeAADBasisRawColumnsMatchCentralBumpsAllRepresentations) { AssertXccyNodeBatteries(XCCY_BASIS); }

TEST(RateCashflowPricingTest, TestXccyNodeAADPlannerEmitsConsumedDependencyKeys) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 10, 15);
    const Dal::Date_ maturity(2029, 1, 15);
    const auto trade = Trade(Dal::RateInstrumentType_("XCCY"), today, start, maturity, Dal::RateTradeTerms_(XccyTerms()));
    const auto market = FlatXccyMarket(today);

    // Market-aware overload: "depends on" = the collateral/tenor-selected curves actually consumed
    // (domestic discount + forecast, foreign discount + forecast, basis), in deterministic walk order.
    const auto plan = Dal::BuildRateCashflowPlan(trade, market);
    ASSERT_EQ(plan.dependencyComponentKeys_,
              (Dal::Vector_<Dal::String_>({XCCY_DOM_OIS, XCCY_DOM_FWD_3M, XCCY_FOR_OIS, XCCY_FOR_FWD_3M, XCCY_BASIS})));

    // The (trade, valuationTime) signature and its XCCY behaviour stay unchanged: no keys without a market.
    ASSERT_TRUE(Dal::BuildRateCashflowPlan(trade, market.valuationTime_).dependencyComponentKeys_.empty());
    const auto priced = Dal::PriceRateTrade(trade, market);
    ASSERT_TRUE(priced.succeeded_);
    ASSERT_EQ(priced.dependencyComponentKeys_, plan.dependencyComponentKeys_);

    // Without projection the forecast routes to the discount slot: the walk dedupes by pointer, so
    // each block contributes exactly one curve.
    const auto noProjection = Trade(Dal::RateInstrumentType_("XCCY"), today, start, maturity, Dal::RateTradeTerms_(XccyTerms(true, false)));
    ASSERT_EQ(Dal::BuildRateCashflowPlan(noProjection, market).dependencyComponentKeys_,
              (Dal::Vector_<Dal::String_>({XCCY_DOM_OIS, XCCY_FOR_OIS, XCCY_BASIS})));
}

TEST(RateCashflowPricingTest, TestXccyNodeAADAllConsumedComponentsEligibleWithLiveGradients) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 10, 15);
    const Dal::Date_ maturity(2029, 1, 15);
    const auto trade = Trade(Dal::RateInstrumentType_("XCCY"), today, start, maturity, Dal::RateTradeTerms_(XccyTerms()));
    const auto market = FlatXccyMarket(today);
    const auto passive = Dal::PriceRateTrade(trade, market);
    ASSERT_TRUE(passive.succeeded_);

    for (const auto& key : {XCCY_DOM_OIS, XCCY_DOM_FWD_3M, XCCY_FOR_OIS, XCCY_FOR_FWD_3M, XCCY_BASIS}) {
        const auto sensitivity = Dal::RateTradeNodeSensitivities(trade, market, key);
        ASSERT_TRUE(sensitivity.eligible_) << key;
        ASSERT_EQ(static_cast<int>(sensitivity.gradient_.size()), 1) << key;
        ASSERT_DOUBLE_EQ(sensitivity.pv_, passive.pv_) << key;
        // Magnitude floor, not just non-zero: a live column must carry real sensitivity.
        ASSERT_GT(std::abs(sensitivity.gradient_[0]), 1.0e-8) << key;
    }
}

TEST(RateCashflowPricingTest, TestXccyNodeAADMultiCurrencyPairs) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 10, 15);
    const Dal::Date_ maturity(2029, 1, 15);

    {
        auto spec = FlatXccySpec(Dal::Date_(2031, 1, 15));
        spec.domestic_ = Dal::Ccy_("EUR");
        spec.foreign_ = Dal::Ccy_("JPY");
        spec.fxSpot_ = 160.0;
        spec.domesticOis_ = FlatCurve(Dal::Date_(2031, 1, 15), 0.025, "EUR");
        spec.domesticFwd3M_ = FlatCurve(Dal::Date_(2031, 1, 15), 0.027, "EUR");
        spec.foreignOis_ = FlatCurve(Dal::Date_(2031, 1, 15), 0.005, "JPY");
        spec.foreignFwd3M_ = FlatCurve(Dal::Date_(2031, 1, 15), 0.007, "JPY");
        spec.basis_ = FlatCurve(Dal::Date_(2031, 1, 15), 0.0008, "EUR");
        const auto market = BuildXccyMarket(today, spec);
        const auto trade = Trade(Dal::RateInstrumentType_("XCCY"), today, start, maturity,
                                 Dal::RateTradeTerms_(XccyTerms(Dal::Ccy_("EUR"), Dal::Ccy_("JPY"), true, true)));
        const auto passive = Dal::PriceRateTrade(trade, market);
        ASSERT_TRUE(passive.succeeded_);
        ASSERT_NE(passive.pv_, 0.0);
        for (const auto& key : {XCCY_DOM_OIS, XCCY_DOM_FWD_3M, XCCY_FOR_OIS, XCCY_FOR_FWD_3M, XCCY_BASIS}) {
            const auto sensitivity = Dal::RateTradeNodeSensitivities(trade, market, key);
            ASSERT_TRUE(sensitivity.eligible_) << key;
            ASSERT_DOUBLE_EQ(sensitivity.pv_, passive.pv_) << key;
            ASSERT_GT(std::abs(sensitivity.gradient_[0]), 1.0e-8) << key;
        }
    }

    {
        // USD/EUR with the reverse notional direction: PV and gradient flip sign together.
        const auto market = FlatXccyMarket(today);
        const auto receive = Trade(Dal::RateInstrumentType_("XCCY"), today, start, maturity, Dal::RateTradeTerms_(XccyTerms(true)));
        const auto pay = Trade(Dal::RateInstrumentType_("XCCY"), today, start, maturity, Dal::RateTradeTerms_(XccyTerms(false)));
        const auto receiveSensitivity = Dal::RateTradeNodeSensitivities(receive, market, XCCY_DOM_OIS);
        const auto paySensitivity = Dal::RateTradeNodeSensitivities(pay, market, XCCY_DOM_OIS);
        ASSERT_TRUE(receiveSensitivity.eligible_);
        ASSERT_TRUE(paySensitivity.eligible_);
        ASSERT_DOUBLE_EQ(paySensitivity.pv_, -receiveSensitivity.pv_);
        ASSERT_DOUBLE_EQ(paySensitivity.gradient_[0], -receiveSensitivity.gradient_[0]);
    }
}

TEST(RateCashflowPricingTest, TestXccyNodeAADCollateralRoutingSelectsConsumedDiscount) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 10, 15);
    const Dal::Date_ maturity(2029, 1, 15);
    const auto horizon = Dal::Date_(2031, 1, 15);
    auto spec = FlatXccySpec(horizon);
    spec.domesticGc_ = FlatCurve(horizon, 0.035, "USD");
    const auto market = BuildXccyMarket(today, spec);
    const auto trade = Trade(
        Dal::RateInstrumentType_("XCCY"), today, start, maturity,
        Dal::RateTradeTerms_(XccyTerms(Dal::Ccy_("USD"), Dal::Ccy_("EUR"), true, true, Dal::CollateralType_(Dal::CollateralType_::Value_::GC))));

    // The GC-collateral index selects the GC discount curve; the OIS discount curve stays in the
    // block but unconsumed, so it is not a dependency (unused-block-member semantics).
    ASSERT_EQ(Dal::BuildRateCashflowPlan(trade, market).dependencyComponentKeys_,
              (Dal::Vector_<Dal::String_>({"domGc", XCCY_DOM_FWD_3M, XCCY_FOR_OIS, XCCY_FOR_FWD_3M, XCCY_BASIS})));
    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(trade, market, XCCY_DOM_OIS), "TRADE_DOES_NOT_DEPEND_ON_COMPONENT");

    const auto passive = Dal::PriceRateTrade(trade, market);
    ASSERT_TRUE(passive.succeeded_);
    const auto sensitivity = Dal::RateTradeNodeSensitivities(trade, market, "domGc");
    ASSERT_TRUE(sensitivity.eligible_);
    ASSERT_DOUBLE_EQ(sensitivity.pv_, passive.pv_);
    ASSERT_GT(std::abs(sensitivity.gradient_[0]), 1.0e-8);
}

TEST(RateCashflowPricingTest, TestXccyNodeAADAddressingNegativeCases) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 10, 15);
    const Dal::Date_ maturity(2029, 1, 15);
    const auto horizon = Dal::Date_(2031, 1, 15);
    const auto trade = Trade(Dal::RateInstrumentType_("XCCY"), today, start, maturity, Dal::RateTradeTerms_(XccyTerms()));
    const auto supported = FlatXccyMarket(today);

    // Wrong key: never consumed, whatever the registry holds.
    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(trade, supported, "unknown"), "TRADE_DOES_NOT_DEPEND_ON_COMPONENT");

    {
        // Unused in-block member: a registered 6M forward the trade never reads must be a canonical
        // failure, never eligible-with-all-zeros.
        auto spec = FlatXccySpec(horizon);
        spec.domesticFwd6M_ = FlatCurve(horizon, 0.04, "USD");
        const auto market = BuildXccyMarket(today, spec);
        AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(trade, market, XCCY_DOM_FWD_6M), "TRADE_DOES_NOT_DEPEND_ON_COMPONENT");
        ASSERT_TRUE(Dal::RateTradeNodeSensitivities(trade, market, XCCY_DOM_OIS).eligible_);
        ASSERT_GT(std::abs(Dal::RateTradeNodeSensitivities(trade, market, XCCY_DOM_OIS).gradient_[0]), 1.0e-8);
    }

    {
        // Unregistered key: the consumed foreign forecast is dropped from the registry; the key no
        // longer addresses anything, and the remaining targets still sweep unchanged.
        auto unregistered = supported;
        unregistered.curveComponents_.erase(XCCY_FOR_FWD_3M);
        AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(trade, unregistered, XCCY_FOR_FWD_3M), "TRADE_DOES_NOT_DEPEND_ON_COMPONENT");
        const auto reference = Dal::RateTradeNodeSensitivities(trade, supported, XCCY_DOM_OIS);
        const auto unaffected = Dal::RateTradeNodeSensitivities(trade, unregistered, XCCY_DOM_OIS);
        ASSERT_TRUE(unaffected.eligible_);
        ASSERT_DOUBLE_EQ(unaffected.pv_, reference.pv_);
        ASSERT_EQ(unaffected.gradient_, reference.gradient_);
    }

    {
        // In-block unclassifiable representation, unused member: never classified, no effect.
        auto spec = FlatXccySpec(horizon);
        spec.domesticFwd6M_ = Dal::Handle_<Dal::DiscountCurve_>(std::make_shared<const UnsupportedDiscountCurve_>());
        const auto market = BuildXccyMarket(today, spec);
        const auto sensitivity = Dal::RateTradeNodeSensitivities(trade, market, XCCY_DOM_OIS);
        ASSERT_TRUE(sensitivity.eligible_);
        ASSERT_GT(std::abs(sensitivity.gradient_[0]), 1.0e-8);
    }

    {
        // In-block unclassifiable representation, addressed target: the representation token.
        auto spec = FlatXccySpec(horizon);
        spec.domesticOis_ = Dal::Handle_<Dal::DiscountCurve_>(std::make_shared<const UnsupportedDiscountCurve_>());
        const auto market = BuildXccyMarket(today, spec);
        AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(trade, market, XCCY_DOM_OIS), "CURVE_REPRESENTATION_NOT_AAD_ENABLED");
    }

    {
        // In-block unclassifiable representation, consumed non-target: passive pricing succeeds on
        // its constant DFs, but the active assembly cannot rebuild it — an evaluation failure, not
        // the representation token (that token stays a failure of the addressed component).
        auto spec = FlatXccySpec(horizon);
        spec.foreignOis_ = Dal::Handle_<Dal::DiscountCurve_>(std::make_shared<const UnsupportedDiscountCurve_>("EUR"));
        const auto market = BuildXccyMarket(today, spec);
        const auto passive = Dal::PriceRateTrade(trade, market);
        ASSERT_TRUE(passive.succeeded_);
        AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(trade, market, XCCY_DOM_OIS), "AAD_EVALUATION_FAILED");
    }
}

TEST(RateCashflowPricingTest, TestXccyNodeAADBeforeAndAfterMaturityExpiry) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 2, 15);
    const Dal::Date_ maturity(2026, 4, 15);
    const auto trade = Trade(Dal::RateInstrumentType_("XCCY"), today, start, maturity, Dal::RateTradeTerms_(XccyTerms()));
    const auto horizon = Dal::Date_(2031, 1, 15);

    auto expired = FlatXccySpec(horizon);
    const auto expiredMarket = BuildXccyMarket(maturity.AddDays(1), expired);
    const auto liveMarket = BuildXccyMarket(today, FlatXccySpec(horizon));

    for (const auto& key : {XCCY_DOM_OIS, XCCY_DOM_FWD_3M, XCCY_FOR_OIS, XCCY_FOR_FWD_3M, XCCY_BASIS}) {
        const auto live = Dal::RateTradeNodeSensitivities(trade, liveMarket, key);
        ASSERT_TRUE(live.eligible_) << key;
        ASSERT_NE(live.pv_, 0.0) << key;
        ASSERT_GT(std::abs(live.gradient_[0]), 1.0e-8) << key;

        const auto settled = Dal::RateTradeNodeSensitivities(trade, expiredMarket, key);
        ASSERT_TRUE(settled.eligible_) << key;
        ASSERT_DOUBLE_EQ(settled.pv_, 0.0) << key;
        for (const double column : settled.gradient_)
            ASSERT_DOUBLE_EQ(column, 0.0) << key;
    }
}

TEST(RateCashflowPricingTest, TestXccyNodeAADFixingStatesProjectedSuppliedAndMissing) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 2, 15);
    const Dal::Date_ maturity(2026, 4, 15);
    const auto horizon = Dal::Date_(2031, 1, 15);
    const auto trade = Trade(Dal::RateInstrumentType_("XCCY"), today, start, maturity, Dal::RateTradeTerms_(XccyTerms()));
    const auto terms = XccyTerms();
    const Dal::String_ domesticIndex = terms.config_.domesticRateFixing_.indexName_;
    const Dal::String_ foreignIndex = terms.config_.foreignRateFixing_.indexName_;
    // Fixing keys come from the plan itself: the schedule business-day adjustment moves the accrual
    // start, so hand-built DateTime_ keys would silently miss.
    const auto plan = Dal::BuildXccyCashflowPlan(start, maturity, terms.config_);
    const Dal::DateTime_ domesticFixingTime = plan.domesticPeriods_.front().rateFixingTime_;
    const Dal::DateTime_ foreignFixingTime = plan.foreignPeriods_.front().rateFixingTime_;
    const Dal::Date_ valuationDate = std::max(domesticFixingTime.Date(), foreignFixingTime.Date()).AddDays(1);

    {
        // Both legs projected before either fixing.
        const auto market = BuildXccyMarket(today, FlatXccySpec(horizon));
        for (const auto& key : {XCCY_DOM_FWD_3M, XCCY_FOR_FWD_3M, XCCY_DOM_OIS, XCCY_BASIS}) {
            const auto sensitivity = Dal::RateTradeNodeSensitivities(trade, market, key);
            ASSERT_TRUE(sensitivity.eligible_) << key;
            ASSERT_GT(std::abs(sensitivity.gradient_[0]), 1.0e-8) << key;
        }
    }

    const auto withFixings = [&](const Dal::MarketFixingSnapshot_::values_t& values) {
        auto spec = FlatXccySpec(horizon);
        spec.fixings_ = Dal::Handle_<Dal::MarketFixingSnapshot_>(new Dal::MarketFixingSnapshot_(values));
        return BuildXccyMarket(valuationDate, spec);
    };
    Dal::MarketFixingSnapshot_::values_t domesticOnly;
    domesticOnly[domesticIndex][domesticFixingTime] = 0.031;
    Dal::MarketFixingSnapshot_::values_t foreignOnly;
    foreignOnly[foreignIndex][foreignFixingTime] = 0.021;
    Dal::MarketFixingSnapshot_::values_t both = domesticOnly;
    both[foreignIndex][foreignFixingTime] = 0.021;

    {
        // Missing fixing in the foreign currency: passive pricing records both requests, fails on
        // exactly the foreign one, and the node stage answers with the validation token.
        const auto market = withFixings(domesticOnly);
        const auto passive = Dal::PriceRateTrade(trade, market);
        ASSERT_FALSE(passive.succeeded_);
        ASSERT_EQ(passive.requiredHistoricalFixings_.size(), 2);
        ASSERT_EQ(passive.missingHistoricalFixings_.size(), 1);
        ASSERT_EQ(passive.missingHistoricalFixings_[0].indexName_, foreignIndex);
        AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(trade, market, XCCY_DOM_OIS), "TRADE_VALIDATION_FAILED");
    }
    {
        // Mirror: missing fixing in the domestic currency.
        const auto market = withFixings(foreignOnly);
        const auto passive = Dal::PriceRateTrade(trade, market);
        ASSERT_FALSE(passive.succeeded_);
        ASSERT_EQ(passive.missingHistoricalFixings_.size(), 1);
        ASSERT_EQ(passive.missingHistoricalFixings_[0].indexName_, domesticIndex);
        AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(trade, market, XCCY_FOR_OIS), "TRADE_VALIDATION_FAILED");
    }
    {
        // Both fixings missing: accounting is fail-fast on the first missing request (name order),
        // so exactly one entry is recorded before the validation throw.
        const auto market = withFixings({});
        const auto passive = Dal::PriceRateTrade(trade, market);
        ASSERT_FALSE(passive.succeeded_);
        ASSERT_EQ(passive.requiredHistoricalFixings_.size(), 2);
        ASSERT_EQ(passive.missingHistoricalFixings_.size(), 1);
        AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(trade, market, XCCY_DOM_OIS), "TRADE_VALIDATION_FAILED");
    }
    {
        // Both supplied: the whole float legs are constants, so both forecast gradients are
        // structurally zero while discounting and the basis stay live.
        const auto market = withFixings(both);
        const auto passive = Dal::PriceRateTrade(trade, market);
        ASSERT_TRUE(passive.succeeded_);
        ASSERT_EQ(passive.requiredHistoricalFixings_.size(), 2);
        ASSERT_TRUE(passive.missingHistoricalFixings_.empty());
        for (const auto& key : {XCCY_DOM_FWD_3M, XCCY_FOR_FWD_3M}) {
            const auto sensitivity = Dal::RateTradeNodeSensitivities(trade, market, key);
            ASSERT_TRUE(sensitivity.eligible_) << key;
            ASSERT_DOUBLE_EQ(sensitivity.pv_, passive.pv_) << key;
            for (const double column : sensitivity.gradient_)
                ASSERT_DOUBLE_EQ(column, 0.0) << key;
        }
        for (const auto& key : {XCCY_DOM_OIS, XCCY_FOR_OIS, XCCY_BASIS}) {
            const auto sensitivity = Dal::RateTradeNodeSensitivities(trade, market, key);
            ASSERT_TRUE(sensitivity.eligible_) << key;
            ASSERT_GT(std::abs(sensitivity.gradient_[0]), 1.0e-8) << key;
        }
    }
}

TEST(RateCashflowPricingTest, TestXccyNodeAADTriPartiteIsolation) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 10, 15);
    const Dal::Date_ maturity(2029, 1, 15);
    // The last knot sits inside the accrual span so every column is live.
    const Dal::Vector_<Dal::Date_> knots{Dal::Date_(2027, 1, 15), Dal::Date_(2028, 1, 15), Dal::Date_(2028, 10, 15)};
    const auto shaped = [&](double forward, const Dal::String_& ccy) {
        return Dal::Handle_<Dal::DiscountCurve_>(Dal::NewDiscountPWC("shaped", ccy, Dal::PiecewiseConstant_(knots, {forward, forward, forward})));
    };
    const auto noCurve = Dal::Handle_<Dal::DiscountCurve_>();
    const auto assemble = [&](const Dal::Handle_<Dal::DiscountCurve_>& denseForeignOis, const Dal::Handle_<Dal::DiscountCurve_>& denseDomesticFwd) {
        auto spec = FlatXccySpec(Dal::Date_(2031, 1, 15));
        spec.domesticOis_ = shaped(0.03, "USD");
        spec.domesticFwd3M_ = denseDomesticFwd ? denseDomesticFwd : shaped(0.032, "USD");
        spec.foreignOis_ = denseForeignOis ? denseForeignOis : shaped(0.02, "EUR");
        spec.foreignFwd3M_ = shaped(0.022, "EUR");
        spec.basis_ = shaped(0.001, "USD");
        return BuildXccyMarket(today, spec);
    };
    const auto trade = Trade(Dal::RateInstrumentType_("XCCY"), today, start, maturity, Dal::RateTradeTerms_(XccyTerms()));

    {
        // Each of the five consumed components reports exactly its own three columns — all live.
        const auto market = assemble(noCurve, noCurve);
        const auto passive = Dal::PriceRateTrade(trade, market);
        ASSERT_TRUE(passive.succeeded_);
        for (const auto& key : {XCCY_DOM_OIS, XCCY_DOM_FWD_3M, XCCY_FOR_OIS, XCCY_FOR_FWD_3M, XCCY_BASIS}) {
            const auto sensitivity = Dal::RateTradeNodeSensitivities(trade, market, key);
            ASSERT_TRUE(sensitivity.eligible_) << key;
            ASSERT_EQ(static_cast<int>(sensitivity.gradient_.size()), 3) << key;
            ASSERT_DOUBLE_EQ(sensitivity.pv_, passive.pv_) << key;
            for (int column = 0; column < 3; ++column)
                ASSERT_GT(std::abs(sensitivity.gradient_[column]), 1.0e-8) << key << " column " << column;
        }
    }

    // Dense non-target curves with the same flat forwards: the constant-typed rebuilds contribute
    // their values only, so the target's PV and gradient do not scale with the passive node count.
    Dal::Vector_<Dal::Date_> denseKnots;
    for (Dal::Date_ knot = Dal::Date_(2026, 2, 15); knot <= Dal::Date_(2029, 12, 15); knot = knot.AddDays(30))
        denseKnots.push_back(knot);
    const auto dense = [&](double forward, const Dal::String_& ccy) {
        return Dal::Handle_<Dal::DiscountCurve_>(
            Dal::NewDiscountPWC("dense", ccy, Dal::PiecewiseConstant_(denseKnots, Dal::Vector_<>(denseKnots.size(), forward))));
    };
    const auto assertDenseNonTargetLeavesTargetUntouched = [&](const Dal::String_& key, const auto& flat, const auto& withDense) {
        const auto small = Dal::RateTradeNodeSensitivities(trade, flat, key);
        const auto large = Dal::RateTradeNodeSensitivities(trade, withDense, key);
        ASSERT_TRUE(small.eligible_);
        ASSERT_TRUE(large.eligible_);
        ASSERT_EQ(small.gradient_.size(), large.gradient_.size());
        ASSERT_NEAR(large.pv_, small.pv_, 1.0e-10 * std::max(1.0, std::abs(small.pv_)));
        for (int column = 0; column < static_cast<int>(small.gradient_.size()); ++column)
            ASSERT_NEAR(large.gradient_[column], small.gradient_[column], 1.0e-10 * std::max(1.0, std::abs(small.gradient_[column])))
                << "column " << column;
    };
    assertDenseNonTargetLeavesTargetUntouched(XCCY_DOM_OIS, assemble(noCurve, noCurve), assemble(dense(0.02, "EUR"), noCurve));
    assertDenseNonTargetLeavesTargetUntouched(XCCY_FOR_OIS, assemble(noCurve, noCurve), assemble(noCurve, dense(0.032, "USD")));
}

#if !defined(DAL_USE_XAD_AAD) && !defined(DAL_USE_CODIPACK_AAD) && !defined(DAL_USE_ADEPT_AAD)
TEST(RateCashflowPricingTest, TestXccyNodeAADTapeSizeBoundedByPeriodsTimesKnots) {
    // Frozen P0 contract 8 size bound: the kernel resolves one fixing per coupon period, so the
    // recording scales with periods (and a per-knot curve-construction constant), never with the
    // calendar-day count a daily-compounding loop would add. Measured on the native tape.
    using Dal::AAD::Number_;
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Vector_<Dal::Date_> knots{Dal::Date_(2026, 7, 15), Dal::Date_(2027, 7, 15), Dal::Date_(2028, 7, 15), Dal::Date_(2029, 7, 15)};

    const auto flatForward = [&](const Dal::String_& ccy, double forward) {
        const auto definition =
            Dal::MakeCurveDefinition("flat", ccy, Dal::CurveParameterization_(Dal::CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD),
                                     Dal::LogDfScheme_::Value_::LOG_LINEAR, knots, today, Dal::DayBasis_("ACT_365F"));
        Dal::Vector_<Number_> parameters(knots.size());
        for (int i = 0; i < static_cast<int>(knots.size()); ++i)
            parameters[i] = Number_(forward);
        return Dal::BuildDiscountCurveUniqueT<Number_>(definition, parameters);
    };

    Dal::RateLegConvention_ quarterlyLeg;
    quarterlyLeg.paymentFrequency_ = Dal::PeriodLength_("3M");
    quarterlyLeg.dayBasis_ = Dal::DayBasis_("ACT_365F");
    const auto configWithLeg = [&](const Dal::RateLegConvention_& leg) {
        auto config = XccyTerms(true, true).config_;
        config.convention_.domesticLeg_ = leg;
        config.convention_.foreignLeg_ = leg;
        return config;
    };

    // Mirrors the stage order: recording starts, then every consumed curve is rebuilt (per-knot
    // constant) and the contract priced.
    const auto measure = [&](const Dal::Date_& start, const Dal::Date_& maturity, const Dal::RateLegConvention_& leg) {
        auto* tape = Dal::AAD::Tape();
        Dal::AAD::Rewind(*tape);
        Dal::AAD::NewRecording(*tape);
        auto domesticOis = flatForward("USD", 0.03);
        auto domesticFwd = flatForward("USD", 0.032);
        auto foreignOis = flatForward("EUR", 0.02);
        auto foreignFwd = flatForward("EUR", 0.022);
        auto basis = flatForward("USD", 0.001);
        Dal::Tape::JointCurveBlock_<Number_> domestic, foreign;
        domestic.discountCurves[Dal::CollateralType_(Dal::CollateralType_::Value_::OIS)] = domesticOis.get();
        domestic.forwardCurves[Dal::PeriodLength_("3M")] = domesticFwd.get();
        foreign.discountCurves[Dal::CollateralType_(Dal::CollateralType_::Value_::OIS)] = foreignOis.get();
        foreign.forwardCurves[Dal::PeriodLength_("3M")] = foreignFwd.get();
        Dal::XccyMarketView_<Number_> view;
        view.valuationTime_ = Dal::DateTime_(today, 10, 30);
        view.pair_ = Dal::CurrencyPair_(Dal::Ccy_("USD"), Dal::Ccy_("EUR"));
        view.collateralCurrency_ = Dal::Ccy_("USD");
        view.fxSpot_ = Number_(1.2);
        view.domestic_ = &domestic;
        view.foreign_ = &foreign;
        view.basis_ = basis.get();
        const Dal::MarketFixingSnapshot_ empty;
        const Number_ pv = Dal::PriceXccyContract(Dal::BuildXccyCashflowPlan(start, maturity, configWithLeg(leg)), view, empty, 0.001, true, true);
        (void)Dal::AAD::Value(pv);
        const int size = tape->nodes_.Size();
        Dal::AAD::Rewind(*tape);
        return size;
    };

    // Eight quarterly periods vs sixteen: linear in periods up to the fixed curve-construction cost.
    const int sizeQuarterly = measure(Dal::Date_(2026, 4, 15), Dal::Date_(2028, 4, 15), quarterlyLeg);
    const int sizeQuarterlyDoubled = measure(Dal::Date_(2026, 4, 15), Dal::Date_(2030, 4, 15), quarterlyLeg);
    ASSERT_GT(sizeQuarterly, 0);
    ASSERT_NEAR(sizeQuarterlyDoubled, 2 * sizeQuarterly, 0.25 * sizeQuarterly);

    // Eight annual periods vs eight quarterly periods: the same period count over a four-times
    // longer calendar span records a comparable amount — no per-day amplification.
    const int sizeAnnual = measure(Dal::Date_(2026, 4, 15), Dal::Date_(2034, 4, 15), AnnualLeg());
    ASSERT_GT(sizeAnnual, 0);
    ASSERT_LE(sizeAnnual, 1.5 * sizeQuarterly);
}
#endif

TEST(RateCashflowPricingTest, TestFraAndFutureNodeAADActivePvEqualsPassiveBitwiseBothSettlementModes) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 4, 15);
    const Dal::Date_ maturity(2026, 7, 15);
    const auto market = ComponentMarket(today, FlatCurve(maturity, 0.04), FlatCurve(maturity, 0.03));

    for (const bool settleAtStart : {true, false}) {
        for (const bool receiveFloating : {true, false}) {
            auto terms = FraTerms(settleAtStart);
            terms.receiveFloating_ = receiveFloating;
            const auto trade = Trade(Dal::RateInstrumentType_("FRA"), today, start, maturity, terms);
            const auto passive = Dal::PriceRateTrade(trade, market);
            ASSERT_TRUE(passive.succeeded_);
            const auto forecastSensitivity = Dal::RateTradeNodeSensitivities(trade, market, "forecast");
            const auto discountSensitivity = Dal::RateTradeNodeSensitivities(trade, market, "discount");
            ASSERT_TRUE(forecastSensitivity.eligible_);
            ASSERT_TRUE(discountSensitivity.eligible_);
            ASSERT_DOUBLE_EQ(forecastSensitivity.pv_, passive.pv_);
            ASSERT_DOUBLE_EQ(discountSensitivity.pv_, passive.pv_);
        }
    }

    for (const bool isLong : {true, false}) {
        auto terms = FutureTerms();
        terms.long_ = isLong;
        const auto trade = Trade(Dal::RateInstrumentType_("FUTURE"), today, start, maturity, terms);
        const auto passive = Dal::PriceRateTrade(trade, market);
        ASSERT_TRUE(passive.succeeded_);
        const auto sensitivity = Dal::RateTradeNodeSensitivities(trade, market, "forecast");
        ASSERT_TRUE(sensitivity.eligible_);
        ASSERT_DOUBLE_EQ(sensitivity.pv_, passive.pv_);
    }
}

TEST(RateCashflowPricingTest, TestFraNodeAADFixingStatesProjectedSuppliedAndMissing) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 4, 15);
    const Dal::Date_ maturity(2026, 7, 15);
    auto terms = FraTerms(false);
    const auto trade = Trade(Dal::RateInstrumentType_("FRA"), today, start, maturity, terms);

    {
        const auto market = ComponentMarket(today, FlatCurve(maturity, 0.04), FlatCurve(maturity, 0.03));
        const auto projected = Dal::RateTradeNodeSensitivities(trade, market, "forecast");
        ASSERT_TRUE(projected.eligible_);
        ASSERT_NE(projected.gradient_[0], 0.0);
        const auto discounted = Dal::RateTradeNodeSensitivities(trade, market, "discount");
        ASSERT_TRUE(discounted.eligible_);
        ASSERT_NE(discounted.gradient_[0], 0.0);
    }

    Dal::MarketFixingSnapshot_::values_t history;
    history["USD-LIBOR-3M"][Dal::DateTime_(start, 11, 0)] = 0.0415;
    auto supplied = ComponentMarket(Dal::Date_(start.AddDays(1)), FlatCurve(maturity, 0.04), FlatCurve(maturity, 0.03));
    supplied.valuationTime_ = Dal::DateTime_(start.AddDays(1), 10, 30);
    supplied.fixings_ = Dal::Handle_<Dal::MarketFixingSnapshot_>(new Dal::MarketFixingSnapshot_(history));
    {
        const auto passive = Dal::PriceRateTrade(trade, supplied);
        ASSERT_TRUE(passive.succeeded_);
        ASSERT_EQ(passive.requiredHistoricalFixings_.size(), 1);
        ASSERT_TRUE(passive.missingHistoricalFixings_.empty());

        const auto forecastSensitivity = Dal::RateTradeNodeSensitivities(trade, supplied, "forecast");
        ASSERT_TRUE(forecastSensitivity.eligible_);
        ASSERT_EQ(forecastSensitivity.gradient_.size(), 1);
        ASSERT_DOUBLE_EQ(forecastSensitivity.pv_, passive.pv_);
        for (const double column : forecastSensitivity.gradient_)
            ASSERT_DOUBLE_EQ(column, 0.0);

        const auto discountSensitivity = Dal::RateTradeNodeSensitivities(trade, supplied, "discount");
        ASSERT_TRUE(discountSensitivity.eligible_);
        ASSERT_DOUBLE_EQ(discountSensitivity.pv_, passive.pv_);
        ASSERT_NE(discountSensitivity.gradient_[0], 0.0);
    }

    auto missing = ComponentMarket(Dal::Date_(start.AddDays(1)), FlatCurve(maturity, 0.04), FlatCurve(maturity, 0.03));
    missing.valuationTime_ = Dal::DateTime_(start.AddDays(1), 10, 30);
    {
        const auto passive = Dal::PriceRateTrade(trade, missing);
        ASSERT_FALSE(passive.succeeded_);
        ASSERT_EQ(passive.missingHistoricalFixings_.size(), 1);
        Dal::RateTradeNodeSensitivityResult_ sensitivity;
        ASSERT_NO_THROW(sensitivity = Dal::RateTradeNodeSensitivities(trade, missing, "forecast"));
        AssertCanonicalFailure(sensitivity, "TRADE_VALIDATION_FAILED");
    }
}

TEST(RateCashflowPricingTest, TestFutureNodeAADFixingStatesProjectedSuppliedAndMissing) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 4, 15);
    const Dal::Date_ maturity(2026, 7, 15);
    const auto trade = Trade(Dal::RateInstrumentType_("FUTURE"), today, start, maturity, FutureTerms());

    {
        const auto market = ComponentMarket(today, FlatCurve(maturity, 0.04), FlatCurve(maturity, 0.03));
        const auto projected = Dal::RateTradeNodeSensitivities(trade, market, "forecast");
        ASSERT_TRUE(projected.eligible_);
        ASSERT_NE(projected.gradient_[0], 0.0);
    }

    Dal::MarketFixingSnapshot_::values_t history;
    history["USD-LIBOR-3M"][Dal::DateTime_(start, 11, 0)] = 0.0415;
    auto supplied = ComponentMarket(Dal::Date_(start.AddDays(1)), FlatCurve(maturity, 0.04), FlatCurve(maturity, 0.03));
    supplied.valuationTime_ = Dal::DateTime_(start.AddDays(1), 10, 30);
    supplied.fixings_ = Dal::Handle_<Dal::MarketFixingSnapshot_>(new Dal::MarketFixingSnapshot_(history));
    {
        const auto passive = Dal::PriceRateTrade(trade, supplied);
        ASSERT_TRUE(passive.succeeded_);
        const auto sensitivity = Dal::RateTradeNodeSensitivities(trade, supplied, "forecast");
        ASSERT_TRUE(sensitivity.eligible_);
        ASSERT_DOUBLE_EQ(sensitivity.pv_, passive.pv_);
        for (const double column : sensitivity.gradient_)
            ASSERT_DOUBLE_EQ(column, 0.0);
    }

    auto missing = ComponentMarket(Dal::Date_(start.AddDays(1)), FlatCurve(maturity, 0.04), FlatCurve(maturity, 0.03));
    missing.valuationTime_ = Dal::DateTime_(start.AddDays(1), 10, 30);
    {
        const auto passive = Dal::PriceRateTrade(trade, missing);
        ASSERT_FALSE(passive.succeeded_);
        ASSERT_EQ(passive.missingHistoricalFixings_.size(), 1);
        AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(trade, missing, "forecast"), "TRADE_VALIDATION_FAILED");
    }
}

TEST(RateCashflowPricingTest, TestFraAndFutureNodeAADBeforeAndAfterMaturityExpiry) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 4, 15);
    const Dal::Date_ maturity(2026, 7, 15);
    auto expired = ComponentMarket(Dal::Date_(maturity.AddDays(1)), FlatCurve(maturity.AddDays(30), 0.04), FlatCurve(maturity.AddDays(30), 0.03));
    expired.valuationTime_ = Dal::DateTime_(maturity.AddDays(1), 10, 30);

    for (const bool settleAtStart : {true, false}) {
        const auto trade = Trade(Dal::RateInstrumentType_("FRA"), today, start, maturity, FraTerms(settleAtStart));
        const auto live =
            Dal::RateTradeNodeSensitivities(trade, ComponentMarket(today, FlatCurve(maturity, 0.04), FlatCurve(maturity, 0.03)), "forecast");
        ASSERT_TRUE(live.eligible_);
        ASSERT_NE(live.pv_, 0.0);
        ASSERT_NE(live.gradient_[0], 0.0);

        const auto settled = Dal::RateTradeNodeSensitivities(trade, expired, "forecast");
        ASSERT_TRUE(settled.eligible_);
        ASSERT_DOUBLE_EQ(settled.pv_, 0.0);
        for (const double column : settled.gradient_)
            ASSERT_DOUBLE_EQ(column, 0.0);
        const auto settledDiscount = Dal::RateTradeNodeSensitivities(trade, expired, "discount");
        ASSERT_TRUE(settledDiscount.eligible_);
        ASSERT_DOUBLE_EQ(settledDiscount.pv_, 0.0);
        for (const double column : settledDiscount.gradient_)
            ASSERT_DOUBLE_EQ(column, 0.0);
    }

    const auto futureTrade = Trade(Dal::RateInstrumentType_("FUTURE"), today, start, maturity, FutureTerms());
    const auto futureSettled = Dal::RateTradeNodeSensitivities(futureTrade, expired, "forecast");
    ASSERT_TRUE(futureSettled.eligible_);
    ASSERT_DOUBLE_EQ(futureSettled.pv_, 0.0);
    for (const double column : futureSettled.gradient_)
        ASSERT_DOUBLE_EQ(column, 0.0);
}

TEST(RateCashflowPricingTest, TestFraNodeAADIsolationFromPassiveNodeCount) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 10, 15);
    const Dal::Date_ maturity(2029, 1, 15);
    const auto trade = Trade(Dal::RateInstrumentType_("FRA"), today, start, maturity, FraTerms(false));
    const Dal::Vector_<Dal::Date_> forecastKnots{Dal::Date_(2026, 7, 15), Dal::Date_(2027, 1, 15), Dal::Date_(2028, 1, 15)};
    const auto forecast =
        Dal::Handle_<Dal::DiscountCurve_>(Dal::NewDiscountPWC("pwc", "USD", Dal::PiecewiseConstant_(forecastKnots, {0.01, -0.005, 0.03})));

    const auto small = Dal::RateTradeNodeSensitivities(trade, ComponentMarket(today, forecast, FlatCurve(maturity, 0.03)), "forecast");
    Dal::Vector_<Dal::Date_> denseKnots;
    for (Dal::Date_ knot = Dal::Date_(2026, 2, 15); knot <= Dal::Date_(2029, 12, 15); knot = knot.AddDays(30))
        denseKnots.push_back(knot);
    const Dal::Vector_<> denseForward(denseKnots.size(), 0.03);
    const auto denseDiscount =
        Dal::Handle_<Dal::DiscountCurve_>(Dal::NewDiscountPWC("dense", "USD", Dal::PiecewiseConstant_(denseKnots, denseForward)));
    const auto large = Dal::RateTradeNodeSensitivities(trade, ComponentMarket(today, forecast, denseDiscount), "forecast");

    ASSERT_TRUE(small.eligible_);
    ASSERT_TRUE(large.eligible_);
    ASSERT_EQ(small.gradient_.size(), 3);
    ASSERT_EQ(large.gradient_.size(), 3);
    ASSERT_NEAR(large.pv_, small.pv_, 1.0e-10 * std::max(1.0, std::abs(small.pv_)));
    for (int column = 0; column < 3; ++column)
        ASSERT_NEAR(large.gradient_[column], small.gradient_[column], 1.0e-10 * std::max(1.0, std::abs(small.gradient_[column])))
            << "column " << column;

    const auto discountSensitivity = Dal::RateTradeNodeSensitivities(trade, ComponentMarket(today, forecast, denseDiscount), "discount");
    ASSERT_TRUE(discountSensitivity.eligible_);
    ASSERT_EQ(static_cast<int>(discountSensitivity.gradient_.size()), static_cast<int>(denseKnots.size()));
}

TEST(RateCashflowPricingTest, TestFraNodeAADBaseCurveIsolationOnLayeredForecast) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 10, 15);
    const Dal::Date_ maturity(2029, 1, 15);
    const Dal::Vector_<Dal::Date_> knots{Dal::Date_(2026, 7, 15), Dal::Date_(2027, 1, 15), Dal::Date_(2028, 1, 15)};
    const Dal::Vector_<> overlayParameters{0.005, 0.01, 0.015};
    const auto base = FlatCurve(maturity, 0.02);
    const auto buildCurve = [&](const Dal::Vector_<>& parameters) {
        return Dal::Handle_<Dal::DiscountCurve_>(Dal::NewDiscountPWC("overlay", "USD", Dal::PiecewiseConstant_(knots, parameters), base));
    };
    const auto trade = Trade(Dal::RateInstrumentType_("FRA"), today, start, maturity, FraTerms(false));
    const auto market = ComponentMarket(today, buildCurve(overlayParameters), FlatCurve(maturity, 0.03));
    const auto aad = Dal::RateTradeNodeSensitivities(trade, market, "forecast");
    const auto passive = Dal::PriceRateTrade(trade, market);

    ASSERT_TRUE(passive.succeeded_);
    ASSERT_TRUE(aad.eligible_);
    ASSERT_EQ(static_cast<int>(aad.gradient_.size()), 3);
    ASSERT_DOUBLE_EQ(aad.pv_, passive.pv_);
    for (const double column : aad.gradient_)
        ASSERT_NE(column, 0.0);

    constexpr double bump = 1.0e-6;
    for (int column = 0; column < 3; ++column) {
        auto plusParameters = overlayParameters;
        auto minusParameters = overlayParameters;
        plusParameters[column] += bump;
        minusParameters[column] -= bump;
        const auto plus = Dal::PriceRateTrade(trade, ComponentMarket(today, buildCurve(plusParameters), FlatCurve(maturity, 0.03)));
        const auto minus = Dal::PriceRateTrade(trade, ComponentMarket(today, buildCurve(minusParameters), FlatCurve(maturity, 0.03)));
        ASSERT_TRUE(plus.succeeded_);
        ASSERT_TRUE(minus.succeeded_);
        const double finiteDifference = (plus.pv_ - minus.pv_) / (2.0 * bump);
        const double tolerance = 1.0e-6 + 1.0e-8 * std::max(std::abs(aad.gradient_[column]), std::abs(finiteDifference));
        ASSERT_NEAR(aad.gradient_[column], finiteDifference, tolerance) << "overlay column " << column;
    }
}

TEST(RateCashflowPricingTest, TestFraAndFutureFormulas) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 4, 15);
    const Dal::Date_ maturity(2026, 7, 15);
    const auto curve = FlatCurve(maturity);
    const auto market = Market(today, curve);
    const auto index = QuarterlyIndex();
    const double accrual = index.dayBasis_(start, maturity, nullptr);
    const double forward = (1.0 / (*curve)(start, maturity) - 1.0) / accrual;

    Dal::FraTradeTerms_ fra;
    fra.notional_ = 2'000'000.0;
    fra.contractRate_ = 0.03;
    fra.receiveFloating_ = true;
    fra.settleAtStart_ = true;
    fra.index_ = index;
    fra.fixingIdentity_ = {"USD-LIBOR-3M", 11, 0};
    fra.forecastComponentKey_ = "forecast";
    fra.discountComponentKey_ = "discount";
    const auto fraResult = Dal::PriceRateTrade(Trade(Dal::RateInstrumentType_("FRA"), today, start, maturity, fra), market);
    const double payoff = fra.notional_ * accrual * (forward - fra.contractRate_) / (1.0 + accrual * forward);
    ASSERT_NEAR(fraResult.pv_, payoff * (*curve)(today, start), 1.0e-8);

    Dal::FutureTradeTerms_ future;
    future.contractCount_ = 12.0;
    future.long_ = true;
    future.referencePrice_ = 95.0;
    future.contractValuePerPricePoint_ = 25.0;
    future.convexityAdjustment_ = 0.0005;
    future.index_ = index;
    future.fixingIdentity_ = {"USD-LIBOR-3M", 11, 0};
    future.forecastComponentKey_ = "forecast";
    const auto futureResult = Dal::PriceRateTrade(Trade(Dal::RateInstrumentType_("FUTURE"), today, start, maturity, future), market);
    const double modelPrice = 100.0 * (1.0 - forward + future.convexityAdjustment_);
    ASSERT_NEAR(futureResult.pv_, 12.0 * 25.0 * (modelPrice - 95.0), 1.0e-10);

    future.long_ = false;
    const auto shortResult = Dal::PriceRateTrade(Trade(Dal::RateInstrumentType_("FUTURE"), today, start, maturity, future), market);
    ASSERT_NEAR(shortResult.pv_, -futureResult.pv_, 1.0e-10);
}

TEST(RateCashflowPricingTest, TestFloatingFamilySidesAreExactOpposites) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ maturity(2028, 1, 15);
    const auto curve = FlatCurve(maturity);
    const auto market = Market(today, curve);

    Dal::FixedFloatTradeTerms_ fixedFloat;
    fixedFloat.notional_ = 1'000'000.0;
    fixedFloat.contractRate_ = 0.03;
    fixedFloat.payFixed_ = true;
    fixedFloat.fixedLeg_ = AnnualLeg();
    fixedFloat.floatLeg_ = AnnualLeg();
    fixedFloat.floatIndex_ = QuarterlyIndex();
    fixedFloat.fixingIdentity_ = {"USD-SOFR", 11, 0};
    fixedFloat.forecastComponentKey_ = "forecast";
    fixedFloat.discountComponentKey_ = "discount";
    for (const auto& family : {"OIS", "IRS"}) {
        const auto type = Dal::RateInstrumentType_(family);
        const double receive = Dal::PriceRateTrade(Trade(type, today, today, maturity, AsFamilyTerms(type, fixedFloat)), market).pv_;
        fixedFloat.payFixed_ = false;
        const double pay = Dal::PriceRateTrade(Trade(type, today, today, maturity, AsFamilyTerms(type, fixedFloat)), market).pv_;
        ASSERT_NEAR(receive, -pay, 1.0e-10);
        fixedFloat.payFixed_ = true;
    }

    const auto basis = BasisTerms();
    const auto first = Dal::PriceRateTrade(Trade(Dal::RateInstrumentType_("BASIS_SWAP"), today, today, maturity, basis), market);
    const auto opposite = Dal::PriceRateTrade(Trade(Dal::RateInstrumentType_("BASIS_SWAP"), today, today, maturity, BasisTerms(false)), market);
    ASSERT_NEAR(first.pv_, -opposite.pv_, 1.0e-10);
}

TEST(RateCashflowPricingTest, TestXccySpreadLegSidesAndPositionCount) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ maturity(2028, 1, 15);
    const auto usd = FlatCurve(maturity, 0.04, "USD");
    const auto eur = FlatCurve(maturity, 0.04, "EUR");
    const auto collateral = Dal::CollateralType_(Dal::CollateralType_::Value_::OIS);
    const auto usdBlock = Dal::Handle_<Dal::CurveBlock_>(new Dal::CurveBlock_("usd", "USD", {{collateral, usd}}, {}, Dal::DayBasis_("ACT_365F")));
    const auto eurBlock = Dal::Handle_<Dal::CurveBlock_>(new Dal::CurveBlock_("eur", "EUR", {{collateral, eur}}, {}, Dal::DayBasis_("ACT_365F")));
    const auto fixings = Dal::Handle_<Dal::MarketFixingSnapshot_>(new Dal::MarketFixingSnapshot_());
    const auto xccyMarket =
        std::make_shared<Dal::CrossCurrencyMarket_>(usdBlock, eurBlock, 1.2, Dal::DateTime_(today, 10, 30), Dal::Ccy_("USD"), fixings);

    Dal::XccyTradeTerms_ terms;
    terms.positionCount_ = 2.0;
    terms.contractSpread_ = 0.001;
    terms.spreadOnForeignLeg_ = true;
    terms.receiveNonSpreadPaySpread_ = true;
    terms.config_.pair_ = Dal::CurrencyPair_(Dal::Ccy_("USD"), Dal::Ccy_("EUR"));
    terms.config_.domesticNotional_ = 120.0;
    terms.config_.foreignNotional_ = 100.0;
    terms.config_.convention_.domesticLeg_ = AnnualLeg();
    terms.config_.convention_.foreignLeg_ = AnnualLeg();
    terms.config_.convention_.domesticIndex_ = QuarterlyIndex();
    terms.config_.convention_.foreignIndex_ = QuarterlyIndex();
    terms.config_.convention_.spreadOnForeignLeg_ = true;
    terms.config_.domesticRateFixing_ = {"USD-SOFR", 11, 0};
    terms.config_.foreignRateFixing_ = {"EUR-ESTR", 11, 0};

    Dal::RatePricingMarket_ market;
    market.valuationTime_ = Dal::DateTime_(today, 10, 30);
    market.resultCurrency_ = Dal::Ccy_("USD");
    market.xccyMarket_ = xccyMarket;
    market.fixings_ = fixings;
    const auto trade = Trade(Dal::RateInstrumentType_("XCCY"), today, today, maturity, terms);
    const auto receive = Dal::PriceRateTrade(trade, market);
    ASSERT_TRUE(receive.succeeded_);

    terms.receiveNonSpreadPaySpread_ = false;
    const auto pay = Dal::PriceRateTrade(Trade(Dal::RateInstrumentType_("XCCY"), today, today, maturity, terms), market);
    ASSERT_TRUE(pay.succeeded_);
    ASSERT_NEAR(receive.pv_, -pay.pv_, 1.0e-10);

    terms.positionCount_ = 1.0;
    terms.receiveNonSpreadPaySpread_ = true;
    const auto single = Dal::PriceRateTrade(Trade(Dal::RateInstrumentType_("XCCY"), today, today, maturity, terms), market);
    ASSERT_TRUE(single.succeeded_);
    ASSERT_NEAR(receive.pv_, 2.0 * single.pv_, 1.0e-10);
}

TEST(RateCashflowPricingTest, TestFixingAndPaymentBoundaries) {
    const Dal::Date_ start(2026, 1, 15);
    const Dal::Date_ maturity(2026, 4, 15);
    const auto curve = FlatCurve(maturity);
    Dal::FraTradeTerms_ terms;
    terms.notional_ = 1'000'000.0;
    terms.contractRate_ = 0.03;
    terms.receiveFloating_ = true;
    terms.settleAtStart_ = false;
    terms.index_ = QuarterlyIndex();
    terms.fixingIdentity_ = {"USD-LIBOR-3M", 11, 0};
    terms.forecastComponentKey_ = "forecast";
    terms.discountComponentKey_ = "discount";
    const auto trade = Trade(Dal::RateInstrumentType_("FRA"), start, start, maturity, terms);

    auto missingMarket = Market(start, curve);
    missingMarket.valuationTime_ = Dal::DateTime_(start.AddDays(1), 10, 30);
    const auto missing = Dal::PriceRateTrade(trade, missingMarket);
    ASSERT_FALSE(missing.succeeded_);
    ASSERT_EQ(missing.requiredHistoricalFixings_.size(), 1);
    ASSERT_EQ(missing.missingHistoricalFixings_.size(), 1);

    auto atFixing = Market(start, curve);
    atFixing.valuationTime_ = Dal::DateTime_(start, 11, 0);
    const auto forecast = Dal::PriceRateTrade(trade, atFixing);
    ASSERT_TRUE(forecast.succeeded_);
    ASSERT_TRUE(forecast.requiredHistoricalFixings_.empty());

    Dal::MarketFixingSnapshot_::values_t history;
    history["USD-LIBOR-3M"][Dal::DateTime_(start, 11, 0)] = -0.001;
    auto atPayment = Market(start, curve);
    atPayment.valuationTime_ = Dal::DateTime_(maturity, 10, 30);
    atPayment.fixings_ = Dal::Handle_<Dal::MarketFixingSnapshot_>(new Dal::MarketFixingSnapshot_(history));
    const auto payment = Dal::PriceRateTrade(trade, atPayment);
    ASSERT_TRUE(payment.succeeded_);
    const double accrual = terms.index_.dayBasis_(start, maturity, nullptr);
    ASSERT_NEAR(payment.pv_, terms.notional_ * accrual * (-0.001 - terms.contractRate_), 1.0e-10);

    auto expired = Market(start, curve);
    expired.valuationTime_ = Dal::DateTime_(maturity.AddDays(1), 10, 30);
    const auto paid = Dal::PriceRateTrade(trade, expired);
    ASSERT_TRUE(paid.succeeded_);
    ASSERT_DOUBLE_EQ(paid.pv_, 0.0);
    ASSERT_TRUE(paid.requiredHistoricalFixings_.empty());
}

TEST(RateCashflowPricingTest, TestOisPlanRecordsEachDailyHistoricalObservation) {
    const Dal::Date_ start(2026, 1, 15);
    const Dal::Date_ maturity(2026, 4, 15);
    Dal::FixedFloatTradeTerms_ fixedFloat;
    fixedFloat.notional_ = 1'000'000.0;
    fixedFloat.contractRate_ = 0.03;
    fixedFloat.payFixed_ = true;
    fixedFloat.fixedLeg_ = AnnualLeg();
    fixedFloat.floatLeg_ = AnnualLeg();
    fixedFloat.floatIndex_ = QuarterlyIndex();
    fixedFloat.fixingIdentity_ = {"USD-SOFR", 11, 0};
    fixedFloat.forecastComponentKey_ = "forecast";
    fixedFloat.discountComponentKey_ = "discount";
    const auto trade = Trade(Dal::RateInstrumentType_("OIS"), start, start, maturity, Dal::OisTradeTerms_{fixedFloat});

    const auto plan = Dal::BuildRateCashflowPlan(trade, Dal::DateTime_(start.AddDays(2), 10, 30));

    ASSERT_EQ(plan.requiredHistoricalFixings_.size(), 2);
    ASSERT_EQ(plan.requiredHistoricalFixings_[0].fixingTime_, Dal::DateTime_(start, 11, 0));
    ASSERT_EQ(plan.requiredHistoricalFixings_[1].fixingTime_, Dal::DateTime_(start.AddDays(1), 11, 0));
}

TEST(RateCashflowPricingTest, TestBatchCellsMatchSingleTradeCallsAndIsolatePartialFailure) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 10, 15);
    const Dal::Date_ maturity(2029, 1, 15);
    const auto market = ComponentMarket(today, FlatCurve(maturity, 0.04), FlatCurve(maturity, 0.03));

    auto firstDeposit = DepositTerms();
    firstDeposit.notional_ = 100.0;
    auto secondDeposit = DepositTerms();
    secondDeposit.notional_ = 250.0;
    secondDeposit.lend_ = false;
    auto invalidFra = FraTerms();
    invalidFra.notional_ = std::numeric_limits<double>::quiet_NaN();
    const Dal::Vector_<Dal::RateTradeDefinition_> trades{
        Trade(Dal::RateInstrumentType_("DEPOSIT"), today, start, maturity, firstDeposit),
        Trade(Dal::RateInstrumentType_("DEPOSIT"), today, start, maturity, secondDeposit),
        Trade(Dal::RateInstrumentType_("FRA"), today, start, maturity, invalidFra),
        Trade(Dal::RateInstrumentType_("OIS"), today, start, maturity, Dal::OisTradeTerms_{FixedFloatTerms()}),
    };
    const Dal::Vector_<Dal::String_> keys{"discount", "forecast"};

    const auto cells = Dal::RateTradeNodeSensitivitiesBatch(trades, market, keys);

    // Deterministic serial order: trade-major, then the shared key list.
    ASSERT_EQ(cells.size(), 8u);
    for (int tradeIndex = 0; tradeIndex < 4; ++tradeIndex)
        for (int keyIndex = 0; keyIndex < 2; ++keyIndex) {
            const auto& cell = cells[tradeIndex * 2 + keyIndex];
            ASSERT_EQ(cell.instrumentId_, trades[tradeIndex].instrumentId_);
            ASSERT_EQ(cell.componentKey_, keys[keyIndex]);
        }

    // Successful entries are numerically identical to the single-trade calls, including cells that
    // follow failing entries in the same batch (per-entry isolation).
    for (int tradeIndex : {0, 1, 3})
        for (int keyIndex = 0; keyIndex < 2; ++keyIndex) {
            const auto& cell = cells[tradeIndex * 2 + keyIndex];
            const auto single = Dal::RateTradeNodeSensitivities(trades[tradeIndex], market, keys[keyIndex]);
            ASSERT_EQ(cell.result_.eligible_, single.eligible_);
            ASSERT_DOUBLE_EQ(cell.result_.pv_, single.pv_);
            ASSERT_EQ(cell.result_.gradient_, single.gradient_);
            ASSERT_EQ(cell.result_.reason_, single.reason_);
        }

    // Deposit never depends on the forecast key; the invalid FRA fails validation on both keys.
    AssertCanonicalFailure(cells[1].result_, "TRADE_DOES_NOT_DEPEND_ON_COMPONENT");
    AssertCanonicalFailure(cells[3].result_, "TRADE_DOES_NOT_DEPEND_ON_COMPONENT");
    AssertCanonicalFailure(cells[4].result_, "TRADE_VALIDATION_FAILED");
    AssertCanonicalFailure(cells[5].result_, "TRADE_VALIDATION_FAILED");
}

TEST(RateCashflowPricingTest, TestBatchHandlesEmptyListsAndDuplicateKeys) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ maturity(2027, 1, 15);
    const auto market = Market(today, FlatCurve(maturity));
    const auto trade = Trade(Dal::RateInstrumentType_("DEPOSIT"), today, today, maturity, DepositTerms());

    ASSERT_TRUE(Dal::RateTradeNodeSensitivitiesBatch({}, market, {"discount"}).empty());
    ASSERT_TRUE(Dal::RateTradeNodeSensitivitiesBatch({trade}, market, {}).empty());

    const auto cells = Dal::RateTradeNodeSensitivitiesBatch({trade}, market, {"discount", "discount", "unknown"});
    ASSERT_EQ(cells.size(), 3u);
    const auto single = Dal::RateTradeNodeSensitivities(trade, market, "discount");
    for (int duplicate = 0; duplicate < 2; ++duplicate) {
        ASSERT_EQ(cells[duplicate].componentKey_, "discount");
        ASSERT_DOUBLE_EQ(cells[duplicate].result_.pv_, single.pv_);
        ASSERT_EQ(cells[duplicate].result_.gradient_, single.gradient_);
    }
    AssertCanonicalFailure(cells[2].result_, "TRADE_DOES_NOT_DEPEND_ON_COMPONENT");
}

TEST(RateCashflowPricingTest, TestBatchHoistsPassivePricingAndPerCurvePreparation) {
    namespace internal = Dal::RateCashflowPricingInternal;
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 10, 15);
    const Dal::Date_ maturity(2029, 1, 15);

    {
        // Three FRAs over two shared keys: one passive PV per trade, one preparation per curve.
        const auto market = ComponentMarket(today, FlatCurve(maturity, 0.04), FlatCurve(maturity, 0.03));
        const Dal::Vector_<Dal::RateTradeDefinition_> trades{
            Trade(Dal::RateInstrumentType_("FRA"), today, start, maturity, FraTerms()),
            Trade(Dal::RateInstrumentType_("FRA"), today, start, maturity, FraTerms(false)),
            Trade(Dal::RateInstrumentType_("FRA"), today, start, maturity, FraTerms(false)),
        };
        internal::g_nodeSensitivityPassivePriceCount = 0;
        internal::g_nodeSensitivityPreparationCount = 0;
        const auto cells = Dal::RateTradeNodeSensitivitiesBatch(trades, market, {"forecast", "discount"});
        ASSERT_EQ(cells.size(), 6u);
        for (const auto& cell : cells)
            ASSERT_TRUE(cell.result_.eligible_);
        ASSERT_EQ(internal::g_nodeSensitivityPassivePriceCount, 3);
        ASSERT_EQ(internal::g_nodeSensitivityPreparationCount, 2);
    }

    {
        // Trades whose every cell fails the dependency gate are never passively priced.
        const auto market = Market(today, FlatCurve(maturity));
        const auto trade = Trade(Dal::RateInstrumentType_("DEPOSIT"), today, today, maturity, DepositTerms());
        internal::g_nodeSensitivityPassivePriceCount = 0;
        internal::g_nodeSensitivityPreparationCount = 0;
        const auto cells = Dal::RateTradeNodeSensitivitiesBatch({trade}, market, {"unrelated"});
        ASSERT_EQ(cells.size(), 1u);
        AssertCanonicalFailure(cells[0].result_, "TRADE_DOES_NOT_DEPEND_ON_COMPONENT");
        ASSERT_EQ(internal::g_nodeSensitivityPassivePriceCount, 0);
        ASSERT_EQ(internal::g_nodeSensitivityPreparationCount, 0);
    }

    {
        // Two XCCY trades sharing one market: the five consumed curves are prepared once between
        // them (pointer-keyed memo), and each trade is passively priced once.
        const auto market = FlatXccyMarket(today);
        const Dal::Vector_<Dal::RateTradeDefinition_> trades{
            Trade(Dal::RateInstrumentType_("XCCY"), today, start, maturity, Dal::RateTradeTerms_(XccyTerms())),
            Trade(Dal::RateInstrumentType_("XCCY"), today, start, maturity, Dal::RateTradeTerms_(XccyTerms(false))),
        };
        internal::g_nodeSensitivityPassivePriceCount = 0;
        internal::g_nodeSensitivityPreparationCount = 0;
        const auto cells = Dal::RateTradeNodeSensitivitiesBatch(trades, market, {XCCY_DOM_OIS, XCCY_FOR_FWD_3M});
        ASSERT_EQ(cells.size(), 4u);
        for (const auto& cell : cells)
            ASSERT_TRUE(cell.result_.eligible_);
        ASSERT_EQ(internal::g_nodeSensitivityPassivePriceCount, 2);
        ASSERT_EQ(internal::g_nodeSensitivityPreparationCount, 5);
    }
}

TEST(RateCashflowPricingTest, TestBatchXccyCellsMatchSingleTradeCalls) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 10, 15);
    const Dal::Date_ maturity(2029, 1, 15);
    const auto market = FlatXccyMarket(today);
    const Dal::Vector_<Dal::RateTradeDefinition_> trades{
        Trade(Dal::RateInstrumentType_("XCCY"), today, start, maturity, Dal::RateTradeTerms_(XccyTerms())),
        Trade(Dal::RateInstrumentType_("XCCY"), today, start, maturity, Dal::RateTradeTerms_(XccyTerms(true, false))),
    };
    const Dal::Vector_<Dal::String_> keys{XCCY_DOM_OIS, XCCY_DOM_FWD_3M, XCCY_FOR_OIS, XCCY_FOR_FWD_3M, XCCY_BASIS};

    const auto cells = Dal::RateTradeNodeSensitivitiesBatch(trades, market, keys);
    ASSERT_EQ(cells.size(), 10u);
    for (int tradeIndex = 0; tradeIndex < 2; ++tradeIndex)
        for (int keyIndex = 0; keyIndex < 5; ++keyIndex) {
            const auto& cell = cells[tradeIndex * 5 + keyIndex];
            const auto single = Dal::RateTradeNodeSensitivities(trades[tradeIndex], market, keys[keyIndex]);
            ASSERT_EQ(cell.result_.eligible_, single.eligible_);
            ASSERT_DOUBLE_EQ(cell.result_.pv_, single.pv_);
            ASSERT_EQ(cell.result_.gradient_, single.gradient_);
            ASSERT_EQ(cell.result_.reason_, single.reason_);
        }
}

TEST(RateCashflowPricingTest, TestAggregatePortfolioNodeRiskBuildsDensePerComponentTensors) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 10, 15);
    const Dal::Date_ maturity(2029, 1, 15);
    const Dal::Vector_<Dal::Date_> knots{Dal::Date_(2026, 7, 15), Dal::Date_(2027, 7, 15), Dal::Date_(2028, 7, 15)};
    const auto discount = Dal::Handle_<Dal::DiscountCurve_>(
        Dal::NewDiscountPWC("pwc", "USD", Dal::PiecewiseConstant_(knots, Dal::Vector_<>(3, 0.03))));
    const auto market = ComponentMarket(today, FlatCurve(maturity, 0.04), discount);
    const Dal::Vector_<Dal::RateTradeDefinition_> trades{
        Trade(Dal::RateInstrumentType_("DEPOSIT"), today, today, maturity, DepositTerms()),
        Trade(Dal::RateInstrumentType_("FRA"), today, start, maturity, FraTerms()),
    };
    const Dal::Vector_<Dal::String_> keys{"discount", "forecast"};

    const auto aggregate = Dal::AggregateRatePortfolioNodeRisk(trades, market, keys);

    ASSERT_EQ(aggregate.policy_, "UnconvertedByActualPvCcy");
    ASSERT_EQ(aggregate.meta_.size(), 4u);
    ASSERT_TRUE(aggregate.meta_[0].eligible_);
    ASSERT_FALSE(aggregate.meta_[1].eligible_);
    ASSERT_EQ(aggregate.meta_[1].reason_, "TRADE_DOES_NOT_DEPEND_ON_COMPONENT");
    ASSERT_TRUE(aggregate.meta_[2].eligible_);
    ASSERT_TRUE(aggregate.meta_[3].eligible_);

    // PV totals count each trade once, not once per component.
    ASSERT_EQ(aggregate.pvByActualPvCcy_.size(), 1u);
    const double expectedPv = Dal::RateTradeNodeSensitivities(trades[0], market, "discount").pv_ +
                               Dal::RateTradeNodeSensitivities(trades[1], market, "discount").pv_;
    ASSERT_NEAR(aggregate.pvByActualPvCcy_.at("USD"), expectedPv, 1.0e-10 * std::max(1.0, std::abs(expectedPv)));

    // One dense Report_ per component, node axis from the layout, header rows from the free
    // parameters, values the sum of the per-trade gradients.
    ASSERT_EQ(aggregate.components_.size(), 2u);
    ASSERT_EQ(aggregate.components_[0].componentKey_, "discount");
    ASSERT_EQ(aggregate.components_[1].componentKey_, "forecast");
    const auto& discountComponent = *aggregate.components_[0].values_;
    const auto definition = Dal::MakeCurveDefinition("pwc", "USD", Dal::CurveParameterization_(Dal::CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD),
                                                     Dal::LogDfScheme_::Value_::LOG_LINEAR, knots, today, Dal::DayBasis_("ACT_365F"));
    const int expectedCount = Dal::BuildCurveParameterLayout(definition).parameterCount_;
    ASSERT_EQ(discountComponent.Size("node"), expectedCount);
    const auto freeParameters = Dal::DescribeCurveFreeParameters(definition);
    const auto& header = discountComponent.Header("node");
    for (int node = 0; node < expectedCount; ++node) {
        Dal::Report::Address_ address = discountComponent.MakeAddress();
        address["node"] = node;
        const double expected = Dal::RateTradeNodeSensitivities(trades[0], market, "discount").gradient_[node] +
                                Dal::RateTradeNodeSensitivities(trades[1], market, "discount").gradient_[node];
        ASSERT_DOUBLE_EQ(discountComponent[address], expected);
        ASSERT_EQ(header.values_(node, 0), Dal::Cell_(freeParameters[node].date_));
        ASSERT_EQ(header.values_(node, 1), Dal::Cell_(freeParameters[node].component_.String()));
    }
}

TEST(RateCashflowPricingTest, TestAggregatePortfolioNodeRiskGroupsByActualPvCurrency) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 10, 15);
    const Dal::Date_ maturity(2029, 1, 15);

    {
        // The passthrough result currency is never the grouping key: report in JPY, group in USD.
        auto market = Market(today, FlatCurve(maturity));
        market.resultCurrency_ = Dal::Ccy_("JPY");
        const auto trade = Trade(Dal::RateInstrumentType_("DEPOSIT"), today, today, maturity, DepositTerms());
        const auto aggregate = Dal::AggregateRatePortfolioNodeRisk({trade}, market, {"discount"});
        ASSERT_EQ(aggregate.pvByActualPvCcy_.size(), 1u);
        ASSERT_EQ(aggregate.pvByActualPvCcy_.count("JPY"), 0u) << "result currency leaked into the grouping";
        ASSERT_EQ(aggregate.pvByActualPvCcy_.count("USD"), 1u);
        ASSERT_EQ(aggregate.meta_[0].actualPvCcy_, Dal::Ccy_("USD"));
    }

    {
        // XCCY PV is domestic by covered-interest parity: the USD/EUR swap groups under USD while
        // an EUR-denominated FRA on the same market's foreign curves groups under EUR.
        const auto market = FlatXccyMarket(today);
        const auto xccy = Trade(Dal::RateInstrumentType_("XCCY"), today, start, maturity, Dal::RateTradeTerms_(XccyTerms()));
        auto eurTerms = FraTerms();
        eurTerms.forecastComponentKey_ = XCCY_FOR_FWD_3M;
        eurTerms.discountComponentKey_ = XCCY_FOR_OIS;
        const Dal::RateTradeDefinition_ eurFra{"fra-eur", Dal::RateInstrumentType_("FRA"), today, start, maturity, Dal::Ccy_("EUR"), eurTerms};
        const auto aggregate = Dal::AggregateRatePortfolioNodeRisk({xccy, eurFra}, market, {XCCY_DOM_OIS, XCCY_FOR_FWD_3M});
        ASSERT_EQ(aggregate.meta_.size(), 4u);
        ASSERT_TRUE(aggregate.meta_[0].eligible_);
        ASSERT_TRUE(aggregate.meta_[3].eligible_);
        ASSERT_EQ(aggregate.pvByActualPvCcy_.size(), 2u);
        ASSERT_EQ(aggregate.pvByActualPvCcy_.count("USD"), 1u);
        ASSERT_EQ(aggregate.pvByActualPvCcy_.count("EUR"), 1u);
        ASSERT_EQ(aggregate.meta_[0].actualPvCcy_, Dal::Ccy_("USD"));
        ASSERT_EQ(aggregate.meta_[3].actualPvCcy_, Dal::Ccy_("EUR"));
    }
}

TEST(RateCashflowPricingTest, TestAggregatePortfolioNodeRiskKeepsFailuresInMetaTableOnly) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ maturity(2027, 1, 15);
    const auto market = Market(today, FlatCurve(maturity));
    auto invalid = DepositTerms();
    invalid.notional_ = std::numeric_limits<double>::quiet_NaN();
    const Dal::Vector_<Dal::RateTradeDefinition_> trades{
        Trade(Dal::RateInstrumentType_("DEPOSIT"), today, today, maturity, DepositTerms()),
        Trade(Dal::RateInstrumentType_("DEPOSIT"), today, today, maturity, invalid),
    };
    const Dal::Vector_<Dal::String_> keys{"discount", "missing"};

    const auto aggregate = Dal::AggregateRatePortfolioNodeRisk(trades, market, keys);

    ASSERT_EQ(aggregate.meta_.size(), 4u);
    ASSERT_TRUE(aggregate.meta_[0].eligible_);
    for (const int row : {1, 3}) {
        ASSERT_FALSE(aggregate.meta_[row].eligible_);
        ASSERT_DOUBLE_EQ(aggregate.meta_[row].pv_, 0.0);
        ASSERT_EQ(aggregate.meta_[row].reason_, "TRADE_DOES_NOT_DEPEND_ON_COMPONENT");
    }
    ASSERT_FALSE(aggregate.meta_[2].eligible_);
    ASSERT_EQ(aggregate.meta_[2].reason_, "TRADE_VALIDATION_FAILED");

    // "missing" has no tensor (no classified curve to label an axis); "discount" keeps its dense
    // zero-or-summed tensor. No padding, no ragged rows.
    ASSERT_EQ(aggregate.components_.size(), 1u);
    ASSERT_EQ(aggregate.components_[0].componentKey_, "discount");
}

TEST(RateCashflowPricingTest, TestXccyNodeAADReasonPrecedenceForCombinedFailures) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 10, 15);
    const Dal::Date_ maturity(2029, 1, 15);
    const auto horizon = Dal::Date_(2031, 1, 15);
    const auto trade = Trade(Dal::RateInstrumentType_("XCCY"), today, start, maturity, Dal::RateTradeTerms_(XccyTerms()));
    auto invalidTerms = XccyTerms();
    invalidTerms.positionCount_ = std::numeric_limits<double>::quiet_NaN();
    const auto invalidTrade = Trade(Dal::RateInstrumentType_("XCCY"), today, start, maturity, Dal::RateTradeTerms_(invalidTerms));
    const auto supported = FlatXccyMarket(today);

    // Family precedes everything: mismatched terms alternative stays on the family token.
    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(
                               Trade(Dal::RateInstrumentType_("IRS"), today, start, maturity, DepositTerms()), supported, XCCY_DOM_OIS),
                           "TRADE_FAMILY_NOT_AAD_ENABLED");

    // Requested dependency precedes classification and validation: wrong key wins on a market
    // whose addressed target is unclassifiable and whose trade is invalid.
    auto unsupportedTarget = FlatXccySpec(horizon);
    unsupportedTarget.domesticOis_ = Dal::Handle_<Dal::DiscountCurve_>(std::make_shared<const UnsupportedDiscountCurve_>());
    const auto unsupportedTargetMarket = BuildXccyMarket(today, unsupportedTarget);
    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(invalidTrade, unsupportedTargetMarket, "unknown"), "TRADE_DOES_NOT_DEPEND_ON_COMPONENT");

    // The classification walk precedes passive validation: the addressed unclassifiable target
    // keeps the representation token even when passive pricing would also fail, and a consumed
    // non-target that cannot be classified is an evaluation failure on the same precedence.
    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(invalidTrade, unsupportedTargetMarket, XCCY_DOM_OIS), "CURVE_REPRESENTATION_NOT_AAD_ENABLED");
    auto unsupportedForeign = FlatXccySpec(horizon);
    unsupportedForeign.foreignOis_ = Dal::Handle_<Dal::DiscountCurve_>(std::make_shared<const UnsupportedDiscountCurve_>("EUR"));
    const auto unsupportedForeignMarket = BuildXccyMarket(today, unsupportedForeign);
    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(invalidTrade, unsupportedForeignMarket, XCCY_DOM_OIS), "AAD_EVALUATION_FAILED");

    // Walk order decides between two unclassifiable consumed curves: domestic discount walks
    // first, so requesting the foreign discount still reports the domestic failure as evaluation,
    // while requesting the domestic discount keeps the representation token.
    auto bothUnsupported = FlatXccySpec(horizon);
    bothUnsupported.domesticOis_ = Dal::Handle_<Dal::DiscountCurve_>(std::make_shared<const UnsupportedDiscountCurve_>());
    bothUnsupported.foreignOis_ = Dal::Handle_<Dal::DiscountCurve_>(std::make_shared<const UnsupportedDiscountCurve_>("EUR"));
    const auto bothUnsupportedMarket = BuildXccyMarket(today, bothUnsupported);
    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(trade, bothUnsupportedMarket, XCCY_DOM_OIS), "CURVE_REPRESENTATION_NOT_AAD_ENABLED");
    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(trade, bothUnsupportedMarket, XCCY_FOR_OIS), "AAD_EVALUATION_FAILED");

    // Passive validation on an otherwise clean market.
    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(invalidTrade, supported, XCCY_DOM_OIS), "TRADE_VALIDATION_FAILED");
}

#if !defined(DAL_USE_XAD_AAD) && !defined(DAL_USE_CODIPACK_AAD) && !defined(DAL_USE_ADEPT_AAD)
TEST(RateCashflowPricingTest, TestNodeSensitivitySweepTapeSizeIndependentOfPassiveCurveDensity) {
    // Direct native seam (Tape_::nodes_ / BlockList_::Size()): an unregistered-constant
    // AAD::Number_ passive curve would still pass value and width assertions, but its per-knot
    // construction state would land on the tape — only this count separates the two.
    namespace internal = Dal::RateCashflowPricingInternal;
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 10, 15);
    const Dal::Date_ maturity(2029, 1, 15);
    const auto forecast = Dal::Handle_<Dal::DiscountCurve_>(Dal::NewDiscountPWC(
        "pwc", "USD", Dal::PiecewiseConstant_({Dal::Date_(2026, 7, 15), Dal::Date_(2027, 7, 15), Dal::Date_(2028, 7, 15)}, {0.01, -0.005, 0.03})));
    Dal::Vector_<Dal::Date_> denseKnots;
    for (Dal::Date_ knot = Dal::Date_(2026, 2, 15); knot <= Dal::Date_(2029, 12, 15); knot = knot.AddDays(30))
        denseKnots.push_back(knot);
    const auto denseDiscount = Dal::Handle_<Dal::DiscountCurve_>(
        Dal::NewDiscountPWC("dense", "USD", Dal::PiecewiseConstant_(denseKnots, Dal::Vector_<>(denseKnots.size(), 0.03))));
    const auto smallDiscount = FlatCurve(maturity, 0.03);

    std::atomic<int> fraSmallTape{0};
    std::atomic<int> fraDenseTape{0};
    std::atomic<int> oisSmallTape{0};
    std::atomic<int> oisDenseTape{0};
    const auto fraTrade = Trade(Dal::RateInstrumentType_("FRA"), today, start, maturity, FraTerms());
    const auto oisTrade = Trade(Dal::RateInstrumentType_("OIS"), today, start, maturity, Dal::OisTradeTerms_{FixedFloatTerms()});

    {
        internal::NodeSensitivityTapeSizeObservation_ observation(fraSmallTape);
        ASSERT_TRUE(Dal::RateTradeNodeSensitivities(fraTrade, ComponentMarket(today, forecast, smallDiscount), "forecast").eligible_);
    }
    {
        internal::NodeSensitivityTapeSizeObservation_ observation(fraDenseTape);
        ASSERT_TRUE(Dal::RateTradeNodeSensitivities(fraTrade, ComponentMarket(today, forecast, denseDiscount), "forecast").eligible_);
    }
    {
        internal::NodeSensitivityTapeSizeObservation_ observation(oisSmallTape);
        ASSERT_TRUE(Dal::RateTradeNodeSensitivities(oisTrade, ComponentMarket(today, forecast, smallDiscount), "forecast").eligible_);
    }
    {
        internal::NodeSensitivityTapeSizeObservation_ observation(oisDenseTape);
        ASSERT_TRUE(Dal::RateTradeNodeSensitivities(oisTrade, ComponentMarket(today, forecast, denseDiscount), "forecast").eligible_);
    }

    ASSERT_GT(fraSmallTape.load(), 0);
    ASSERT_GT(oisSmallTape.load(), 0);
    ASSERT_EQ(fraSmallTape.load(), fraDenseTape.load()) << "FRA sweep tape grows with passive discount node count";
    ASSERT_EQ(oisSmallTape.load(), oisDenseTape.load()) << "OIS sweep tape grows with passive discount node count";
    ASSERT_GT(oisSmallTape.load(), fraSmallTape.load()) << "OIS daily compounding shapes the recording";

    std::atomic<int> mixedHighWater{0};
    {
        internal::NodeSensitivityTapeSizeObservation_ observation(mixedHighWater);
        ASSERT_TRUE(Dal::RateTradeNodeSensitivities(oisTrade, ComponentMarket(today, forecast, smallDiscount), "forecast").eligible_);
        ASSERT_TRUE(Dal::RateTradeNodeSensitivities(fraTrade, ComponentMarket(today, forecast, smallDiscount), "forecast").eligible_);
    }
    ASSERT_EQ(mixedHighWater.load(), oisSmallTape.load()) << "The tape observation seam did not retain the per-sweep high-water mark";
}

TEST(RateCashflowPricingTest, TestNodeSensitivityTapeObservationIsScopedAndConcurrent) {
    namespace internal = Dal::RateCashflowPricingInternal;
    std::atomic<int> highWater{0};
    {
        internal::NodeSensitivityTapeSizeObservation_ observation(highWater);
        std::thread low([]() {
            for (int i = 0; i < 100; ++i)
                internal::RecordNodeSensitivityTapeSize(37);
        });
        std::thread high([]() {
            for (int i = 0; i < 100; ++i)
                internal::RecordNodeSensitivityTapeSize(83);
        });
        low.join();
        high.join();
        ASSERT_EQ(highWater.load(), 83);
    }
    ASSERT_EQ(internal::g_nodeSensitivityTapeSizeSink.load(), nullptr);

    EXPECT_THROW(
        {
            std::atomic<int> ignored{0};
            internal::NodeSensitivityTapeSizeObservation_ observation(ignored);
            throw std::runtime_error("injected observation failure");
        },
        std::runtime_error);
    ASSERT_EQ(internal::g_nodeSensitivityTapeSizeSink.load(), nullptr);
}
#endif

TEST(RateCashflowPricingTest, TestAggregatePortfolioNodeRiskCountsDuplicateKeysOnce) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ maturity(2027, 1, 15);
    const auto market = Market(today, FlatCurve(maturity));
    const auto trade = Trade(Dal::RateInstrumentType_("DEPOSIT"), today, today, maturity, DepositTerms());

    // Duplicate keys still produce one meta row per cell, but the PV total and every component
    // tensor must match the single-key aggregate exactly -- never scaled by the duplicate count.
    const auto single = Dal::AggregateRatePortfolioNodeRisk({trade}, market, {"discount"});
    const auto duplicated = Dal::AggregateRatePortfolioNodeRisk({trade}, market, {"discount", "discount", "discount"});

    ASSERT_EQ(single.meta_.size(), 1u);
    ASSERT_EQ(duplicated.meta_.size(), 3u);
    for (const auto& meta : duplicated.meta_)
        ASSERT_TRUE(meta.eligible_);
    ASSERT_EQ(single.pvByActualPvCcy_.size(), duplicated.pvByActualPvCcy_.size());
    ASSERT_DOUBLE_EQ(duplicated.pvByActualPvCcy_.at("USD"), single.pvByActualPvCcy_.at("USD"));
    ASSERT_EQ(duplicated.components_.size(), 1u);
    ASSERT_EQ(duplicated.components_[0].componentKey_, "discount");
    ASSERT_EQ(duplicated.components_[0].values_->Size("node"), single.components_[0].values_->Size("node"));
    Dal::Report::Address_ singleAddress = single.components_[0].values_->MakeAddress();
    Dal::Report::Address_ duplicatedAddress = duplicated.components_[0].values_->MakeAddress();
    for (int node = 0; node < single.components_[0].values_->Size("node"); ++node) {
        singleAddress["node"] = node;
        duplicatedAddress["node"] = node;
        ASSERT_DOUBLE_EQ((*duplicated.components_[0].values_)[duplicatedAddress], (*single.components_[0].values_)[singleAddress]);
    }
}

TEST(RateCashflowPricingTest, TestXccyNodeAADUnresolvableMarketFailsAsValidation) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 10, 15);
    const Dal::Date_ maturity(2029, 1, 15);
    const auto trade = Trade(Dal::RateInstrumentType_("XCCY"), today, start, maturity, Dal::RateTradeTerms_(XccyTerms()));

    // No XCCY market at all: nothing is addressable, and the honest token is the passive
    // validation failure -- not "trade does not depend on component".
    auto noXccyMarket = FlatXccyMarket(today);
    noXccyMarket.xccyMarket_.reset();
    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(trade, noXccyMarket, XCCY_DOM_OIS), "TRADE_VALIDATION_FAILED");
    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(trade, noXccyMarket, "unknown"), "TRADE_VALIDATION_FAILED");

    // An expired trade has no market dependency: passive pricing returns zero before touching the
    // XCCY market, so the request stays on the dependency token even without a resolvable market.
    const auto expired =
        Trade(Dal::RateInstrumentType_("XCCY"), today, today.AddDays(-400), today.AddDays(-5), Dal::RateTradeTerms_(XccyTerms()));
    AssertCanonicalFailure(Dal::RateTradeNodeSensitivities(expired, noXccyMarket, XCCY_DOM_OIS), "TRADE_DOES_NOT_DEPEND_ON_COMPONENT");
}

TEST(RateCashflowPricingTest, TestIrsNodeAADBaseCurveIsolationOnLayeredCurves) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 10, 15);
    const Dal::Date_ maturity(2029, 1, 15);
    const Dal::Vector_<Dal::Date_> knots{Dal::Date_(2026, 7, 15), Dal::Date_(2027, 1, 15), Dal::Date_(2028, 1, 15)};
    const Dal::Vector_<> overlayParameters{0.005, 0.01, 0.015};
    const auto base = FlatCurve(maturity, 0.02);
    const auto buildCurve = [&](const Dal::Vector_<>& parameters) {
        return Dal::Handle_<Dal::DiscountCurve_>(Dal::NewDiscountPWC("overlay", "USD", Dal::PiecewiseConstant_(knots, parameters), base));
    };
    const auto trade = Trade(Dal::RateInstrumentType_("IRS"), today, start, maturity, AsFamilyTerms(Dal::RateInstrumentType_("IRS"), FixedFloatTerms()));

    for (const char* componentKey : {"forecast", "discount"}) {
        const auto assembleMarket = [&](const Dal::Handle_<Dal::DiscountCurve_>& layered) {
            return Dal::String_(componentKey) == "forecast" ? ComponentMarket(today, layered, FlatCurve(maturity, 0.03))
                                                            : ComponentMarket(today, FlatCurve(maturity, 0.035), layered);
        };
        const auto market = assembleMarket(buildCurve(overlayParameters));
        const auto aad = Dal::RateTradeNodeSensitivities(trade, market, componentKey);
        const auto passive = Dal::PriceRateTrade(trade, market);

        ASSERT_TRUE(passive.succeeded_);
        ASSERT_TRUE(aad.eligible_);
        ASSERT_EQ(static_cast<int>(aad.gradient_.size()), 3);
        // The overlay is the active layer: bitwise identical PV, nonzero gradient on every overlay
        // node, while the double base stays passive and out of the tape.
        ASSERT_DOUBLE_EQ(aad.pv_, passive.pv_);
        for (const double column : aad.gradient_)
            ASSERT_NE(column, 0.0);

        constexpr double bump = 1.0e-6;
        for (int column = 0; column < 3; ++column) {
            auto plusParameters = overlayParameters;
            auto minusParameters = overlayParameters;
            plusParameters[column] += bump;
            minusParameters[column] -= bump;
            const auto plus = Dal::PriceRateTrade(trade, assembleMarket(buildCurve(plusParameters)));
            const auto minus = Dal::PriceRateTrade(trade, assembleMarket(buildCurve(minusParameters)));
            ASSERT_TRUE(plus.succeeded_);
            ASSERT_TRUE(minus.succeeded_);
            const double finiteDifference = (plus.pv_ - minus.pv_) / (2.0 * bump);
            const double tolerance = 1.0e-6 + 1.0e-7 * std::max(std::abs(aad.gradient_[column]), std::abs(finiteDifference));
            ASSERT_NEAR(aad.gradient_[column], finiteDifference, tolerance) << componentKey << " overlay column " << column;
        }
    }
}

TEST(RateCashflowPricingTest, TestXccyNodeAADBaseCurveIsolationOnLayeredCurve) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 10, 15);
    const Dal::Date_ maturity(2029, 1, 15);
    const auto horizon = Dal::Date_(2031, 1, 15);
    const Dal::Vector_<Dal::Date_> knots{Dal::Date_(2026, 7, 15), Dal::Date_(2027, 1, 15), Dal::Date_(2028, 1, 15)};
    const Dal::Vector_<> overlayParameters{0.005, 0.01, 0.015};
    const auto base = FlatCurve(horizon, 0.025, "USD");
    const auto buildCurve = [&](const Dal::Vector_<>& parameters) {
        return Dal::Handle_<Dal::DiscountCurve_>(Dal::NewDiscountPWC("overlay", "USD", Dal::PiecewiseConstant_(knots, parameters), base));
    };
    const auto trade = Trade(Dal::RateInstrumentType_("XCCY"), today, start, maturity, Dal::RateTradeTerms_(XccyTerms()));
    const auto assembleMarket = [&](const Dal::Handle_<Dal::DiscountCurve_>& layered) {
        auto spec = FlatXccySpec(horizon);
        spec.domesticFwd3M_ = layered;
        return BuildXccyMarket(today, spec);
    };
    const auto market = assembleMarket(buildCurve(overlayParameters));
    const auto aad = Dal::RateTradeNodeSensitivities(trade, market, XCCY_DOM_FWD_3M);
    const auto passive = Dal::PriceRateTrade(trade, market);

    ASSERT_TRUE(passive.succeeded_);
    ASSERT_TRUE(aad.eligible_);
    ASSERT_EQ(static_cast<int>(aad.gradient_.size()), 3);
    ASSERT_DOUBLE_EQ(aad.pv_, passive.pv_);
    for (const double column : aad.gradient_)
        ASSERT_NE(column, 0.0);

    constexpr double bump = 1.0e-6;
    for (int column = 0; column < 3; ++column) {
        auto plusParameters = overlayParameters;
        auto minusParameters = overlayParameters;
        plusParameters[column] += bump;
        minusParameters[column] -= bump;
        const auto plus = Dal::PriceRateTrade(trade, assembleMarket(buildCurve(plusParameters)));
        const auto minus = Dal::PriceRateTrade(trade, assembleMarket(buildCurve(minusParameters)));
        ASSERT_TRUE(plus.succeeded_);
        ASSERT_TRUE(minus.succeeded_);
        const double finiteDifference = (plus.pv_ - minus.pv_) / (2.0 * bump);
        const double tolerance = 1.0e-6 + 1.0e-7 * std::max(std::abs(aad.gradient_[column]), std::abs(finiteDifference));
        ASSERT_NEAR(aad.gradient_[column], finiteDifference, tolerance) << "xccy overlay column " << column;
    }

    // The other consumed components keep their own live gradients against the same layered market.
    const auto domOis = Dal::RateTradeNodeSensitivities(trade, market, XCCY_DOM_OIS);
    ASSERT_TRUE(domOis.eligible_);
    ASSERT_DOUBLE_EQ(domOis.pv_, passive.pv_);
    ASSERT_GT(std::abs(domOis.gradient_[0]), 1.0e-8);
}

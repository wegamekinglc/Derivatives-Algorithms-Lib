#pragma once

#include <array>

#include <dal-public/src/curveinstrument.hpp>
#include <dal-public/src/curvepricing.hpp>

namespace Dal::StagedQuoteRiskTest {
    inline const Date_ TODAY(2024, 1, 15);
    inline const PeriodLength_ SIX_MONTHS("6M");
    inline const RateQuoteRiskProvenanceConfig_ CONFIG{"EUR-calibration", {{"stage:0", "ESTR"}, {"stage:1", "Euribor6M"}}};
    inline constexpr std::array<double, 4> PORTFOLIO_DV01{-1.13929254, -0.62725918, 109.06027558, -3.41379201};

    inline MultiCurveCalibrationSpec_ Spec() {
        RateLegConvention_ leg;
        leg.paymentFrequency_ = SIX_MONTHS;
        RateIndexConvention_ index;
        index.useProjectionCurve_ = true;
        index.forecastTenor_ = SIX_MONTHS;

        CurveCalibrationSpec_ discount;
        discount.today_ = TODAY;
        discount.curveName_ = "ESTR";
        discount.parameterization_ = CurveParameterization_("ZERO_RATE");
        discount.knotPolicy_ = CurveKnotPolicy_("INSTRUMENTS");
        discount.tolerance_ = 1.e-13;
        discount.initialGuess_ = 0.02;
        auto forward = discount;
        forward.curveName_ = "Euribor6M";
        forward.calibrateDiscountCurve_ = false;
        forward.targetTenor_ = SIX_MONTHS;
        forward.tolerance_ = 5.e-13;
        forward.initialGuess_ = 0.01;
        const auto longDate = Date::AddMonths(TODAY, 24), shortDate = Date::AddMonths(TODAY, 6);
        discount.instruments_ = {DepositNew(TODAY, TODAY, longDate, .035, {}), DepositNew(TODAY, TODAY, shortDate, .021, {})};
        forward.instruments_ = {SwapNew(TODAY, TODAY, longDate, .042, leg, index, leg), SwapNew(TODAY, TODAY, shortDate, .028, leg, index, leg)};
        MultiCurveCalibrationSpec_ spec;
        spec.ccy_ = "EUR";
        spec.stages_ = {discount, forward};
        return spec;
    }

    inline RatePricingMarket_ Market(const MultiCurveCalibrationResult_& fitted) {
        RatePricingMarket_ market;
        market.resultCurrency_ = Ccy_("USD");
        market.valuationTime_ = DateTime_(TODAY, 10, 0);
        market.curveComponents_ = {{"ESTR", fitted.discountCurves_.at(CollateralType_("OIS"))}, {"Euribor6M", fitted.forwardCurves_.at(SIX_MONTHS)}};
        return market;
    }

    inline RateTradeDefinition_ SwapTrade(bool payFixed) {
        IrsTradeTerms_ terms;
        auto& swap = terms.value_;
        swap.notional_ = payFixed ? 1'000'000. : 400'000.;
        swap.payFixed_ = payFixed;
        swap.contractRate_ = payFixed ? .031 : .048;
        swap.fixedLeg_.paymentFrequency_ = swap.floatLeg_.paymentFrequency_ = SIX_MONTHS;
        swap.floatIndex_.forecastTenor_ = SIX_MONTHS;
        swap.floatIndex_.useProjectionCurve_ = true;
        swap.fixingIdentity_ = {"Euribor6M", 11, 0};
        swap.forecastComponentKey_ = "Euribor6M";
        swap.discountComponentKey_ = "ESTR";
        return {payFixed ? "payer" : "receiver",
                RateInstrumentType_::Value_::IRS,
                TODAY,
                Date::AddMonths(TODAY, 1),
                Date::AddMonths(TODAY, payFixed ? 19 : 13),
                Ccy_("EUR"),
                terms};
    }
} // namespace Dal::StagedQuoteRiskTest

//
// Created by dal-implementer on 2026/7/28.
//

#pragma once

#include <map>
#include <memory>
#include <variant>

#include <dal/curve/discount.hpp>
#include <dal/curve/xccycalibration.hpp>
#include <dal/curve/xccyinstrument.hpp>
#include <dal/indice/fixingsnapshot.hpp>
#include <dal/protocol/rateconvention.hpp>
#include <dal/time/datetime.hpp>

/*IF--------------------------------------------------------------------------
enumeration RateInstrumentType
    Closed Curve Lab pricing family registry
switchable
alternative DEPOSIT
alternative FRA
alternative FUTURE
alternative OIS
alternative IRS
alternative BASIS_SWAP
alternative XCCY
-IF-------------------------------------------------------------------------*/

namespace Dal {
#include <dal/auto/MG_RateInstrumentType_enum.hpp>

    struct DepositTradeTerms_ {
        double notional_ = 0.0;
        double contractRate_ = 0.0;
        bool lend_ = true;
        RateIndexConvention_ index_;
        String_ discountComponentKey_;
    };

    struct FraTradeTerms_ {
        double notional_ = 0.0;
        double contractRate_ = 0.0;
        bool receiveFloating_ = true;
        bool settleAtStart_ = true;
        RateIndexConvention_ index_;
        FixingIdentity_ fixingIdentity_;
        String_ forecastComponentKey_;
        String_ discountComponentKey_;
    };

    struct FutureTradeTerms_ {
        double contractCount_ = 0.0;
        bool long_ = true;
        double referencePrice_ = 0.0;
        double contractValuePerPricePoint_ = 0.0;
        double convexityAdjustment_ = 0.0;
        RateIndexConvention_ index_;
        FixingIdentity_ fixingIdentity_;
        String_ forecastComponentKey_;
    };

    struct FixedFloatTradeTerms_ {
        double notional_ = 0.0;
        double contractRate_ = 0.0;
        bool payFixed_ = true;
        RateLegConvention_ fixedLeg_;
        RateLegConvention_ floatLeg_;
        RateIndexConvention_ floatIndex_;
        FixingIdentity_ fixingIdentity_;
        String_ forecastComponentKey_;
        String_ discountComponentKey_;
    };

    struct OisTradeTerms_ {
        FixedFloatTradeTerms_ value_;
    };

    struct IrsTradeTerms_ {
        FixedFloatTradeTerms_ value_;
    };

    struct BasisTradeTerms_ {
        double notional_ = 0.0;
        double contractSpread_ = 0.0;
        bool receiveReferencePaySpread_ = true;
        RateLegConvention_ spreadLeg_;
        RateLegConvention_ referenceLeg_;
        RateIndexConvention_ spreadIndex_;
        RateIndexConvention_ referenceIndex_;
        FixingIdentity_ spreadFixingIdentity_;
        FixingIdentity_ referenceFixingIdentity_;
        String_ spreadForecastComponentKey_;
        String_ referenceForecastComponentKey_;
        String_ discountComponentKey_;
    };

    struct XccyTradeTerms_ {
        double positionCount_ = 0.0;
        double contractSpread_ = 0.0;
        bool spreadOnForeignLeg_ = true;
        bool receiveNonSpreadPaySpread_ = true;
        CrossCurrencySwapConfig_ config_;
    };

    using RateTradeTerms_ =
        std::variant<DepositTradeTerms_, FraTradeTerms_, FutureTradeTerms_, OisTradeTerms_, IrsTradeTerms_, BasisTradeTerms_, XccyTradeTerms_>;

    struct RateTradeDefinition_ {
        String_ instrumentId_;
        RateInstrumentType_ instrumentType_;
        Date_ tradeDate_;
        Date_ startDate_;
        Date_ maturityDate_;
        Ccy_ currencyOrPair_;
        RateTradeTerms_ terms_;
    };

    struct RatePricingMarket_ {
        DateTime_ valuationTime_;
        Ccy_ resultCurrency_;
        std::map<String_, Handle_<DiscountCurve_>> curveComponents_;
        std::shared_ptr<const CrossCurrencyMarket_> xccyMarket_;
        Handle_<MarketFixingSnapshot_> fixings_;
    };

    struct RateCashflowPlan_ {
        RateTradeDefinition_ trade_;
        Vector_<FixingRequest_> requiredHistoricalFixings_;
        Vector_<String_> dependencyComponentKeys_;
    };

    struct RatePricingTradeResult_ {
        String_ instrumentId_;
        RateInstrumentType_ instrumentType_;
        bool succeeded_ = false;
        double pv_ = 0.0;
        Ccy_ currency_;
        Vector_<FixingRequest_> requiredHistoricalFixings_;
        Vector_<FixingRequest_> missingHistoricalFixings_;
        Vector_<String_> dependencyComponentKeys_;
        String_ error_;
    };

    RateCashflowPlan_ BuildRateCashflowPlan(const RateTradeDefinition_& trade, const DateTime_& valuationTime);
    RatePricingTradeResult_ PriceRateTrade(const RateTradeDefinition_& trade, const RatePricingMarket_& market);
    Vector_<RatePricingTradeResult_> PriceRateTrades(const Vector_<RateTradeDefinition_>& trades, const RatePricingMarket_& market);
} // namespace Dal

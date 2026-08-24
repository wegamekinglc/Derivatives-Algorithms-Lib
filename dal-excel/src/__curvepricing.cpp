//
// Created by dal-implementer on 2026/8/24.
//

#pragma once

#include "__curve_storable.hpp"
#include "__curvepricing_test_api.hpp"
#include "__platform.hpp"
#include "__settingskeys.hpp"
#include <algorithm>
#include <cmath>
#include <dal-public/src/curvepricing.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/math/cell.hpp>
#include <dal/utilities/exceptions.hpp>

// clang-format off
/*IF--------------------------------------------------------------------------
public RateTradeHeader_New
    Create the shared header (identifier, schedule, currency) of a rate trade
&inputs
instrumentId is string
    Trade identifier
tradeDate is date
    The trade date
start is date
    The start date
maturity is date
    The maturity date
currency is string
    Trade currency code (for XCCY pass the domestic currency, the actual PV denomination)
&outputs
header is handle StorableRateTradeDefinition
    The trade header; pass it to one of the RATExxxTRADE.NEW builders
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public RateFixingIdentity_New
    Create a rate fixing publication identity
&inputs
indexName is string
    Fixing index name (e.g. "USD-LIBOR-3M")
hour is integer
    Fixing publication hour (0-23)
minute is integer
    Fixing publication minute (0-59)
&outputs
identity is handle StorableFixingIdentity
    The fixing identity
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public RateDepositTrade_New
    Create a deposit rate trade for node-sensitivity pricing
&inputs
header is handle StorableRateTradeDefinition
    The trade header (from RATETRADEHEADER.NEW)
notional is number
    Positive notional
contractRate is number
    Contract rate
lend is boolean
    True to lend (receive the maturity payment)
index is handle StorableRateIndexConvention
    The rate index convention
discountComponentKey is string
    Market component key of the discount curve
&outputs
trade is handle StorableRateTradeDefinition
    The deposit trade definition
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public RateFraTrade_New
    Create a FRA rate trade for node-sensitivity pricing
&inputs
header is handle StorableRateTradeDefinition
    The trade header (from RATETRADEHEADER.NEW)
notional is number
    Positive notional
contractRate is number
    Contract rate
receiveFloating is boolean
    True to receive the floating leg
settleAtStart is boolean
    True to settle at the period start
index is handle StorableRateIndexConvention
    The rate index convention
fixingIdentity is handle StorableFixingIdentity
    The fixing publication identity (from RATEFIXINGIDENTITY.NEW)
forecastComponentKey is string
    Market component key of the forecast curve
discountComponentKey is string
    Market component key of the discount curve
&outputs
trade is handle StorableRateTradeDefinition
    The FRA trade definition
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public RateFutureTrade_New
    Create a futures rate trade for node-sensitivity pricing
&inputs
header is handle StorableRateTradeDefinition
    The trade header (from RATETRADEHEADER.NEW)
contractCount is number
    Positive contract count
longPosition is boolean
    True when long the future
referencePrice is number
    Reference (trade) price
contractValuePerPricePoint is number
    Positive value per price point
convexityAdjustment is number
    Convexity adjustment
index is handle StorableRateIndexConvention
    The rate index convention
fixingIdentity is handle StorableFixingIdentity
    The fixing publication identity (from RATEFIXINGIDENTITY.NEW)
forecastComponentKey is string
    Market component key of the forecast curve
&outputs
trade is handle StorableRateTradeDefinition
    The futures trade definition
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public RateFixedFloatTrade_New
    Create an OIS or IRS rate trade for node-sensitivity pricing
&inputs
header is handle StorableRateTradeDefinition
    The trade header (from RATETRADEHEADER.NEW)
family is string
    Family: OIS or IRS
notional is number
    Positive notional
contractRate is number
    Fixed contract rate
payFixed is boolean
    True to pay the fixed leg
fixedLeg is handle StorableRateLegConvention
    Fixed leg convention
floatLeg is handle StorableRateLegConvention
    Float leg convention
floatIndex is handle StorableRateIndexConvention
    Float index convention
fixingIdentity is handle StorableFixingIdentity
    The fixing publication identity (from RATEFIXINGIDENTITY.NEW)
forecastComponentKey is string
    Market component key of the forecast curve
discountComponentKey is string
    Market component key of the discount curve
&outputs
trade is handle StorableRateTradeDefinition
    The fixed-float trade definition
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public RateBasisTrade_New
    Create a basis swap rate trade for node-sensitivity pricing
&inputs
header is handle StorableRateTradeDefinition
    The trade header (from RATETRADEHEADER.NEW)
notional is number
    Positive notional
contractSpread is number
    Contract spread on the spread leg
receiveReference is boolean
    True to receive the reference leg
spreadLeg is handle StorableRateLegConvention
    Spread leg convention
referenceLeg is handle StorableRateLegConvention
    Reference leg convention
spreadIndex is handle StorableRateIndexConvention
    Spread leg index convention
referenceIndex is handle StorableRateIndexConvention
    Reference leg index convention
spreadFixingIdentity is handle StorableFixingIdentity
    Spread leg fixing identity
referenceFixingIdentity is handle StorableFixingIdentity
    Reference leg fixing identity
spreadForecastComponentKey is string
    Market component key of the spread forecast curve
referenceForecastComponentKey is string
    Market component key of the reference forecast curve
discountComponentKey is string
    Market component key of the discount curve
&outputs
trade is handle StorableRateTradeDefinition
    The basis swap trade definition
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public RateXccyTrade_New
    Create a cross-currency swap rate trade for node-sensitivity pricing
&inputs
header is handle StorableRateTradeDefinition
    The trade header (from RATETRADEHEADER.NEW); its currency must be the domestic currency
positionCount is number
    Positive position count
contractSpread is number
    Contract spread on the spread leg
spreadOnForeignLeg is boolean
    True when the spread rides the foreign leg (must match the config convention)
receiveNonSpread is boolean
    True to receive the non-spread leg
config is handle StorableCrossCurrencySwapConfig
    The cross-currency swap config
&outputs
trade is handle StorableRateTradeDefinition
    The cross-currency swap trade definition
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public RatePricingMarket_New
    Assemble a rate pricing market from curve components and an optional XCCY market
&inputs
valuationTime is cell
    Valuation timestamp as an Excel date or date-time cell
resultCurrency is string
    Reported (passthrough) currency code; never the aggregation grouping key
componentKeys is string[]
    Component keys, parallel to componentCurves
componentCurves is handle[]
    Discount curve handles, parallel to componentKeys
&optional
fixings is handle StorableMarketFixingSnapshot
    Immutable fixing snapshot
domesticBlock is handle StorableCurveBlock
    XCCY domestic curve block (required for XCCY trades)
foreignBlock is handle StorableCurveBlock
    XCCY foreign curve block (required for XCCY trades)
fxSpot is number (0.0)
    Positive domestic-per-foreign FX spot
collateralCurrency is string
    XCCY collateral currency code
basisCurve is handle StorableDiscountCurve
    XCCY basis curve
&outputs
market is handle StorableRatePricingMarket
    The rate pricing market
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public RateTradeNodeSensitivitiesBatch_Spill
    Node sensitivities of a trade batch as a long-form spill
&inputs
trades is handle[]
    Rate trade definition handles (created with the RATExxxTRADE.NEW builders)
componentKeys is string[]
    One shared component key list applied to every trade
market is handle StorableRatePricingMarket
    The rate pricing market
&outputs
spill is cell[][]
    Long-form rows of trade, component, reason, pv, node, value: one row per node of each eligible
    (trade, component) cell, one reason row per failed cell. Node labels pair the parameter date
    with its free-parameter component.
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public RatePortfolioNodeRisk_Spill
    Portfolio node-risk aggregation as a long-form spill
&inputs
trades is handle[]
    Rate trade definition handles
componentKeys is string[]
    One shared component key list applied to every trade
market is handle StorableRatePricingMarket
    The rate pricing market
&outputs
spill is cell[][]
    Long-form rows of trade, component, reason, pv, node, value, currency: one row per node of each
    component's dense tensor, one aggregate row per actual PV currency carrying the
    UnconvertedByActualPvCcy policy label, and one reason row per failed (trade, component) cell.
-IF-------------------------------------------------------------------------*/
// clang-format on

namespace Dal {
    namespace {
        DateTime_ RateValuationTime(const Cell_& value) {
            if (Cell::IsDouble(value)) {
                const double serial = Cell::ToDouble(value);
                REQUIRE(std::isfinite(serial), "Rate pricing valuation time must be a finite Excel serial date");
                const int dateSerial = static_cast<int>(std::floor(serial));
                return DateTime_(Date::FromExcel(dateSerial), serial - dateSerial);
            }
            if (Cell::IsDate(value))
                return DateTime_(Cell::ToDate(value));
            return Cell::ToDateTime(value);
        }

        // Family of the terms alternative -- the immutable terms/type pairing the core validates.
        RateInstrumentType_ FamilyOfTerms(const RateTradeTerms_& terms) {
            return std::visit(
                [](const auto& value) {
                    using terms_t = std::decay_t<decltype(value)>;
                    if constexpr (std::is_same_v<terms_t, DepositTradeTerms_>)
                        return RateInstrumentType_(RateInstrumentType_::Value_::DEPOSIT);
                    else if constexpr (std::is_same_v<terms_t, FraTradeTerms_>)
                        return RateInstrumentType_(RateInstrumentType_::Value_::FRA);
                    else if constexpr (std::is_same_v<terms_t, FutureTradeTerms_>)
                        return RateInstrumentType_(RateInstrumentType_::Value_::FUTURE);
                    else if constexpr (std::is_same_v<terms_t, OisTradeTerms_>)
                        return RateInstrumentType_(RateInstrumentType_::Value_::OIS);
                    else if constexpr (std::is_same_v<terms_t, IrsTradeTerms_>)
                        return RateInstrumentType_(RateInstrumentType_::Value_::IRS);
                    else if constexpr (std::is_same_v<terms_t, BasisTradeTerms_>)
                        return RateInstrumentType_(RateInstrumentType_::Value_::BASIS_SWAP);
                    else
                        return RateInstrumentType_(RateInstrumentType_::Value_::XCCY);
                },
                terms);
        }

        // Completes a trade header with family terms: the header's identifier, schedule, and
        // currency carry over unchanged.
        StorableRateTradeDefinition_ TradeFromHeader(const Handle_<StorableRateTradeDefinition_>& header, RateTradeTerms_ terms) {
            REQUIRE(header, "Invalid rate trade header handle (created with RATETRADEHEADER.NEW)");
            RateTradeDefinition_ definition = header->val_;
            definition.terms_ = std::move(terms);
            definition.instrumentType_ = FamilyOfTerms(definition.terms_);
            return StorableRateTradeDefinition_(definition);
        }

        const FixingIdentity_& FixingIdentityFromHandle(const Handle_<StorableFixingIdentity_>& identity) {
            REQUIRE(identity, "Invalid fixing identity handle (created with RATEFIXINGIDENTITY.NEW)");
            REQUIRE(ValidFixingIdentity(identity->val_), "Rate trade fixing identity requires a name and a valid publication time");
            return identity->val_;
        }

        Vector_<RateTradeDefinition_> TradesFromHandles(const Vector_<Handle_<Storable_>>& trades) {
            Vector_<RateTradeDefinition_> result;
            result.reserve(trades.size());
            for (const auto& handle : trades) {
                const auto* trade = dynamic_cast<const StorableRateTradeDefinition_*>(handle.get());
                REQUIRE(trade, "Rate risk inputs must be rate trade definition handles (created with the RATExxxTRADE.NEW builders)");
                result.push_back(trade->val_);
            }
            return result;
        }

        // Registers the parallel key/curve arrays into the market's component map.
        void
        AddCurveComponents(const Vector_<String_>& componentKeys, const Vector_<Handle_<Storable_>>& componentCurves, RatePricingMarket_* market) {
            REQUIRE(componentKeys.size() == componentCurves.size(), "Rate pricing market component keys and curves must be parallel arrays");
            for (int index = 0; index < static_cast<int>(componentKeys.size()); ++index) {
                const auto* curve = dynamic_cast<const StorableDiscountCurve_*>(componentCurves[index].get());
                REQUIRE(curve && curve->val_, "Rate pricing market component " + componentKeys[index] + " must be a discount curve handle");
                market->curveComponents_[componentKeys[index]] = curve->val_;
            }
        }

        // Optional XCCY market: both blocks and a positive FX spot are required once any XCCY
        // input is present.
        void AddXccyMarket(const Handle_<StorableCurveBlock_>& domesticBlock,
                           const Handle_<StorableCurveBlock_>& foreignBlock,
                           double fxSpot,
                           const String_& collateralCurrency,
                           const Handle_<StorableDiscountCurve_>& basisCurve,
                           const RatePricingMarket_& market,
                           RatePricingMarket_* result) {
            const bool hasXccy = domesticBlock || foreignBlock || basisCurve;
            REQUIRE(!hasXccy || (domesticBlock && foreignBlock), "An XCCY rate pricing market needs both the domestic and foreign curve blocks");
            if (!hasXccy)
                return;
            REQUIRE(std::isfinite(fxSpot) && fxSpot > 0.0, "An XCCY rate pricing market needs a positive FX spot");
            REQUIRE(!collateralCurrency.empty(), "An XCCY rate pricing market needs a collateral currency");
            auto native = std::make_shared<CrossCurrencyMarket_>(domesticBlock->val_, foreignBlock->val_, fxSpot, market.valuationTime_,
                                                                 Ccy_(collateralCurrency), market.fixings_);
            if (basisCurve && basisCurve->val_)
                native->SetBasisCurve(basisCurve->val_);
            result->xccyMarket_ = native;
        }

        // Long-form rows per batch cell: one row per node of an eligible cell (label = parameter
        // date and free-parameter component), one reason row per failed cell.
        void WriteBatchCellRows(const RateTradeNodeSensitivityCell_& cell, const Vector_<String_>& axisLabels, Matrix_<Cell_>* spill, int* row) {
            const auto write = [&](const Cell_& reason, const Cell_& pv, const Cell_& node, const Cell_& value) {
                (*spill)(*row, 0) = Cell_(cell.instrumentId_);
                (*spill)(*row, 1) = Cell_(cell.componentKey_);
                (*spill)(*row, 2) = reason;
                (*spill)(*row, 3) = pv;
                (*spill)(*row, 4) = node;
                (*spill)(*row, 5) = value;
                ++(*row);
            };
            if (!cell.result_.eligible_) {
                write(Cell_(cell.result_.reason_), Cell_(0.0), Cell_(), Cell_());
                return;
            }
            for (int node = 0; node < static_cast<int>(axisLabels.size()); ++node)
                write(Cell_(), Cell_(cell.result_.pv_), Cell_(axisLabels[node]), Cell_(cell.result_.gradient_[node]));
        }

        // Long-form rows per aggregated component: one row per node of the dense tensor.
        void WriteComponentNodeRows(const RatePortfolioNodeRiskComponent_& component, Matrix_<Cell_>* spill, int* row) {
            if (!component.values_)
                return;
            const int nodeCount = component.values_->Size("node");
            const auto& header = component.values_->Header("node");
            Report::Address_ address = component.values_->MakeAddress();
            for (int node = 0; node < nodeCount; ++node) {
                address["node"] = node;
                const String_ label = Date::ToString(Cell::ToDate(header.values_(node, 0))) + ":" + Cell::ToString(header.values_(node, 1));
                (*spill)(*row, 0) = Cell_();
                (*spill)(*row, 1) = Cell_(component.componentKey_);
                (*spill)(*row, 2) = Cell_();
                (*spill)(*row, 3) = Cell_();
                (*spill)(*row, 4) = Cell_(label);
                (*spill)(*row, 5) = Cell_((*component.values_)[address]);
                (*spill)(*row, 6) = Cell_();
                ++(*row);
            }
        }

        // Aggregate PV rows (currency + policy label) and failure rows from the meta table.
        void WriteAggregateRows(const RatePortfolioNodeRisk_& aggregate, Matrix_<Cell_>* spill, int* row) {
            for (const auto& [ccy, pv] : aggregate.pvByActualPvCcy_) {
                (*spill)(*row, 0) = Cell_();
                (*spill)(*row, 1) = Cell_();
                (*spill)(*row, 2) = Cell_(aggregate.policy_);
                (*spill)(*row, 3) = Cell_(pv);
                (*spill)(*row, 4) = Cell_();
                (*spill)(*row, 5) = Cell_();
                (*spill)(*row, 6) = Cell_(ccy);
                ++(*row);
            }
            for (const auto& meta : aggregate.meta_) {
                if (meta.eligible_)
                    continue;
                (*spill)(*row, 0) = Cell_(meta.instrumentId_);
                (*spill)(*row, 1) = Cell_(meta.componentKey_);
                (*spill)(*row, 2) = Cell_(meta.reason_);
                (*spill)(*row, 3) = Cell_();
                (*spill)(*row, 4) = Cell_();
                (*spill)(*row, 5) = Cell_();
                (*spill)(*row, 6) = Cell_(meta.actualPvCcy_.String());
                ++(*row);
            }
        }
    } // namespace

    void RateTradeHeader_New(const String_& instrumentId,
                             const Date_& tradeDate,
                             const Date_& start,
                             const Date_& maturity,
                             const String_& currency,
                             Handle_<StorableRateTradeDefinition_>* header) {
        RateTradeDefinition_ definition;
        definition.instrumentId_ = instrumentId;
        definition.tradeDate_ = tradeDate;
        definition.startDate_ = start;
        definition.maturityDate_ = maturity;
        definition.currencyOrPair_ = Ccy_(currency);
        header->reset(new StorableRateTradeDefinition_(definition));
    }

    void RateFixingIdentity_New(const String_& indexName, int hour, int minute, Handle_<StorableFixingIdentity_>* identity) {
        FixingIdentity_ value;
        value.indexName_ = indexName;
        value.fixingHour_ = hour;
        value.fixingMinute_ = minute;
        REQUIRE(ValidFixingIdentity(value), "Rate trade fixing identity requires a name and a valid publication time");
        identity->reset(new StorableFixingIdentity_(value));
    }

    void RateDepositTrade_New(const Handle_<StorableRateTradeDefinition_>& header,
                              double notional,
                              double contractRate,
                              bool lend,
                              const Handle_<StorableRateIndexConvention_>& index,
                              const String_& discountComponentKey,
                              Handle_<StorableRateTradeDefinition_>* trade) {
        REQUIRE(index, "Invalid rate index convention handle");
        DepositTradeTerms_ terms;
        terms.notional_ = notional;
        terms.contractRate_ = contractRate;
        terms.lend_ = lend;
        terms.index_ = index->val_;
        terms.discountComponentKey_ = discountComponentKey;
        trade->reset(new StorableRateTradeDefinition_(TradeFromHeader(header, std::move(terms))));
    }

    void RateFraTrade_New(const Handle_<StorableRateTradeDefinition_>& header,
                          double notional,
                          double contractRate,
                          bool receiveFloating,
                          bool settleAtStart,
                          const Handle_<StorableRateIndexConvention_>& index,
                          const Handle_<StorableFixingIdentity_>& fixingIdentity,
                          const String_& forecastComponentKey,
                          const String_& discountComponentKey,
                          Handle_<StorableRateTradeDefinition_>* trade) {
        REQUIRE(index, "Invalid rate index convention handle");
        FraTradeTerms_ terms;
        terms.notional_ = notional;
        terms.contractRate_ = contractRate;
        terms.receiveFloating_ = receiveFloating;
        terms.settleAtStart_ = settleAtStart;
        terms.index_ = index->val_;
        terms.fixingIdentity_ = FixingIdentityFromHandle(fixingIdentity);
        terms.forecastComponentKey_ = forecastComponentKey;
        terms.discountComponentKey_ = discountComponentKey;
        trade->reset(new StorableRateTradeDefinition_(TradeFromHeader(header, std::move(terms))));
    }

    void RateFutureTrade_New(const Handle_<StorableRateTradeDefinition_>& header,
                             double contractCount,
                             bool longPosition,
                             double referencePrice,
                             double contractValuePerPricePoint,
                             double convexityAdjustment,
                             const Handle_<StorableRateIndexConvention_>& index,
                             const Handle_<StorableFixingIdentity_>& fixingIdentity,
                             const String_& forecastComponentKey,
                             Handle_<StorableRateTradeDefinition_>* trade) {
        REQUIRE(index, "Invalid rate index convention handle");
        FutureTradeTerms_ terms;
        terms.contractCount_ = contractCount;
        terms.long_ = longPosition;
        terms.referencePrice_ = referencePrice;
        terms.contractValuePerPricePoint_ = contractValuePerPricePoint;
        terms.convexityAdjustment_ = convexityAdjustment;
        terms.index_ = index->val_;
        terms.fixingIdentity_ = FixingIdentityFromHandle(fixingIdentity);
        terms.forecastComponentKey_ = forecastComponentKey;
        trade->reset(new StorableRateTradeDefinition_(TradeFromHeader(header, std::move(terms))));
    }

    void RateFixedFloatTrade_New(const Handle_<StorableRateTradeDefinition_>& header,
                                 const String_& family,
                                 double notional,
                                 double contractRate,
                                 bool payFixed,
                                 const Handle_<StorableRateLegConvention_>& fixedLeg,
                                 const Handle_<StorableRateLegConvention_>& floatLeg,
                                 const Handle_<StorableRateIndexConvention_>& floatIndex,
                                 const Handle_<StorableFixingIdentity_>& fixingIdentity,
                                 const String_& forecastComponentKey,
                                 const String_& discountComponentKey,
                                 Handle_<StorableRateTradeDefinition_>* trade) {
        REQUIRE(fixedLeg && floatLeg && floatIndex, "Invalid fixed-float leg or index convention handle");
        const RateInstrumentType_ type(family);
        REQUIRE(type == RateInstrumentType_(RateInstrumentType_::Value_::OIS) || type == RateInstrumentType_(RateInstrumentType_::Value_::IRS),
                "RateFixedFloatTrade_New family must be OIS or IRS");
        FixedFloatTradeTerms_ terms;
        terms.notional_ = notional;
        terms.contractRate_ = contractRate;
        terms.payFixed_ = payFixed;
        terms.fixedLeg_ = fixedLeg->val_;
        terms.floatLeg_ = floatLeg->val_;
        terms.floatIndex_ = floatIndex->val_;
        terms.fixingIdentity_ = FixingIdentityFromHandle(fixingIdentity);
        terms.forecastComponentKey_ = forecastComponentKey;
        terms.discountComponentKey_ = discountComponentKey;
        const RateTradeTerms_ wrapped = type == RateInstrumentType_(RateInstrumentType_::Value_::OIS) ? RateTradeTerms_(OisTradeTerms_{terms})
                                                                                                      : RateTradeTerms_(IrsTradeTerms_{terms});
        trade->reset(new StorableRateTradeDefinition_(TradeFromHeader(header, wrapped)));
    }

    void RateBasisTrade_New(const Handle_<StorableRateTradeDefinition_>& header,
                            double notional,
                            double contractSpread,
                            bool receiveReference,
                            const Handle_<StorableRateLegConvention_>& spreadLeg,
                            const Handle_<StorableRateLegConvention_>& referenceLeg,
                            const Handle_<StorableRateIndexConvention_>& spreadIndex,
                            const Handle_<StorableRateIndexConvention_>& referenceIndex,
                            const Handle_<StorableFixingIdentity_>& spreadFixingIdentity,
                            const Handle_<StorableFixingIdentity_>& referenceFixingIdentity,
                            const String_& spreadForecastComponentKey,
                            const String_& referenceForecastComponentKey,
                            const String_& discountComponentKey,
                            Handle_<StorableRateTradeDefinition_>* trade) {
        REQUIRE(spreadLeg && referenceLeg && spreadIndex && referenceIndex, "Invalid basis swap leg or index convention handle");
        BasisTradeTerms_ terms;
        terms.notional_ = notional;
        terms.contractSpread_ = contractSpread;
        terms.receiveReferencePaySpread_ = receiveReference;
        terms.spreadLeg_ = spreadLeg->val_;
        terms.referenceLeg_ = referenceLeg->val_;
        terms.spreadIndex_ = spreadIndex->val_;
        terms.referenceIndex_ = referenceIndex->val_;
        terms.spreadFixingIdentity_ = FixingIdentityFromHandle(spreadFixingIdentity);
        terms.referenceFixingIdentity_ = FixingIdentityFromHandle(referenceFixingIdentity);
        terms.spreadForecastComponentKey_ = spreadForecastComponentKey;
        terms.referenceForecastComponentKey_ = referenceForecastComponentKey;
        terms.discountComponentKey_ = discountComponentKey;
        trade->reset(new StorableRateTradeDefinition_(TradeFromHeader(header, std::move(terms))));
    }

    void RateXccyTrade_New(const Handle_<StorableRateTradeDefinition_>& header,
                           double positionCount,
                           double contractSpread,
                           bool spreadOnForeignLeg,
                           bool receiveNonSpread,
                           const Handle_<StorableCrossCurrencySwapConfig_>& config,
                           Handle_<StorableRateTradeDefinition_>* trade) {
        REQUIRE(config, "Invalid cross-currency swap config handle");
        XccyTradeTerms_ terms;
        terms.positionCount_ = positionCount;
        terms.contractSpread_ = contractSpread;
        terms.spreadOnForeignLeg_ = spreadOnForeignLeg;
        terms.receiveNonSpreadPaySpread_ = receiveNonSpread;
        terms.config_ = config->val_;
        REQUIRE(header && header->val_.currencyOrPair_ == terms.config_.pair_.domestic_,
                "RateXccyTrade_New header currency must be the config's domestic currency (the actual PV denomination)");
        trade->reset(new StorableRateTradeDefinition_(TradeFromHeader(header, std::move(terms))));
    }

    void RatePricingMarket_New(const Cell_& valuationTime,
                               const String_& resultCurrency,
                               const Vector_<String_>& componentKeys,
                               const Vector_<Handle_<Storable_>>& componentCurves,
                               const Handle_<StorableMarketFixingSnapshot_>& fixings,
                               const Handle_<StorableCurveBlock_>& domesticBlock,
                               const Handle_<StorableCurveBlock_>& foreignBlock,
                               double fxSpot,
                               const String_& collateralCurrency,
                               const Handle_<StorableDiscountCurve_>& basisCurve,
                               Handle_<StorableRatePricingMarket_>* market) {
        RatePricingMarket_ result;
        result.valuationTime_ = RateValuationTime(valuationTime);
        result.resultCurrency_ = Ccy_(resultCurrency);
        AddCurveComponents(componentKeys, componentCurves, &result);
        if (fixings)
            result.fixings_ = fixings->val_;
        AddXccyMarket(domesticBlock, foreignBlock, fxSpot, collateralCurrency, basisCurve, result, &result);
        market->reset(new StorableRatePricingMarket_(result));
    }

    void RateTradeNodeSensitivitiesBatch_Spill(const Vector_<Handle_<Storable_>>& trades,
                                               const Vector_<String_>& componentKeys,
                                               const Handle_<StorableRatePricingMarket_>& market,
                                               Matrix_<Cell_>* spill) {
        REQUIRE(market, "Invalid rate pricing market handle");
        const auto cells = RateTradeNodeSensitivitiesBatch(TradesFromHandles(trades), market->val_, componentKeys);

        std::map<String_, Vector_<String_>> axisLabels;
        int rowCount = 0;
        for (const auto& cell : cells) {
            if (!cell.result_.eligible_) {
                rowCount += 1;
                continue;
            }
            auto found = axisLabels.find(cell.componentKey_);
            if (found == axisLabels.end())
                found = axisLabels.emplace(cell.componentKey_, RateNodeSensitivityAxisLabels(market->val_, cell.componentKey_)).first;
            rowCount += static_cast<int>(found->second.size());
        }

        spill->Resize(rowCount, 6);
        int row = 0;
        for (const auto& cell : cells)
            WriteBatchCellRows(cell, cell.result_.eligible_ ? axisLabels.at(cell.componentKey_) : Vector_<String_>(), spill, &row);
    }

    void RatePortfolioNodeRisk_Spill(const Vector_<Handle_<Storable_>>& trades,
                                     const Vector_<String_>& componentKeys,
                                     const Handle_<StorableRatePricingMarket_>& market,
                                     Matrix_<Cell_>* spill) {
        REQUIRE(market, "Invalid rate pricing market handle");
        const auto aggregate = AggregateRatePortfolioNodeRisk(TradesFromHandles(trades), market->val_, componentKeys);

        int rowCount = 0;
        for (const auto& component : aggregate.components_)
            rowCount += component.values_ ? component.values_->Size("node") : 0;
        rowCount += static_cast<int>(aggregate.pvByActualPvCcy_.size());
        for (const auto& meta : aggregate.meta_)
            if (!meta.eligible_)
                rowCount += 1;

        spill->Resize(rowCount, 7);
        int row = 0;
        for (const auto& component : aggregate.components_)
            WriteComponentNodeRows(component, spill, &row);
        WriteAggregateRows(aggregate, spill, &row);
    }

    // clang-format off
#ifdef _WIN32
#include <dal-excel/auto/MG_RateTradeHeader_New_public.inc>
#include <dal-excel/auto/MG_RateFixingIdentity_New_public.inc>
#include <dal-excel/auto/MG_RateDepositTrade_New_public.inc>
#include <dal-excel/auto/MG_RateFraTrade_New_public.inc>
#include <dal-excel/auto/MG_RateFutureTrade_New_public.inc>
#include <dal-excel/auto/MG_RateFixedFloatTrade_New_public.inc>
#include <dal-excel/auto/MG_RateBasisTrade_New_public.inc>
#include <dal-excel/auto/MG_RateXccyTrade_New_public.inc>
#include <dal-excel/auto/MG_RatePricingMarket_New_public.inc>
#include <dal-excel/auto/MG_RateTradeNodeSensitivitiesBatch_Spill_public.inc>
#include <dal-excel/auto/MG_RatePortfolioNodeRisk_Spill_public.inc>
#endif
    // clang-format on
} // namespace Dal

//
// Created by dal-implementer on 2026/8/25.
//

#pragma once

#include "__curve_storable.hpp"

#if defined(_WIN32) && defined(DAL_EXCEL_TEST_API_EXPORTS)
#define DAL_EXCEL_TEST_API __declspec(dllexport)
#elif defined(_WIN32) && defined(DAL_EXCEL_TEST_API_IMPORTS)
#define DAL_EXCEL_TEST_API __declspec(dllimport)
#else
#define DAL_EXCEL_TEST_API
#endif

namespace Dal {
    struct StorableRateQuoteRiskProvenance_;

    DAL_EXCEL_TEST_API void RateTradeHeader_New(const String_& instrumentId,
                                                const Date_& tradeDate,
                                                const Date_& start,
                                                const Date_& maturity,
                                                const String_& currency,
                                                Handle_<StorableRateTradeDefinition_>* header);

    DAL_EXCEL_TEST_API void RateFixingIdentity_New(const String_& indexName, int hour, int minute, Handle_<StorableFixingIdentity_>* identity);

    DAL_EXCEL_TEST_API void RateDepositTrade_New(const Handle_<StorableRateTradeDefinition_>& header,
                                                 double notional,
                                                 double contractRate,
                                                 bool lend,
                                                 const Handle_<StorableRateIndexConvention_>& index,
                                                 const String_& discountComponentKey,
                                                 Handle_<StorableRateTradeDefinition_>* trade);

    DAL_EXCEL_TEST_API void RateFraTrade_New(const Handle_<StorableRateTradeDefinition_>& header,
                                             double notional,
                                             double contractRate,
                                             bool receiveFloating,
                                             bool settleAtStart,
                                             const Handle_<StorableRateIndexConvention_>& index,
                                             const Handle_<StorableFixingIdentity_>& fixingIdentity,
                                             const String_& forecastComponentKey,
                                             const String_& discountComponentKey,
                                             Handle_<StorableRateTradeDefinition_>* trade);

    DAL_EXCEL_TEST_API void RateFutureTrade_New(const Handle_<StorableRateTradeDefinition_>& header,
                                                double contractCount,
                                                bool longPosition,
                                                double referencePrice,
                                                double contractValuePerPricePoint,
                                                double convexityAdjustment,
                                                const Handle_<StorableRateIndexConvention_>& index,
                                                const Handle_<StorableFixingIdentity_>& fixingIdentity,
                                                const String_& forecastComponentKey,
                                                Handle_<StorableRateTradeDefinition_>* trade);

    DAL_EXCEL_TEST_API void RateFixedFloatTrade_New(const Handle_<StorableRateTradeDefinition_>& header,
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
                                                    Handle_<StorableRateTradeDefinition_>* trade);

    DAL_EXCEL_TEST_API void RateBasisTrade_New(const Handle_<StorableRateTradeDefinition_>& header,
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
                                               Handle_<StorableRateTradeDefinition_>* trade);

    DAL_EXCEL_TEST_API void RateXccyTrade_New(const Handle_<StorableRateTradeDefinition_>& header,
                                              double positionCount,
                                              double contractSpread,
                                              bool spreadOnForeignLeg,
                                              bool receiveNonSpread,
                                              const Handle_<StorableCrossCurrencySwapConfig_>& config,
                                              Handle_<StorableRateTradeDefinition_>* trade);

    DAL_EXCEL_TEST_API void RatePricingMarket_New(const Cell_& valuationTime,
                                                  const String_& resultCurrency,
                                                  const Vector_<String_>& componentKeys,
                                                  const Vector_<Handle_<Storable_>>& componentCurves,
                                                  const Handle_<StorableMarketFixingSnapshot_>& fixings,
                                                  const Handle_<StorableCurveBlock_>& domesticBlock,
                                                  const Handle_<StorableCurveBlock_>& foreignBlock,
                                                  double fxSpot,
                                                  const String_& collateralCurrency,
                                                  const Handle_<StorableDiscountCurve_>& basisCurve,
                                                  Handle_<StorableRatePricingMarket_>* market);

    DAL_EXCEL_TEST_API void RateTradeNodeSensitivitiesBatch_Spill(const Vector_<Handle_<Storable_>>& trades,
                                                                  const Vector_<String_>& componentKeys,
                                                                  const Handle_<StorableRatePricingMarket_>& market,
                                                                  Matrix_<Cell_>* spill);

    DAL_EXCEL_TEST_API void RatePortfolioNodeRisk_Spill(const Vector_<Handle_<Storable_>>& trades,
                                                        const Vector_<String_>& componentKeys,
                                                        const Handle_<StorableRatePricingMarket_>& market,
                                                        Matrix_<Cell_>* spill);

    DAL_EXCEL_TEST_API void RatePortfolioQuoteRisk_Spill(const Vector_<Handle_<Storable_>>& trades,
                                                         const Handle_<StorableRatePricingMarket_>& market,
                                                         const Vector_<Handle_<Storable_>>& provenances,
                                                         Matrix_<Cell_>* spill);

    DAL_EXCEL_TEST_API void SingleCurveQuoteRiskProvenance_New(const Handle_<StorableCurveCalibrationResult_>& result,
                                                               const String_& calibrationId,
                                                               const Vector_<String_>& parameterBlockKeys,
                                                               const Vector_<String_>& componentKeys,
                                                               const Handle_<StorableRatePricingMarket_>& market,
                                                               Handle_<StorableRateQuoteRiskProvenance_>* provenance);

    DAL_EXCEL_TEST_API void RateQuoteRiskProvenance_New(const Handle_<Storable_>& result,
                                                        const String_& calibrationId,
                                                        const Vector_<String_>& parameterBlockKeys,
                                                        const Vector_<String_>& componentKeys,
                                                        const Handle_<StorableRatePricingMarket_>& market,
                                                        Handle_<StorableRateQuoteRiskProvenance_>* provenance);

    DAL_EXCEL_TEST_API void JointXccyQuoteRiskProvenance_New(const Handle_<StorableJointXccyCalibrationResult_>& result,
                                                             const String_& calibrationId,
                                                             const Vector_<String_>& parameterBlockKeys,
                                                             const Vector_<String_>& componentKeys,
                                                             const Handle_<StorableRatePricingMarket_>& market,
                                                             Handle_<StorableRateQuoteRiskProvenance_>* provenance);

    DAL_EXCEL_TEST_API void StagedXccyBasisQuoteRiskProvenance_New(const Handle_<StorableCrossCurrencyCalibrationResult_>& result,
                                                                   const String_& calibrationId,
                                                                   const Vector_<String_>& parameterBlockKeys,
                                                                   const Vector_<String_>& componentKeys,
                                                                   const Handle_<StorableRatePricingMarket_>& market,
                                                                   Handle_<StorableRateQuoteRiskProvenance_>* provenance);
} // namespace Dal

#undef DAL_EXCEL_TEST_API

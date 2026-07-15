//
// Created by Codex on 2026/7/14.
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
    DAL_EXCEL_TEST_API void XccyResetConvention_New(int fixingLag,
                                                    const String_& fixingHolidays,
                                                    const String_& fixingConvention,
                                                    int fixingHour,
                                                    int fixingMinute,
                                                    Handle_<StorableFxResetConvention_>* resetConvention);

    DAL_EXCEL_TEST_API void MarketFixingSnapshot_New(const Vector_<String_>& indexNames,
                                                     const Vector_<Cell_>& fixingTimes,
                                                     const Vector_<>& values,
                                                     Handle_<StorableMarketFixingSnapshot_>* snapshot);

    DAL_EXCEL_TEST_API void CrossCurrencySwapConfig_New(const Handle_<StorableCurrencyPair_>& currencies,
                                                        const Handle_<StorableRateLegConvention_>& domesticLeg,
                                                        const Handle_<StorableRateIndexConvention_>& domesticIndex,
                                                        const Handle_<StorableRateLegConvention_>& foreignLeg,
                                                        const Handle_<StorableRateIndexConvention_>& foreignIndex,
                                                        const Handle_<StorableFxResetConvention_>& resetConvention,
                                                        const String_& notionalMode,
                                                        const String_& domesticRateIndex,
                                                        int domesticRateFixingHour,
                                                        int domesticRateFixingMinute,
                                                        const String_& foreignRateIndex,
                                                        int foreignRateFixingHour,
                                                        int foreignRateFixingMinute,
                                                        double domesticNotional,
                                                        double foreignNotional,
                                                        Handle_<StorableCrossCurrencySwapConfig_>* config);

    DAL_EXCEL_TEST_API void CrossCurrencySwap_Config_New(const Date_& tradeDate,
                                                         const Date_& start,
                                                         const Date_& maturity,
                                                         double marketRate,
                                                         const Handle_<StorableCrossCurrencySwapConfig_>& config,
                                                         Handle_<StorableCrossCurrencySwap_>* instrument);

    DAL_EXCEL_TEST_API void Calibrate_JointXccy(const Cell_& valuationTime,
                                                const Handle_<StorableCurrencyPair_>& currencies,
                                                const String_& collateralCurrency,
                                                double fxSpot,
                                                const Vector_<Handle_<Storable_>>& domesticInstruments,
                                                const Vector_<Date_>& domesticKnotDates,
                                                const Vector_<Handle_<Storable_>>& foreignInstruments,
                                                const Vector_<Date_>& foreignKnotDates,
                                                const Vector_<Handle_<Storable_>>& basisInstruments,
                                                const Vector_<Date_>& basisKnotDates,
                                                const Handle_<StorableMarketFixingSnapshot_>& fixings,
                                                const Matrix_<Cell_>& settings,
                                                Handle_<StorableJointXccyCalibrationResult_>* result);

    DAL_EXCEL_TEST_API void JointXccyCalibrationResult_Get_DomesticBlock(const Handle_<StorableJointXccyCalibrationResult_>& result,
                                                                         Handle_<StorableCurveBlock_>* block);

    DAL_EXCEL_TEST_API void JointXccyCalibrationResult_Get_ForeignBlock(const Handle_<StorableJointXccyCalibrationResult_>& result,
                                                                        Handle_<StorableCurveBlock_>* block);

    DAL_EXCEL_TEST_API void JointXccyCalibrationResult_Get_BasisCurve(const Handle_<StorableJointXccyCalibrationResult_>& result,
                                                                      Handle_<StorableDiscountCurve_>* curve);

    DAL_EXCEL_TEST_API void
    JointXccyCalibrationResult_Get(const Handle_<StorableJointXccyCalibrationResult_>& result, const String_& attribute, Matrix_<Cell_>* value);
} // namespace Dal

#undef DAL_EXCEL_TEST_API

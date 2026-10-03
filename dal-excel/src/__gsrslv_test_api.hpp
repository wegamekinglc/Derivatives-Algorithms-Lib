//
// Created by Codex on 2026/10/2.
//

#pragma once

#include "__gsr_test_api.hpp"
#include "__curve_storable.hpp"

#if defined(_WIN32) && defined(DAL_EXCEL_TEST_API_EXPORTS)
#define DAL_EXCEL_GSRSLV_API __declspec(dllexport)
#elif defined(_WIN32) && defined(DAL_EXCEL_TEST_API_IMPORTS)
#define DAL_EXCEL_GSRSLV_API __declspec(dllimport)
#else
#define DAL_EXCEL_GSRSLV_API
#endif

namespace Dal {
    using StorableVolQuote_ = GSRValueHandle_<VolQuote_>;
    using StorableGSRSLVCalibrationResult_ = GSRValueHandle_<GSRSLVCalibrationResult_>;
    using StorableGSRSLVQuoteRiskResult_ = GSRValueHandle_<GSRSLVQuoteRiskResult_>;
    using StorableGSRCurveQuoteRisk_ = GSRValueHandle_<GSRCurveQuoteRisk_>;

    DAL_EXCEL_GSRSLV_API void VolQuote_New(const String_& name,
                                           const Handle_<StorableEuropeanRateOption_>& option,
                                           double volatility,
                                           double priceScale,
                                           const String_& convention,
                                           double shift,
                                           Handle_<StorableVolQuote_>* quote);
    DAL_EXCEL_GSRSLV_API void
    VolQuotes_Get_Prices(const Handle_<GSRCurveData_>& snapshot, const Vector_<Handle_<Storable_>>& quotes, Matrix_<Cell_>* result);
    DAL_EXCEL_GSRSLV_API void Calibrate_GSRSLVMarket(const Handle_<ModelData_>& initial,
                                                     const Vector_<Handle_<Storable_>>& quotes,
                                                     const Matrix_<Cell_>& parameters,
                                                     const Matrix_<Cell_>& settings,
                                                     const Vector_<Handle_<Storable_>>& heldOut,
                                                     Handle_<StorableGSRSLVCalibrationResult_>* result);
    DAL_EXCEL_GSRSLV_API void GSRSLV_MarketQuoteRisk(const Handle_<ModelData_>& initial,
                                                     const Vector_<Handle_<Storable_>>& quotes,
                                                     const Matrix_<Cell_>& parameters,
                                                     const Vector_<Handle_<Storable_>>& targets,
                                                     const Matrix_<Cell_>& settings,
                                                     const Matrix_<Cell_>& riskSettings,
                                                     const Handle_<StorableGSRCurveQuoteRisk_>& curveRisk,
                                                     Handle_<StorableGSRSLVQuoteRiskResult_>* result);

    DAL_EXCEL_GSRSLV_API void GSRSLV_EuropeanOptionPrices(const Handle_<ModelData_>& model,
                                                          const Vector_<Handle_<Storable_>>& options,
                                                          const Matrix_<Cell_>& settings,
                                                          Matrix_<Cell_>* result);
    DAL_EXCEL_GSRSLV_API void Calibrate_GSRSLV(const Handle_<ModelData_>& initial,
                                               const Vector_<Handle_<Storable_>>& quotes,
                                               const Matrix_<Cell_>& parameters,
                                               const Matrix_<Cell_>& settings,
                                               const Vector_<Handle_<Storable_>>& heldOut,
                                               Handle_<StorableGSRSLVCalibrationResult_>* result);
    DAL_EXCEL_GSRSLV_API void
    GSRSLVCalibrationResult_Get(const Handle_<StorableGSRSLVCalibrationResult_>& result, const String_& attribute, Matrix_<Cell_>* value);
    DAL_EXCEL_GSRSLV_API void GSRSLVCalibrationResult_Get_Model(const Handle_<StorableGSRSLVCalibrationResult_>& result, Handle_<ModelData_>* model);
    DAL_EXCEL_GSRSLV_API void GSRSLV_QuoteRisk(const Handle_<ModelData_>& initial,
                                               const Vector_<Handle_<Storable_>>& quotes,
                                               const Matrix_<Cell_>& parameters,
                                               const Vector_<Handle_<Storable_>>& targets,
                                               const Matrix_<Cell_>& settings,
                                               const Matrix_<Cell_>& riskSettings,
                                               const Handle_<StorableGSRCurveQuoteRisk_>& curveRisk,
                                               Handle_<StorableGSRSLVQuoteRiskResult_>* result);
    DAL_EXCEL_GSRSLV_API void
    GSRSLVQuoteRiskResult_Get(const Handle_<StorableGSRSLVQuoteRiskResult_>& result, const String_& attribute, Matrix_<Cell_>* value);
    DAL_EXCEL_GSRSLV_API void GSRSLVQuoteRiskResult_Get_Calibration(const Handle_<StorableGSRSLVQuoteRiskResult_>& result,
                                                                    Handle_<StorableGSRSLVCalibrationResult_>* calibration);
    DAL_EXCEL_GSRSLV_API void GSRCurveQuoteRisk_New(const Handle_<GSRCurveData_>& snapshot,
                                                    const Handle_<StorableRatePricingMarket_>& market,
                                                    const Handle_<StorableRateQuoteRiskProvenance_>& provenance,
                                                    const String_& discountComponent,
                                                    const Vector_<String_>& projectionComponents,
                                                    Handle_<StorableGSRCurveQuoteRisk_>* result);
    DAL_EXCEL_GSRSLV_API void
    GSRCurveQuoteRisk_Get(const Handle_<StorableGSRCurveQuoteRisk_>& result, const String_& attribute, Matrix_<Cell_>* value);
} // namespace Dal

#undef DAL_EXCEL_GSRSLV_API

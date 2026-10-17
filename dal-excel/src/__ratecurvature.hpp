//
// Created by Codex on 2026/10/11.
//

#pragma once

#include <dal-excel/src/__riskrequest.hpp>
#include <dal-public/src/ratecurvature.hpp>

#include "__curvaturerequest.hpp"

#if defined(_WIN32) && defined(DAL_EXCEL_TEST_API_EXPORTS)
#define DAL_RATE_CURVATURE_API __declspec(dllexport)
#elif defined(_WIN32) && defined(DAL_EXCEL_TEST_API_IMPORTS)
#define DAL_RATE_CURVATURE_API __declspec(dllimport)
#else
#define DAL_RATE_CURVATURE_API
#endif

namespace Dal {
    inline const char* RiskValueType(const RateCalibrationSnapshot_&) { return "RateCalibrationSnapshot"; }
    inline const char* RiskValueType(const RateTradeQuoteCurvatureSettings_&) { return "RateTradeQuoteCurvatureSettings"; }
    inline const char* RiskValueType(const RateTradeQuoteCurvatureResult_&) { return "RateTradeQuoteCurvatureResult"; }
    using StorableRateCalibrationSnapshot_ = Excel::StorableRiskValue_<RateCalibrationSnapshot_>;
    using StorableRateTradeQuoteCurvatureSettings_ = Excel::StorableRiskValue_<RateTradeQuoteCurvatureSettings_>;
    using StorableRateTradeQuoteCurvatureResult_ = Excel::StorableRiskValue_<RateTradeQuoteCurvatureResult_>;

    DAL_RATE_CURVATURE_API void RateCalibration_New(const String_&, const Handle_<Storable_>&, Handle_<StorableRateCalibrationSnapshot_>*);
    DAL_RATE_CURVATURE_API void RateCalibration_Recalibrate(const String_&,
                                                            const Handle_<StorableRateCalibrationSnapshot_>&,
                                                            const Matrix_<Cell_>&,
                                                            Handle_<StorableRateCalibrationSnapshot_>*);
    DAL_RATE_CURVATURE_API void RateCalibration_Get_Point(const Handle_<StorableRateCalibrationSnapshot_>&, Matrix_<Cell_>*);
    DAL_RATE_CURVATURE_API void RateCalibration_Get_Parameters(const Handle_<StorableRateCalibrationSnapshot_>&, Matrix_<Cell_>*);
    DAL_RATE_CURVATURE_API void RateCalibration_Get_QuotePlan(const Handle_<StorableRateCalibrationSnapshot_>&,
                                                              Handle_<StorableCalibrationRiskPlan_>*);
    DAL_RATE_CURVATURE_API void RateTradeQuoteCurvatureSettings_New(const String_&,
                                                                    const Matrix_<Cell_>&,
                                                                    const Handle_<StorableMarketFixingSnapshot_>&,
                                                                    Handle_<StorableRateTradeQuoteCurvatureSettings_>*);
    DAL_RATE_CURVATURE_API void RateTradeQuoteCurvatureSettings_Get_Weights(const Handle_<StorableRateTradeQuoteCurvatureSettings_>&,
                                                                            Matrix_<Cell_>*);
    DAL_RATE_CURVATURE_API void RateTradeQuoteCurvatureSettings_Get_Fixings(const Handle_<StorableRateTradeQuoteCurvatureSettings_>&,
                                                                            Matrix_<Cell_>*);
    DAL_RATE_CURVATURE_API void RateTradeQuoteCurvatureResult_New(const String_&,
                                                                  const Vector_<Handle_<Storable_>>&,
                                                                  const Handle_<StorableRateCalibrationSnapshot_>&,
                                                                  const Handle_<StorableBumpOverAADRequest_>&,
                                                                  const Handle_<StorableRateTradeQuoteCurvatureSettings_>&,
                                                                  Handle_<StorableRateTradeQuoteCurvatureResult_>*);
    DAL_RATE_CURVATURE_API void RateTradeQuoteCurvatureResult_Get_Value(const Handle_<StorableRateTradeQuoteCurvatureResult_>&, double*);
    DAL_RATE_CURVATURE_API void RateTradeQuoteCurvatureResult_Get_Currency(const Handle_<StorableRateTradeQuoteCurvatureResult_>&, String_*);
    DAL_RATE_CURVATURE_API void RateTradeQuoteCurvatureResult_Get_Point(const Handle_<StorableRateTradeQuoteCurvatureResult_>&, Matrix_<Cell_>*);
    DAL_RATE_CURVATURE_API void RateTradeQuoteCurvatureResult_Get_Gradient(const Handle_<StorableRateTradeQuoteCurvatureResult_>&, Matrix_<Cell_>*);
    DAL_RATE_CURVATURE_API void RateTradeQuoteCurvatureResult_Get_Directions(const Handle_<StorableRateTradeQuoteCurvatureResult_>&, Matrix_<Cell_>*);
    DAL_RATE_CURVATURE_API void RateTradeQuoteCurvatureResult_Get_Steps(const Handle_<StorableRateTradeQuoteCurvatureResult_>&, Matrix_<Cell_>*);
    DAL_RATE_CURVATURE_API void RateTradeQuoteCurvatureResult_Get_HessianProducts(const Handle_<StorableRateTradeQuoteCurvatureResult_>&,
                                                                                  Matrix_<Cell_>*);
    DAL_RATE_CURVATURE_API void RateTradeQuoteCurvatureResult_Get_Shape(const Handle_<StorableRateTradeQuoteCurvatureResult_>&, Matrix_<Cell_>*);
    DAL_RATE_CURVATURE_API void RateTradeQuoteCurvatureResult_Get_Execution(const Handle_<StorableRateTradeQuoteCurvatureResult_>&, Matrix_<Cell_>*);
    DAL_RATE_CURVATURE_API void RateTradeQuoteCurvatureResult_Get_BaseCalibration(const Handle_<StorableRateTradeQuoteCurvatureResult_>&,
                                                                                  Handle_<StorableRateCalibrationSnapshot_>*);
} // namespace Dal

#undef DAL_RATE_CURVATURE_API

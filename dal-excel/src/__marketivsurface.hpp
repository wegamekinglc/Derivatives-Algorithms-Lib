#pragma once

#include "__risk.hpp"
#include <dal-public/src/marketivsurface.hpp>

#if defined(_WIN32) && defined(DAL_EXCEL_TEST_API_EXPORTS)
#define DAL_MARKET_IV_API __declspec(dllexport)
#elif defined(_WIN32) && defined(DAL_EXCEL_TEST_API_IMPORTS)
#define DAL_MARKET_IV_API __declspec(dllimport)
#else
#define DAL_MARKET_IV_API
#endif

namespace Dal {
    inline const char* RiskValueType(const MarketIvMarket_&) { return "MarketIvMarket"; }
    using StorableMarketIvMarket_ = Excel::StorableRiskValue_<MarketIvMarket_>;
    inline const char* RiskValueType(const MarketIvFitSettings_&) { return "MarketIvFitSettings"; }
    using StorableMarketIvFitSettings_ = Excel::StorableRiskValue_<MarketIvFitSettings_>;
    inline const char* RiskValueType(const MarketIvEvaluationSettings_&) { return "MarketIvEvaluationSettings"; }
    using StorableMarketIvEvaluationSettings_ = Excel::StorableRiskValue_<MarketIvEvaluationSettings_>;
    inline const char* RiskValueType(const MarketIvDupireSettings_&) { return "MarketIvDupireSettings"; }
    using StorableMarketIvDupireSettings_ = Excel::StorableRiskValue_<MarketIvDupireSettings_>;
    inline const char* RiskValueType(const MarketIvRefitRiskSettings_&) { return "MarketIvRefitRiskSettings"; }
    using StorableMarketIvRefitRiskSettings_ = Excel::StorableRiskValue_<MarketIvRefitRiskSettings_>;
    inline const char* RiskValueType(const MarketIvFitRequest_&) { return "MarketIvFitRequest"; }
    using StorableMarketIvFitRequest_ = Excel::StorableRiskValue_<MarketIvFitRequest_>;
    inline const char* RiskValueType(const MarketIvFitResult_&) { return "MarketIvFitResult"; }
    using StorableMarketIvFitResult_ = Excel::StorableRiskValue_<MarketIvFitResult_>;
    inline const char* RiskValueType(const MarketIvQuery_&) { return "MarketIvQuery"; }
    using StorableMarketIvQuery_ = Excel::StorableRiskValue_<MarketIvQuery_>;
    inline const char* RiskValueType(const MarketIvPointResult_&) { return "MarketIvPointResult"; }
    using StorableMarketIvPointResult_ = Excel::StorableRiskValue_<MarketIvPointResult_>;
    inline const char* RiskValueType(const MarketIvDupireRequest_&) { return "MarketIvDupireRequest"; }
    using StorableMarketIvDupireRequest_ = Excel::StorableRiskValue_<MarketIvDupireRequest_>;
    inline const char* RiskValueType(const MarketIvDupireResult_&) { return "MarketIvDupireResult"; }
    using StorableMarketIvDupireResult_ = Excel::StorableRiskValue_<MarketIvDupireResult_>;
    inline const char* RiskValueType(const MarketIvRefitRiskRequest_&) { return "MarketIvRefitRiskRequest"; }
    using StorableMarketIvRefitRiskRequest_ = Excel::StorableRiskValue_<MarketIvRefitRiskRequest_>;
    inline const char* RiskValueType(const MarketIvRefitRiskResult_&) { return "MarketIvRefitRiskResult"; }
    using StorableMarketIvRefitRiskResult_ = Excel::StorableRiskValue_<MarketIvRefitRiskResult_>;
    inline const char* RiskValueType(const MarketIvCalibrationSnapshot_&) { return "MarketIvCalibrationSnapshot"; }
    using StorableMarketIvCalibrationSnapshot_ = Excel::StorableRiskValue_<MarketIvCalibrationSnapshot_>;
    DAL_MARKET_IV_API void MarketIvFitSettings_New(const String_&, const Matrix_<Cell_>&, Handle_<StorableMarketIvFitSettings_>*);
    DAL_MARKET_IV_API void MarketIvFitSettings_Get_Configuration(const Handle_<StorableMarketIvFitSettings_>&, Matrix_<Cell_>*);
    DAL_MARKET_IV_API void MarketIvEvaluationSettings_New(const String_&, const Matrix_<Cell_>&, Handle_<StorableMarketIvEvaluationSettings_>*);
    DAL_MARKET_IV_API void MarketIvEvaluationSettings_Get_Configuration(const Handle_<StorableMarketIvEvaluationSettings_>&, Matrix_<Cell_>*);
    DAL_MARKET_IV_API void MarketIvDupireSettings_New(const String_&, const Matrix_<Cell_>&, Handle_<StorableMarketIvDupireSettings_>*);
    DAL_MARKET_IV_API void MarketIvDupireSettings_Get_Configuration(const Handle_<StorableMarketIvDupireSettings_>&, Matrix_<Cell_>*);
    DAL_MARKET_IV_API void MarketIvRefitRiskSettings_New(const String_&, const Matrix_<Cell_>&, Handle_<StorableMarketIvRefitRiskSettings_>*);
    DAL_MARKET_IV_API void MarketIvRefitRiskSettings_Get_Configuration(const Handle_<StorableMarketIvRefitRiskSettings_>&, Matrix_<Cell_>*);
    DAL_MARKET_IV_API void MarketIvMarket_New(const String_&, double, double, const Matrix_<Cell_>&, Handle_<StorableMarketIvMarket_>*);
    DAL_MARKET_IV_API void
    MarketIvFlatMarket_New(const String_&, double, double, double, double, const Matrix_<Cell_>&, Handle_<StorableMarketIvMarket_>*);
    DAL_MARKET_IV_API void MarketIvFitRequest_New(const String_&,
                                                  const Handle_<StorableMarketIvMarket_>&,
                                                  const Matrix_<Cell_>&,
                                                  const Handle_<StorableMarketIvFitSettings_>&,
                                                  Handle_<StorableMarketIvFitRequest_>*);
    DAL_MARKET_IV_API void MarketIvFitResult_New(const String_&, const Handle_<StorableMarketIvFitRequest_>&, Handle_<StorableMarketIvFitResult_>*);
    DAL_MARKET_IV_API void MarketIvQuery_New(const String_&, double, double, Handle_<StorableMarketIvQuery_>*);
    DAL_MARKET_IV_API void MarketIvPointResult_New(const String_&,
                                                   const Handle_<StorableMarketIvCalibrationSnapshot_>&,
                                                   const Handle_<StorableMarketIvQuery_>&,
                                                   const Handle_<StorableMarketIvEvaluationSettings_>&,
                                                   Handle_<StorableMarketIvPointResult_>*);
    DAL_MARKET_IV_API void MarketIvDupireRequest_New(const String_&,
                                                     const Matrix_<Cell_>&,
                                                     const Matrix_<Cell_>&,
                                                     const Handle_<StorableMarketIvDupireSettings_>&,
                                                     Handle_<StorableMarketIvDupireRequest_>*);
    DAL_MARKET_IV_API void MarketIvDupireResult_New(const String_&,
                                                    const Handle_<StorableMarketIvCalibrationSnapshot_>&,
                                                    const Handle_<StorableMarketIvDupireRequest_>&,
                                                    Handle_<StorableMarketIvDupireResult_>*);
    DAL_MARKET_IV_API void MarketIvRefitRiskRequest_New(const String_&,
                                                        const Matrix_<Cell_>&,
                                                        const Matrix_<Cell_>&,
                                                        const Handle_<StorableMarketIvRefitRiskSettings_>&,
                                                        const Matrix_<Cell_>&,
                                                        Handle_<StorableMarketIvRefitRiskRequest_>*);
    DAL_MARKET_IV_API void MarketIvRefitRiskResult_New(const String_&,
                                                       const Handle_<StorableMarketIvCalibrationSnapshot_>&,
                                                       const Handle_<StorableMarketIvRefitRiskRequest_>&,
                                                       Handle_<StorableMarketIvRefitRiskResult_>*);
    DAL_MARKET_IV_API void MarketIvFitResult_Get_Request(const Handle_<StorableMarketIvFitResult_>&, Handle_<StorableMarketIvFitRequest_>*);
    DAL_MARKET_IV_API void MarketIvFitResult_Get_Snapshot(const Handle_<StorableMarketIvFitResult_>&, Handle_<StorableMarketIvCalibrationSnapshot_>*);
    DAL_MARKET_IV_API void MarketIvCalibrationSnapshot_Get_Market(const Handle_<StorableMarketIvCalibrationSnapshot_>&,
                                                                  Handle_<StorableMarketIvMarket_>*);
    DAL_MARKET_IV_API void MarketIvDupireResult_Get_Surface(const Handle_<StorableMarketIvDupireResult_>&, Handle_<LocalVolSurfaceData_>*);
    DAL_MARKET_IV_API void MarketIvMarket_Get_Terms(const Handle_<StorableMarketIvMarket_>&, Matrix_<Cell_>*);
    DAL_MARKET_IV_API void MarketIvMarket_Get_Summary(const Handle_<StorableMarketIvMarket_>&, Matrix_<Cell_>*);
    DAL_MARKET_IV_API void MarketIvFitResult_Get_Summary(const Handle_<StorableMarketIvFitResult_>&, Matrix_<Cell_>*);
    DAL_MARKET_IV_API void MarketIvFitResult_Get_Quotes(const Handle_<StorableMarketIvFitResult_>&, Matrix_<Cell_>*);
    DAL_MARKET_IV_API void MarketIvFitResult_Get_Parameters(const Handle_<StorableMarketIvFitResult_>&, Matrix_<Cell_>*);
    DAL_MARKET_IV_API void MarketIvFitResult_Get_Constraints(const Handle_<StorableMarketIvFitResult_>&, Matrix_<Cell_>*);
    DAL_MARKET_IV_API void MarketIvFitResult_Get_Iterations(const Handle_<StorableMarketIvFitResult_>&, Matrix_<Cell_>*);
    DAL_MARKET_IV_API void MarketIvFitResult_Get_Execution(const Handle_<StorableMarketIvFitResult_>&, Matrix_<Cell_>*);
    DAL_MARKET_IV_API void MarketIvFitResult_Get_Failure(const Handle_<StorableMarketIvFitResult_>&, Matrix_<Cell_>*);
    DAL_MARKET_IV_API void MarketIvCalibrationSnapshot_Get_Terms(const Handle_<StorableMarketIvCalibrationSnapshot_>&, Matrix_<Cell_>*);
    DAL_MARKET_IV_API void MarketIvCalibrationSnapshot_Get_Theta(const Handle_<StorableMarketIvCalibrationSnapshot_>&, Matrix_<Cell_>*);
    DAL_MARKET_IV_API void MarketIvCalibrationSnapshot_Get_Certificate(const Handle_<StorableMarketIvCalibrationSnapshot_>&, Matrix_<Cell_>*);
    DAL_MARKET_IV_API void MarketIvPointResult_Get_Values(const Handle_<StorableMarketIvPointResult_>&, Matrix_<Cell_>*);
    DAL_MARKET_IV_API void MarketIvPointResult_Get_Derivatives(const Handle_<StorableMarketIvPointResult_>&, Matrix_<Cell_>*);
    DAL_MARKET_IV_API void MarketIvPointResult_Get_Summary(const Handle_<StorableMarketIvPointResult_>&, Matrix_<Cell_>*);
    DAL_MARKET_IV_API void MarketIvDupireResult_Get_Nodes(const Handle_<StorableMarketIvDupireResult_>&, Matrix_<Cell_>*);
    DAL_MARKET_IV_API void MarketIvDupireResult_Get_Execution(const Handle_<StorableMarketIvDupireResult_>&, Matrix_<Cell_>*);
    DAL_MARKET_IV_API void MarketIvDupireResult_Get_Failure(const Handle_<StorableMarketIvDupireResult_>&, Matrix_<Cell_>*);
    DAL_MARKET_IV_API void MarketIvRefitRiskResult_Get_Axis(const Handle_<StorableMarketIvRefitRiskResult_>&, Matrix_<Cell_>*);
    DAL_MARKET_IV_API void MarketIvRefitRiskResult_Get_Responses(const Handle_<StorableMarketIvRefitRiskResult_>&, Matrix_<Cell_>*);
    DAL_MARKET_IV_API void MarketIvRefitRiskResult_Get_Execution(const Handle_<StorableMarketIvRefitRiskResult_>&, Matrix_<Cell_>*);
    DAL_MARKET_IV_API void MarketIvRefitRiskResult_Get_Failures(const Handle_<StorableMarketIvRefitRiskResult_>&, Matrix_<Cell_>*);
} // namespace Dal

#undef DAL_MARKET_IV_API

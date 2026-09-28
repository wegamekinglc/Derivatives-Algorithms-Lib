//
// Created by Codex on 2026/9/27.
//

#pragma once

#include <dal-excel/src/__curve_storable.hpp>
#include <dal-public/src/models.hpp>

#if defined(_WIN32) && defined(DAL_EXCEL_TEST_API_EXPORTS)
#define DAL_EXCEL_TEST_API __declspec(dllexport)
#elif defined(_WIN32) && defined(DAL_EXCEL_TEST_API_IMPORTS)
#define DAL_EXCEL_TEST_API __declspec(dllimport)
#else
#define DAL_EXCEL_TEST_API
#endif

namespace Dal {
    DAL_EXCEL_TEST_API void CorrelatedBSModelData_New(const String_& name,
                                                      const Vector_<String_>& indices,
                                                      const Vector_<>& spots,
                                                      const Vector_<>& vols,
                                                      const Vector_<>& divs,
                                                      double rate,
                                                      const Matrix_<>& correlations,
                                                      Handle_<ModelData_>* model);
    DAL_EXCEL_TEST_API void HybridBSEquityData_New(const String_& name,
                                                   const String_& index,
                                                   const String_& currency,
                                                   const String_& factor,
                                                   double spot,
                                                   double vol,
                                                   double div,
                                                   Handle_<HybridComponentData_>* component);
    DAL_EXCEL_TEST_API void
    HybridDeterministicRateData_New(const String_& name, const String_& currency, double rate, Handle_<HybridComponentData_>* component);
    DAL_EXCEL_TEST_API void HybridLogDfRateData_New(const String_& name,
                                                    const String_& currency,
                                                    const Vector_<>& times,
                                                    const Vector_<>& logDF,
                                                    const String_& scheme,
                                                    Handle_<HybridComponentData_>* component);
    DAL_EXCEL_TEST_API void HybridLogDfRateDataFromCurve_New(const String_& name,
                                                             const Handle_<StorableDiscountCurve_>& curve,
                                                             const Date_& evaluationDate,
                                                             const Vector_<Date_>& nodeDates,
                                                             const String_& scheme,
                                                             Handle_<HybridComponentData_>* component);
    DAL_EXCEL_TEST_API void HybridConstantCorrelationData_New(const String_& name,
                                                              const Vector_<String_>& factors,
                                                              const Matrix_<>& correlations,
                                                              Handle_<HybridCorrelationData_>* provider);
    DAL_EXCEL_TEST_API void HybridModelData_New(const String_& name,
                                                const String_& domesticCurrency,
                                                const Vector_<Handle_<Storable_>>& components,
                                                const Handle_<HybridCorrelationData_>& correlation,
                                                Handle_<ModelData_>* model);
    DAL_EXCEL_TEST_API void GSRCurveData_New(const String_& name,
                                            const Date_& evaluationDate,
                                            const String_& currency,
                                            const Vector_<Date_>& nodeDates,
                                            const Vector_<>& discountLogDF,
                                            const Vector_<String_>& projectionTenors,
                                            const Matrix_<>& projectionLogDF,
                                            Handle_<GSRCurveData_>* curve);
    DAL_EXCEL_TEST_API void GSRCurveDataFromCurveBlock_New(const String_& name,
                                                          const Handle_<StorableCurveBlock_>& block,
                                                          const Date_& evaluationDate,
                                                          const Vector_<Date_>& nodeDates,
                                                          const Vector_<String_>& projectionTenors,
                                                          Handle_<GSRCurveData_>* curve);
    DAL_EXCEL_TEST_API void GSRVolData_New(const String_& name,
                                          const Vector_<Date_>& gKnotDates,
                                          const Vector_<>& gValues,
                                          const Vector_<Date_>& hKnotDates,
                                          const Vector_<>& hValues,
                                          Handle_<GSRVolData_>* vol);
    DAL_EXCEL_TEST_API void GSRModelData_New(const String_& name,
                                             const Handle_<GSRCurveData_>& curve,
                                             const Handle_<GSRVolData_>& vol,
                                             Handle_<ModelData_>* model);
} // namespace Dal

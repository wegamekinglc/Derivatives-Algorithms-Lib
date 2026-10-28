#pragma once

#include "__calibrationinstrument.hpp"

#if defined(_WIN32) && defined(DAL_EXCEL_TEST_API_EXPORTS)
#define DAL_CURVEDATA_EXCEL_API __declspec(dllexport)
#elif defined(_WIN32) && defined(DAL_EXCEL_TEST_API_IMPORTS)
#define DAL_CURVEDATA_EXCEL_API __declspec(dllimport)
#else
#define DAL_CURVEDATA_EXCEL_API
#endif

namespace Dal {
    DAL_CURVEDATA_EXCEL_API void DiscountPWC_New(const String_& name,
                                                 const String_& ccy,
                                                 const Vector_<Date_>& knotDates,
                                                 const Vector_<>& rightForwards,
                                                 const Handle_<StorableDiscountCurve_>& base,
                                                 Handle_<StorableDiscountCurve_>* curve);
    DAL_CURVEDATA_EXCEL_API void CurveBlock_New(const String_& name,
                                                const String_& ccy,
                                                const Handle_<StorableCalibrationDiscountCurves_>& discounts,
                                                const Handle_<StorableCalibrationForwardCurves_>& forwards,
                                                const String_& liborBasis,
                                                Handle_<StorableCurveBlock_>* block);
} // namespace Dal

#undef DAL_CURVEDATA_EXCEL_API

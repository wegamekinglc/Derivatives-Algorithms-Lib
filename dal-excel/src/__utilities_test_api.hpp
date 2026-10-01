//
// Created by Codex on 2026/10/1.
//

#pragma once

#include <dal-public/src/calendar.hpp>
#include <dal-public/src/interp.hpp>

#if defined(_WIN32) && defined(DAL_EXCEL_TEST_API_EXPORTS)
#define DAL_UTILITIES_TEST_API __declspec(dllexport)
#elif defined(_WIN32) && defined(DAL_EXCEL_TEST_API_IMPORTS)
#define DAL_UTILITIES_TEST_API __declspec(dllimport)
#else
#define DAL_UTILITIES_TEST_API
#endif

namespace Dal {
    DAL_UTILITIES_TEST_API void Is_BizDay(const String_& center, const Date_& date, bool* result);
    DAL_UTILITIES_TEST_API void Next_BizDay(const String_& center, const Date_& date, Date_* result);
    DAL_UTILITIES_TEST_API void Prev_BizDay(const String_& center, const Date_& date, Date_* result);
    DAL_UTILITIES_TEST_API void Adjust_Date(const String_& center, const Date_& date, const String_& convention, Date_* result);
    DAL_UTILITIES_TEST_API void Count_BusDays(const String_& center, const Date_& begin, const Date_& end, int* result);
    DAL_UTILITIES_TEST_API void Interp1_New_Linear(const String_& name, const Vector_<>& x, const Vector_<>& y, Handle_<Interp1_>* result);
    DAL_UTILITIES_TEST_API void Interp1_New_Linear_Smoothed(
        const String_& name, const Vector_<>& x, const Vector_<>& y, double smoothing, const Vector_<>& fitWeights, Handle_<Interp1_>* result);
    DAL_UTILITIES_TEST_API void Interp1_New_Cubic(const String_& name,
                                                  const Vector_<>& x,
                                                  const Vector_<>& y,
                                                  const Vector_<int>& boundaryOrder,
                                                  const Vector_<>& boundaryValue,
                                                  Handle_<Interp1_>* result);
    DAL_UTILITIES_TEST_API void Interp1_Get(const Handle_<Interp1_>& interpolator, const Vector_<>& x, Vector_<>* values);
    DAL_UTILITIES_TEST_API void
    Interp2_New_Linear(const String_& name, const Vector_<>& x, const Vector_<>& y, const Matrix_<>& values, Handle_<Interp2_>* result);
    DAL_UTILITIES_TEST_API void Interp2_Get(const Handle_<Interp2_>& interpolator, const Vector_<>& x, const Vector_<>& y, Matrix_<>* values);
} // namespace Dal

#undef DAL_UTILITIES_TEST_API

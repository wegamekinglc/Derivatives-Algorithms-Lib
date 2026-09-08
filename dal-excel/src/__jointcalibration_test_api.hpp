// Portable declarations for the generic joint Excel construction path.
#pragma once

#include "__curvepricing_test_api.hpp"

#if defined(_WIN32) && defined(DAL_EXCEL_TEST_API_EXPORTS)
#define DAL_EXCEL_TEST_API __declspec(dllexport)
#elif defined(_WIN32) && defined(DAL_EXCEL_TEST_API_IMPORTS)
#define DAL_EXCEL_TEST_API __declspec(dllimport)
#else
#define DAL_EXCEL_TEST_API
#endif

namespace Dal {
    DAL_EXCEL_TEST_API void JointCurveDeclaration_New(const String_& name,
                                                      const Vector_<Handle_<Storable_>>& instruments,
                                                      const Vector_<Date_>& knotDates,
                                                      bool calibrateDiscountCurve,
                                                      const String_& collateral,
                                                      const String_& tenor,
                                                      const Matrix_<Cell_>& settings,
                                                      const Vector_<>& initialGuessPerNode,
                                                      Handle_<StorableJointCurveDeclaration_>* declaration);
    DAL_EXCEL_TEST_API void JointMultiCurveCalibrationSpec_New(const Date_& today,
                                                               const String_& currency,
                                                               const Vector_<Handle_<Storable_>>& declarations,
                                                               const Matrix_<Cell_>& settings,
                                                               Handle_<StorableJointMultiCurveCalibrationSpec_>* spec);
    DAL_EXCEL_TEST_API void Calibrate_JointMultiCurve(const Handle_<StorableJointMultiCurveCalibrationSpec_>& spec,
                                                      const Matrix_<Cell_>& settings,
                                                      Handle_<StorableJointMultiCurveCalibrationResult_>* result);
    DAL_EXCEL_TEST_API void JointMultiCurveCalibrationResult_Get_Curve(const Handle_<StorableJointMultiCurveCalibrationResult_>& result,
                                                                       int curveIndex,
                                                                       Handle_<StorableDiscountCurve_>* curve);
    DAL_EXCEL_TEST_API void JointMultiCurveCalibrationResult_Get(const Handle_<StorableJointMultiCurveCalibrationResult_>& result,
                                                                 const String_& attribute,
                                                                 Matrix_<Cell_>* value);
} // namespace Dal

#undef DAL_EXCEL_TEST_API

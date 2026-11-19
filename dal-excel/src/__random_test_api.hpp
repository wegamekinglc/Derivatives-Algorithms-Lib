#pragma once

#include <dal-public/src/random.hpp>

#if defined(_WIN32) && defined(DAL_EXCEL_TEST_API_EXPORTS)
#define DAL_EXCEL_TEST_API __declspec(dllexport)
#elif defined(_WIN32) && defined(DAL_EXCEL_TEST_API_IMPORTS)
#define DAL_EXCEL_TEST_API __declspec(dllimport)
#else
#define DAL_EXCEL_TEST_API
#endif

namespace Dal {
    DAL_EXCEL_TEST_API void PseudoRSG_New(const String_& name, double seed, double ndim, Handle_<PseudoRSG_>* f);
    DAL_EXCEL_TEST_API void SobolRSG_New(const String_& name, double iPath, double ndim, bool precise, bool polish, Handle_<SobolRSG_>* f);
    DAL_EXCEL_TEST_API void PseudoRSG_Get_Uniform(const Handle_<PseudoRSG_>& f, double numPaths, Matrix_<>* y);
    DAL_EXCEL_TEST_API void PseudoRSG_Get_Normal(const Handle_<PseudoRSG_>& f, double numPaths, Matrix_<>* y);
    DAL_EXCEL_TEST_API void SobolRSG_Get_Uniform(const Handle_<SobolRSG_>& f, double numPaths, Matrix_<>* y);
    DAL_EXCEL_TEST_API void SobolRSG_Get_Normal(const Handle_<SobolRSG_>& f, double numPaths, Matrix_<>* y);
#if defined(DAL_EXCEL_TEST_API_EXPORTS) || defined(DAL_EXCEL_TEST_API_IMPORTS) || defined(DAL_EXCEL_API_TESTS_PORTABLE)
    DAL_EXCEL_TEST_API Handle_<SobolRSG_>
    NewSobolRSGForTest(const String_& name, double iPath, double ndim, bool precise = false, bool polish = false);
#endif
} // namespace Dal

#undef DAL_EXCEL_TEST_API

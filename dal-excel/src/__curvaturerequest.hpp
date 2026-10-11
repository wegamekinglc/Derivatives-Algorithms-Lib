//
// Created by Codex on 2026/10/11.
//

#pragma once

#include <dal-public/src/bumpoveraad.hpp>

#include "__risk.hpp"

#if defined(_WIN32) && defined(DAL_EXCEL_TEST_API_EXPORTS)
#define DAL_CURVATURE_API __declspec(dllexport)
#elif defined(_WIN32) && defined(DAL_EXCEL_TEST_API_IMPORTS)
#define DAL_CURVATURE_API __declspec(dllimport)
#else
#define DAL_CURVATURE_API
#endif

namespace Dal::AAD {
    inline const char* RiskValueType(const BumpOverAADRequest_&) { return "BumpOverAADRequest"; }
} // namespace Dal::AAD

namespace Dal {
    using StorableBumpOverAADRequest_ = Excel::StorableRiskValue_<AAD::BumpOverAADRequest_>;

    DAL_CURVATURE_API void BumpOverAADRequest_New(
        const String_&, const Matrix_<Cell_>&, const Matrix_<Cell_>&, const Matrix_<Cell_>&, Handle_<StorableBumpOverAADRequest_>*);
    DAL_CURVATURE_API void BumpOverAADRequest_Get_Directions(const Handle_<StorableBumpOverAADRequest_>&, Matrix_<Cell_>*);
    DAL_CURVATURE_API void BumpOverAADRequest_Get_Steps(const Handle_<StorableBumpOverAADRequest_>&, Matrix_<Cell_>*);
    DAL_CURVATURE_API void BumpOverAADRequest_Get_Settings(const Handle_<StorableBumpOverAADRequest_>&, Matrix_<Cell_>*);
    DAL_CURVATURE_API void BumpOverAADRequest_Get_Shape(const Handle_<StorableBumpOverAADRequest_>&, Matrix_<Cell_>*);
} // namespace Dal

#undef DAL_CURVATURE_API

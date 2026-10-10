//
// Created by Codex on 2026/10/11.
//

#pragma once

#include "__curvaturerequest.hpp"
#include "__riskrequestrows.hpp"

namespace Dal::Excel {
    inline constexpr int CURVATURE_MAX_ROWS = 1048576;
    inline constexpr int CURVATURE_MAX_COLUMNS = 16384;

    inline void CheckCurvatureText(const String_& text, const String_& field) {
        REQUIRE(text.find('\0') == String_::npos, "InvalidCurvatureRequest: " + field + "; embedded NUL is unsupported");
    }

    template <class T_> const T_& CheckedCurvatureValue(const Handle_<StorableRiskValue_<T_>>& value, const char* field) {
        REQUIRE(value, String_("InvalidCurvatureRequest: ") + field + "; handle is null");
        return value->val_;
    }

    inline bool BlankCurvatureRange(const Matrix_<Cell_>& cells) {
        return cells.Rows() == 0 || (cells.Rows() == 1 && cells.Cols() == 1 && Cell::IsEmpty(cells(0, 0)));
    }

    inline double CurvatureNumber(const Cell_& cell, const String_& context) {
        const auto* value = std::get_if<double>(&cell.val_);
        REQUIRE(value && std::isfinite(*value), context + "expected finite numeric cell, excluding bool, text and dates");
        return *value;
    }

    inline Cell_ CurvatureBudgetCell(const std::optional<size_t>& value) { return value ? Cell_(double(*value)) : Cell_(); }

    inline Matrix_<Cell_> CurvatureMatrixCells(const Matrix_<>& values) {
        if (values.Rows() == 0 || values.Cols() == 0)
            return Matrix_<Cell_>(1, 1);
        REQUIRE(values.Rows() <= CURVATURE_MAX_ROWS && values.Cols() <= CURVATURE_MAX_COLUMNS,
                "InvalidCurvatureRequest: numeric spill exceeds worksheet bounds");
        return NumericCells(values);
    }

    inline Matrix_<Cell_> CurvatureVectorCells(const Vector_<>& values) {
        REQUIRE(values.size() <= static_cast<size_t>(CURVATURE_MAX_ROWS), "InvalidCurvatureRequest: vector spill exceeds worksheet row limit");
        if (values.empty())
            return Matrix_<Cell_>(1, 1);
        Matrix_<Cell_> cells(static_cast<int>(values.size()), 1);
        for (int row = 0; row < cells.Rows(); ++row)
            cells(row, 0) = values[row];
        return cells;
    }
} // namespace Dal::Excel

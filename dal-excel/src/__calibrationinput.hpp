//
// Created by Codex on 2026/10/5.
//

#pragma once

#include <cmath>

#include "__dupireinput.hpp"

namespace Dal::Excel {
    inline bool CalibrationRecordCapture(const Cell_& input) {
        if (Cell::IsEmpty(input))
            return false;
        REQUIRE(Cell::IsBool(input), "InvalidCalibrationPullback: retainCalibrationRecord; expected boolean or blank");
        return Cell::ToBool(input);
    }

#ifdef _WIN32
    inline void ValidateCalibrationTextInput(const OPER_* input, const String_& field) {
        ValidateNativeTextInput(input, field, "InvalidCalibrationPullback");
    }

    inline void ValidateCalibrationRecordCaptureInput(const OPER_* input) {
        const auto* scalar = ScriptScalarInput(input);
        REQUIRE(ScriptInputBlank(scalar) || scalar->xltype == xltypeBool,
                "InvalidCalibrationPullback: retainCalibrationRecord; type=" + String_(std::to_string(scalar->xltype)) +
                    "; expected boolean or blank");
    }

    inline void ValidateCalibrationAdjointCell(const OPER_& cell, const String_& field, int row, int column) {
        const String_ context = "InvalidCalibrationPullback: " + field + "; row=" + String::FromInt(row) + "; column=" + String::FromInt(column) +
                                "; expected finite numeric cell, excluding bool and text";
        REQUIRE(cell.xltype == xltypeInt || cell.xltype == xltypeNum, context);
        REQUIRE(cell.xltype != xltypeNum || std::isfinite(cell.val.num), context);
    }

    inline void ValidateCalibrationAdjointsInput(const OPER_* input, const String_& field, int expectedRows, int expectedColumns) {
        const bool multi = input->xltype == xltypeMulti;
        const int rows = multi ? input->val.array.rows : 1;
        const int columns = multi ? input->val.array.columns : 1;
        REQUIRE(rows == expectedRows && columns == expectedColumns, "InvalidCalibrationPullback: " + field + "; seed dimensions disagree");
        for (int row = 0; row < rows; ++row)
            for (int column = 0; column < columns; ++column) {
                const auto& cell = multi ? input->val.array.lparray[static_cast<size_t>(row) * columns + column] : *input;
                ValidateCalibrationAdjointCell(cell, field, row + 1, column + 1);
            }
    }
#endif
} // namespace Dal::Excel

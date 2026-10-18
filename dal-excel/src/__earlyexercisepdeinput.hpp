#pragma once

#include "__riskrequestinput.hpp"

namespace Dal::Excel {
#ifdef _WIN32
    inline void ValidateEarlyExercisePdeRange(const OPER_* input, const char* field) {
        if (ScriptInputBlank(ScriptScalarInput(input)))
            return;
        const bool multi = input->xltype == xltypeMulti;
        const int rows = multi ? input->val.array.rows : 1, cols = multi ? input->val.array.columns : 1;
        for (int row = 0; row < rows; ++row) {
            for (int col = 0; col < cols; ++col) {
                const auto* cell = multi ? input->val.array.lparray + row * cols + col : input;
                REQUIRE(ScriptInputBlank(cell) || cell->xltype == xltypeNum || cell->xltype == xltypeInt || cell->xltype == xltypeBool ||
                            cell->xltype == xltypeStr,
                        ScriptSettingLocation("EarlyExercisePdeRequest_New", field, row + 1, col + 1) + "; unsupported cell type");
                ValidateNativeTextInput(cell, field, "InvalidEarlyExercisePdeRequest");
            }
        }
    }
#endif
} // namespace Dal::Excel

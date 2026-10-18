#pragma once

#include <algorithm>

#include "__riskrequestinput.hpp"

namespace Dal::Excel {
#ifdef _WIN32
    inline void ValidateEarlyExercisePdeSettingsInput(const OPER_* input, const String_& function) {
        if (input->xltype == xltypeMulti) {
            const auto* cells = input->val.array.lparray;
            const size_t size = static_cast<size_t>(input->val.array.rows) * input->val.array.columns;
            if (std::all_of(cells, cells + size, [](const auto& cell) { return ScriptInputBlank(&cell); }))
                return;
        }
        ValidateRiskRequestSettings(input, function);
    }

    inline bool IsEarlyExercisePdeCell(const OPER_* cell) {
        return ScriptInputBlank(cell) || cell->xltype == xltypeNum || cell->xltype == xltypeInt || cell->xltype == xltypeBool ||
               cell->xltype == xltypeStr;
    }

    inline void ValidateEarlyExercisePdeRange(const OPER_* input, const char* field) {
        if (ScriptInputBlank(ScriptScalarInput(input)))
            return;
        const bool multi = input->xltype == xltypeMulti;
        const int rows = multi ? input->val.array.rows : 1, cols = multi ? input->val.array.columns : 1;
        for (int row = 0; row < rows; ++row) {
            for (int col = 0; col < cols; ++col) {
                const auto* cell = multi ? input->val.array.lparray + row * cols + col : input;
                REQUIRE(IsEarlyExercisePdeCell(cell),
                        ScriptSettingLocation("EarlyExercisePdeRequest_New", field, row + 1, col + 1) + "; unsupported cell type");
                ValidateNativeTextInput(cell, field, "InvalidEarlyExercisePdeRequest");
            }
        }
    }
#endif
} // namespace Dal::Excel

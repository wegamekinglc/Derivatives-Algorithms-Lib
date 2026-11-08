#pragma once

#include "__earlyexercisepdeinput.hpp"

namespace Dal::Excel {
#ifdef _WIN32
    inline void ValidateEarlyExercisePdeRiskName(const OPER_* input, const char* field = "name") {
        const auto* scalar = ScriptScalarInput(input);
        REQUIRE(ScriptInputBlank(scalar) || scalar->xltype == xltypeStr,
                String_("InvalidEarlyExercisePdeRiskRequest; ") + field + "; expected string or blank");
        ValidateNativeTextInput(scalar, field, "InvalidEarlyExercisePdeRiskRequest");
    }
    inline void ValidateEarlyExercisePdeRiskRange(const OPER_* input, const char* field) {
        const bool multi = input->xltype == xltypeMulti;
        const int rows = multi ? input->val.array.rows : 1, cols = multi ? input->val.array.columns : 1;
        for (int row = 0; row < rows; ++row) {
            for (int col = 0; col < cols; ++col) {
                const auto* cell = multi ? input->val.array.lparray + row * cols + col : input;
                const auto context = ScriptSettingLocation("EarlyExercisePdeRiskSettings_New", field, row + 1, col + 1);
                REQUIRE(IsEarlyExercisePdeCell(cell), "InvalidEarlyExercisePdeRiskRequest; " + context + "; unsupported cell type");
                ValidateNativeTextInput(cell, context, "InvalidEarlyExercisePdeRiskRequest");
            }
        }
    }
#endif
} // namespace Dal::Excel

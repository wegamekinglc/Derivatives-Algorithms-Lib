#pragma once

#include "__earlyexercisepdeinput.hpp"

namespace Dal::Excel {
#ifdef _WIN32
    inline void ValidateEarlyExercisePdeRiskAscii(const OPER_* cell, const String_& context) {
        if (cell->xltype != xltypeStr)
            return;
        const auto* begin = cell->val.str + 1;
        const auto* end = begin + static_cast<size_t>(cell->val.str[0]);
        REQUIRE(std::all_of(begin, end, [](wchar_t value) { return value > 0 && value <= 127; }),
                "InvalidEarlyExercisePdeRiskRequest; " + context + "; expected exact ASCII text without NUL");
    }
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
                if (std::string(field) == "settings" && col == 0)
                    ValidateEarlyExercisePdeRiskAscii(cell, context);
            }
        }
    }
    inline String_ ToString_orEarlyExerciseRisk(const OPER_* input, const char* field) {
        const auto* scalar = ScriptScalarInput(input);
        ValidateEarlyExercisePdeRiskName(scalar, field);
        if (std::string(field) == "risk_id")
            ValidateEarlyExercisePdeRiskAscii(scalar, field);
        try {
            return ToString(scalar);
        } catch (const std::exception& error) {
            THROW(String_("InvalidEarlyExercisePdeRiskRequest; ") + field + "; " + String_(error.what()));
        }
    }
    template <class T_> Handle_<T_> ToHandle_orEarlyExerciseRisk(_ENV, const OPER_* input, const char* field, bool optional = false) {
        try {
            return ToHandle<T_>(_env, ScriptScalarInput(input), optional);
        } catch (const std::exception& error) {
            THROW(String_("InvalidEarlyExercisePdeRiskRequest; ") + field + "; expected typed handle; " + String_(error.what()));
        }
    }
    inline Matrix_<Cell_> ToCellMatrix_orEarlyExerciseRisk(const OPER_* input, const char* field, bool optional = false, bool greedy = false) {
        const ScriptSettingsInput_ normalized(input);
        ValidateEarlyExercisePdeRiskRange(normalized.Get(), field);
        try {
            return ToCellMatrix(normalized.Get(), optional, greedy);
        } catch (const std::exception& error) {
            THROW(String_("InvalidEarlyExercisePdeRiskRequest; ") + field + "; " + String_(error.what()));
        }
    }
#endif
} // namespace Dal::Excel

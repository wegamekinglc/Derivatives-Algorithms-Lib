//
// Created by Codex on 2026/10/6.
//

#pragma once

#include "__dupireinput.hpp"
#include "__riskrequestrows.hpp"
#include "__value.hpp"

namespace Dal::Excel {
#ifdef _WIN32
    inline void ValidateRiskRequestText(const OPER_* input, const String_& field) {
        const auto* scalar = ScriptScalarInput(input);
        REQUIRE(ScriptInputBlank(scalar) || scalar->xltype == xltypeStr, "InvalidRiskRequest: " + field + "; expected string or blank");
        ValidateNativeTextInput(scalar, field, "InvalidRiskRequest");
    }

    inline void ValidateRiskRequestBoolean(const OPER_* input, const String_& field) {
        const auto* scalar = ScriptScalarInput(input);
        REQUIRE(ScriptInputBlank(scalar) || scalar->xltype == xltypeBool, "InvalidRiskRequest: " + field + "; expected boolean or blank");
    }

    inline void ValidateRiskRequestSettings(const OPER_* input, const String_& function) {
        ValidateScriptSettingsRange(input, function, "settings");
        if (ScriptInputBlank(ScriptScalarInput(input)))
            return;
        for (int row = 0; row < input->val.array.rows; ++row)
            for (int column = 0; column < input->val.array.columns; ++column)
                ValidateNativeTextInput(input->val.array.lparray + row * input->val.array.columns + column,
                                        ScriptSettingLocation(function, "settings", row + 1, column + 1), "InvalidRiskRequest");
    }

    inline void ValidateRiskRequestPaths(const OPER_* input, const char* function = "DupireScriptRiskSettings_New") {
        const auto* scalar = ScriptScalarInput(input);
        REQUIRE(scalar->xltype == xltypeNum || scalar->xltype == xltypeInt,
                String_("InvalidPathCount: ") + function + "; n_paths; expected numeric integer, excluding bool and text");
        CheckedMonteCarloPathCount(scalar->xltype == xltypeInt ? double(scalar->val.w) : scalar->val.num, function);
    }

    inline void ValidateRiskRequestBindingRow(const OPER_& ordinal, const OPER_& quote, int row) {
        if (ScriptInputBlank(&ordinal) && ScriptInputBlank(&quote))
            return;
        const auto context = ScriptSettingLocation("DupireScriptRiskRequest_New", "bindings", row, 1);
        REQUIRE(ordinal.xltype == xltypeNum || ordinal.xltype == xltypeInt, context + "constant ordinal must be numeric, excluding bool and text");
        PayloadBudget(Cell_(ordinal.xltype == xltypeInt ? double(ordinal.val.w) : ordinal.val.num), context, "constant ordinal");
        REQUIRE(!ScriptInputBlank(&quote) && quote.xltype == xltypeStr,
                ScriptSettingLocation("DupireScriptRiskRequest_New", "bindings", row, 2) + "expected nonempty quote ID");
        ValidateNativeTextInput(&quote, ScriptSettingLocation("DupireScriptRiskRequest_New", "bindings", row, 2), "InvalidRiskRequest");
    }

    inline void ValidateRiskRequestBindings(const OPER_* input) {
        if (ScriptInputBlank(ScriptScalarInput(input)))
            return;
        REQUIRE(input->xltype == xltypeMulti && input->val.array.columns == 2,
                "InvalidRiskRequest: DupireScriptRiskRequest_New; bindings; expected two columns, ordinal and quote ID");
        for (int row = 0; row < input->val.array.rows; ++row)
            ValidateRiskRequestBindingRow(input->val.array.lparray[2 * row], input->val.array.lparray[2 * row + 1], row + 1);
    }
#endif
} // namespace Dal::Excel

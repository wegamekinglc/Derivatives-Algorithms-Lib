//
// Created by Codex on 2026/10/5.
//

#pragma once

#include <algorithm>

#include "__scriptinput.hpp"

namespace Dal::Excel {
#ifdef _WIN32
    inline void ValidateNativeTextInput(const OPER_* input, const String_& field, const char* identifier) {
        const auto* scalar = ScriptScalarInput(input);
        if (scalar->xltype != xltypeStr)
            return;
        const auto* begin = scalar->val.str + 1;
        const auto* end = begin + static_cast<size_t>(scalar->val.str[0]);
        REQUIRE(std::find(begin, end, L'\0') == end, String_(identifier) + ": " + field + "; embedded NUL is unsupported");
    }

    inline void ValidateDupireTextInput(const OPER_* input, const String_& field) { ValidateNativeTextInput(input, field, "InvalidDupireInput"); }

    inline void ValidateDupireSettingsInput(const OPER_* input) {
        ValidateScriptSettingsRange(input, "MertonIVS_New", "settings");
        if (ScriptInputBlank(ScriptScalarInput(input)))
            return;
        for (int row = 0; row < input->val.array.rows; ++row)
            for (int column = 0; column < input->val.array.columns; ++column)
                ValidateDupireTextInput(input->val.array.lparray + row * input->val.array.columns + column,
                                        ScriptSettingLocation("MertonIVS_New", "settings", row + 1, column + 1));
    }
#endif
} // namespace Dal::Excel

//
// Created by Codex on 2026/10/11.
//

#pragma once

#include <cmath>

#include "__riskrequestinput.hpp"

namespace Dal::Excel {
#ifdef _WIN32
    inline void ValidateCurvatureNumber(const OPER_& value, const String_& context) {
        REQUIRE(value.xltype == xltypeNum || value.xltype == xltypeInt, context + "expected numeric cell, excluding bool, text, blank and errors");
        const double number = value.xltype == xltypeInt ? double(value.val.w) : value.val.num;
        REQUIRE(std::isfinite(number), context + "expected finite numeric cell");
    }

    inline void ValidateCurvatureNumbers(const OPER_* input, const String_& function, const String_& field) {
        const auto* scalar = ScriptScalarInput(input);
        if (ScriptInputBlank(scalar))
            return;
        if (input->xltype != xltypeMulti) {
            ValidateCurvatureNumber(*scalar, ScriptSettingLocation(function, field, 1, 1));
            return;
        }
        for (int row = 0; row < input->val.array.rows; ++row)
            for (int column = 0; column < input->val.array.columns; ++column)
                ValidateCurvatureNumber(input->val.array.lparray[static_cast<size_t>(row) * input->val.array.columns + column],
                                        ScriptSettingLocation(function, field, row + 1, column + 1));
    }
#endif
} // namespace Dal::Excel

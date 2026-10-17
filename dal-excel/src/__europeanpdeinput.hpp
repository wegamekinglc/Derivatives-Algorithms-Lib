//
// Created by Codex on 2026/10/11.
//

#pragma once

#include <cmath>

#include "__riskrequestinput.hpp"

namespace Dal::Excel {
#ifdef _WIN32
    inline void ValidateEuropeanPdeNumber(const OPER_* input, const char* field) {
        const auto* scalar = ScriptScalarInput(input);
        REQUIRE(scalar->xltype == xltypeNum || scalar->xltype == xltypeInt,
                String_("EuropeanPdeRiskRequest_New: ") + field + "; expected finite numeric cell, excluding bool and text");
        const double value = scalar->xltype == xltypeInt ? double(scalar->val.w) : scalar->val.num;
        REQUIRE(std::isfinite(value), String_("EuropeanPdeRiskRequest_New: ") + field + "; expected finite numeric cell");
    }
#endif
} // namespace Dal::Excel

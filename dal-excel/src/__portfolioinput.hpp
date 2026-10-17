//
// Created by Codex on 2026/10/7.
//

#pragma once

#include "__riskrequestinput.hpp"

namespace Dal::Excel {
#ifdef _WIN32
    inline void ValidatePortfolioTrades(const OPER_* input) {
        REQUIRE(input->xltype == xltypeMulti && input->val.array.rows > 0 && input->val.array.columns == 3,
                "InvalidScriptPortfolio: trades; expected nonempty physical three-column table");
        for (int row = 0; row < input->val.array.rows; ++row)
            for (int column = 0; column < 3; ++column) {
                const auto* cell = input->val.array.lparray + row * 3 + column;
                const auto field = ScriptSettingLocation("ScriptPortfolio_New", "trades", row + 1, column + 1);
                REQUIRE(cell->xltype == xltypeStr && cell->val.str[0] > 0, field + "expected nonempty text/handle without coercion");
                ValidateNativeTextInput(cell, field, "InvalidScriptPortfolio");
            }
    }

    inline void ValidatePortfolioTradeOrdinal(const OPER_* input, const char* function) {
        const auto* scalar = ScriptScalarInput(input);
        REQUIRE(scalar->xltype == xltypeNum || scalar->xltype == xltypeInt,
                String_("InvalidPortfolioRiskResult: ") + function + "; trade; expected numeric ordinal excluding bool/text");
        PayloadBudget(Cell_(scalar->xltype == xltypeInt ? double(scalar->val.w) : scalar->val.num), "InvalidPortfolioRiskResult: ", "trade");
    }
#endif
} // namespace Dal::Excel

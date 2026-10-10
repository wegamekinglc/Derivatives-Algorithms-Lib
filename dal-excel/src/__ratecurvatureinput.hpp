//
// Created by Codex on 2026/10/11.
//

#pragma once

#include "__curvatureinput.hpp"

namespace Dal::Excel {
#ifdef _WIN32
    inline void ValidateRateTradeHandle(const OPER_& cell, int ordinal, int row, int column) {
        const auto context = ScriptSettingLocation("RateTradeQuoteCurvatureResult_New", "trades", row, column) +
                             "trade_row=" + String_(std::to_string(ordinal)) + "; ";
        REQUIRE(!ScriptInputBlank(&cell) && cell.xltype == xltypeStr, context + "expected nonempty trade handle");
        ValidateNativeTextInput(&cell, context, "InvalidRateCurvatureRequest");
    }

    inline void ValidateRateTradeHandles(const OPER_* input) {
        if (input->xltype != xltypeMulti) {
            ValidateRateTradeHandle(*input, 1, 1, 1);
            return;
        }
        const auto& range = input->val.array;
        REQUIRE(range.rows > 0 && range.columns > 0 && (range.rows == 1 || range.columns == 1),
                "InvalidRateCurvatureRequest: RateTradeQuoteCurvatureResult_New; trades; expected nonempty row or column vector");
        for (int row = 0; row < range.rows; ++row)
            for (int column = 0; column < range.columns; ++column)
                ValidateRateTradeHandle(range.lparray[row * range.columns + column], row * range.columns + column + 1, row + 1, column + 1);
    }
#endif
} // namespace Dal::Excel

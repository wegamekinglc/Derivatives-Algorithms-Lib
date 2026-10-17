//
// Created by Codex on 2026/10/5.
//

#pragma once

#include <map>

#include "__scriptinput.hpp"

namespace Dal::Excel {
    inline String_ TextValue(const Cell_& cell, const String_& context) {
        REQUIRE(Cell::IsString(cell), context + "expected non-empty string; received cell type=" + String_(std::to_string(cell.val_.index())));
        return std::get<String_>(cell.val_);
    }

    inline bool IsDefaultSettingsInput(const Matrix_<Cell_>& input) {
        return (input.Rows() == 0 && input.Cols() == 0) || (input.Rows() == 1 && input.Cols() == 1 && Cell::IsEmpty(input(0, 0)));
    }

    template <class F_>
    void ReadRows(const Matrix_<Cell_>& input, const String_& function, const String_& argument, F_ applyRow, bool allowEmptyValues = false) {
        if (IsDefaultSettingsInput(input))
            return;
        REQUIRE(input.Cols() == 2, ScriptSettingLocation(function, argument, 1, input.Cols() < 2 ? input.Cols() + 1 : 3) +
                                       "expected 2 columns; rows=" + String_(std::to_string(input.Rows())) +
                                       " cols=" + String_(std::to_string(input.Cols())));
        std::map<String_, int> seen;
        for (int row = 0; row < input.Rows(); ++row) {
            const auto& keyCell = input(row, 0);
            const auto& value = input(row, 1);
            if (Cell::IsEmpty(keyCell) && Cell::IsEmpty(value))
                continue;
            const auto keyContext = ScriptSettingLocation(function, argument, row + 1, 1);
            REQUIRE(!Cell::IsEmpty(keyCell), keyContext + "expected non-empty key");
            const auto key = TextValue(keyCell, keyContext);
            const auto valueContext = ScriptSettingLocation(function, argument, row + 1, 2) + key + "; ";
            REQUIRE(allowEmptyValues || !Cell::IsEmpty(value), valueContext + "expected non-empty value");
            const auto inserted = seen.emplace(key, row + 1);
            REQUIRE(inserted.second, keyContext + "duplicate key " + key + "; first row=" + String_(std::to_string(inserted.first->second)) +
                                         "; expected each key once");
            applyRow(key, value, keyContext, valueContext);
        }
    }
} // namespace Dal::Excel

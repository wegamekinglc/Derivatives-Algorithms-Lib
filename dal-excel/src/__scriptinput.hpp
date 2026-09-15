//
// Created by Codex on 2026/9/15.
//

#pragma once

#include <vector>

#include <dal/string/strings.hpp>
#include <dal/utilities/exceptions.hpp>

#ifdef _WIN32
#include "_excel.hpp"
#include "_xlcall.hpp"
#endif

namespace Dal::Excel {
    inline String_ ScriptSettingLocation(const String_& function, const String_& argument, int row, int column) {
        return "InvalidSetting: " + function + "; " + argument + " row=" + String_(std::to_string(row)) +
               " column=" + String_(std::to_string(column)) + "; ";
    }

#ifdef _WIN32
    class ScriptSettingsInput_ {
        OPER_ normalized_{};
        std::vector<OPER_> cells_;
        const OPER_* input_;

        static OPER_ Normalize(const OPER_& cell) {
            auto result = cell;
            if (cell.xltype == xltypeInt) {
                result.xltype = xltypeNum;
                result.val.num = static_cast<double>(cell.val.w);
            }
            return result;
        }

    public:
        explicit ScriptSettingsInput_(const OPER_* input) : input_(input) {
            if (input->xltype == xltypeInt) {
                normalized_ = Normalize(*input);
                input_ = &normalized_;
            } else if (input->xltype == xltypeMulti) {
                const size_t size = static_cast<size_t>(input->val.array.rows) * input->val.array.columns;
                const auto* cells = input->val.array.lparray;
                bool hasInteger = false;
                for (size_t i = 0; i < size; ++i)
                    hasInteger = hasInteger || cells[i].xltype == xltypeInt;
                if (hasInteger) {
                    cells_.reserve(size);
                    for (size_t i = 0; i < size; ++i)
                        cells_.push_back(Normalize(cells[i]));
                    normalized_ = *input;
                    normalized_.val.array.lparray = cells_.data();
                    input_ = &normalized_;
                }
            }
        }
        [[nodiscard]] const OPER_* Get() const { return input_; }
    };

    inline const OPER_* ScriptScalarInput(const OPER_* input) {
        if (input->xltype == xltypeMulti && input->val.array.rows == 1 && input->val.array.columns == 1)
            return input->val.array.lparray;
        return input;
    }

    inline bool ScriptInputBlank(const OPER_* input) {
        return input->xltype == xltypeMissing || input->xltype == xltypeNil || (input->xltype == xltypeStr && input->val.str[0] == 0);
    }

    inline bool IsScriptSettingCell(const OPER_& cell) {
        return ScriptInputBlank(&cell) || cell.xltype == xltypeStr || cell.xltype == xltypeNum || cell.xltype == xltypeBool;
    }

    inline void ValidateScriptSettingsRange(const OPER_* input, const String_& function, const String_& argument) {
        if (ScriptInputBlank(ScriptScalarInput(input)))
            return;
        const bool multi = input->xltype == xltypeMulti;
        const int rows = multi ? input->val.array.rows : 1, cols = multi ? input->val.array.columns : 1;
        REQUIRE(cols == 2, ScriptSettingLocation(function, argument, 1, cols < 2 ? cols + 1 : 3) +
                               "expected 2 columns; rows=" + String_(std::to_string(rows)) + " cols=" + String_(std::to_string(cols)));
        for (int row = 0; row < rows; ++row)
            for (int col = 0; col < cols; ++col) {
                const auto& cell = input->val.array.lparray[row * cols + col];
                REQUIRE(IsScriptSettingCell(cell), ScriptSettingLocation(function, argument, row + 1, col + 1) + "received Excel type=" +
                                                       String_(std::to_string(cell.xltype)) + "; expected blank, string, number or boolean cell");
            }
    }
#endif
} // namespace Dal::Excel

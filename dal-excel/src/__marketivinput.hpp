#pragma once

#include "__riskrequestinput.hpp"

namespace Dal::Excel {
#ifdef _WIN32
    inline bool ValidMarketIvArray(const OPER_* input) {
        const int rows = input->val.array.rows, columns = input->val.array.columns;
        return rows > 0 && columns > 0 && rows <= 1048576 && columns <= 16384 && input->val.array.lparray &&
               static_cast<size_t>(rows) <= std::numeric_limits<size_t>::max() / static_cast<size_t>(columns);
    }
    inline void ValidateMarketIvNumber(const OPER_* input, const char* field) {
        if (input->xltype == xltypeMulti)
            REQUIRE(ValidMarketIvArray(input), String_("InvalidMarketIvRequest: ") + field + "; invalid array dimensions/storage");
        const auto* cell = ScriptScalarInput(input);
        REQUIRE(cell->xltype == xltypeNum || cell->xltype == xltypeInt,
                String_("InvalidMarketIvRequest: ") + field + "; numeric cell required excluding bool/text");
        const double value = cell->xltype == xltypeInt ? double(cell->val.w) : cell->val.num;
        REQUIRE(std::isfinite(value), String_("InvalidMarketIvRequest: ") + field + "; finite number required");
    }
    inline void ValidateMarketIvRange(const OPER_* input, const char* field, bool settings = false) {
        const bool multi = input->xltype == xltypeMulti;
        REQUIRE(!multi || ValidMarketIvArray(input), String_("InvalidMarketIvRequest: ") + field + "; invalid array dimensions/storage");
        const size_t count = multi ? static_cast<size_t>(input->val.array.rows) * input->val.array.columns : 1;
        const auto* cells = multi ? input->val.array.lparray : input;
        bool blank = true;
        for (size_t i = 0; i < count; ++i)
            blank &= ScriptInputBlank(cells + i);
        if (blank)
            return;
        if (settings)
            REQUIRE(multi && input->val.array.columns == 2, "InvalidMarketIvRequest: settings; nonempty range requires two columns");
        for (size_t i = 0; i < count; ++i) {
            const auto* cell = cells + i;
            REQUIRE(ScriptInputBlank(cell) || cell->xltype == xltypeNum || cell->xltype == xltypeInt || cell->xltype == xltypeStr,
                    String_("InvalidMarketIvRequest: ") + field + "; unsupported Excel cell type at ordinal=" + String_(std::to_string(i)));
            const int columns = multi ? input->val.array.columns : 1;
            ValidateNativeTextInput(
                cell, ScriptSettingLocation("MarketIvInput", field, static_cast<int>(i / columns + 1), static_cast<int>(i % columns + 1)),
                "InvalidMarketIvRequest");
        }
    }
#endif
} // namespace Dal::Excel

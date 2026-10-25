#pragma once

#include "__calibrationinstrumentinput.hpp"
#include "__scriptinput.hpp"
#include <dal-public/src/xccymontecarlo.hpp>

#ifdef _WIN32
namespace Dal::Excel {
    struct XccyTableInput_ {
        String_ function_, argument_;
        int columns_;
        XccyTableInput_(const String_& function, const String_& argument, int columns)
            : function_(function), argument_(argument), columns_(columns) {}
    };
    inline String_ XccyRawField(const OPER_* cells, int row, int col, int columns) {
        const auto& key = cells[columns == 2 ? row * columns : col];
        return key.xltype == xltypeStr && key.val.str ? ToString(&key) : String_();
    }
    inline bool IsXccyRawCell(const OPER_& cell) {
        if (cell.xltype == xltypeStr)
            return cell.val.str != nullptr;
        return cell.xltype == xltypeInt || IsScriptSettingCell(cell);
    }
    inline bool XccyRawBlank(const OPER_& cell) { return IsXccyRawCell(cell) && ScriptInputBlank(&cell); }
    inline void ValidateXccyRawCells(const OPER_* input, const XccyTableInput_& table) {
        const int columns = input->val.array.columns;
        for (int row = 0; row < input->val.array.rows; ++row)
            for (int col = 0; col < columns; ++col) {
                const auto& cell = input->val.array.lparray[row * columns + col];
                REQUIRE(IsXccyRawCell(cell), ScriptSettingLocation(table.function_, table.argument_, row + 1, col + 1) +
                                                 "field=" + XccyRawField(input->val.array.lparray, row, col, columns) + "; received Excel type=" +
                                                 String_(std::to_string(cell.xltype)) + "; expected blank, text, number or boolean");
            }
    }
    inline void ValidateXccyRawInput(const OPER_* input, const XccyTableInput_& table) {
        const auto location = ScriptSettingLocation(table.function_, table.argument_, 1, 1);
        if (input->xltype != xltypeMulti) {
            REQUIRE(XccyRawBlank(*input), location + "expected table or blank input");
            return;
        }
        REQUIRE(input->val.array.rows > 0 && input->val.array.rows <= 1048576 && input->val.array.lparray,
                location + "invalid table dimensions/storage");
        if (input->val.array.rows == 1 && input->val.array.columns == 1) {
            const auto* cell = input->val.array.lparray;
            if (XccyRawBlank(*cell))
                return;
        }
        REQUIRE(input->val.array.columns == table.columns_, location + "wrong table column count");
        ValidateXccyRawCells(input, table);
    }
    inline Matrix_<Cell_> ToCellMatrix_orXccy(const OPER_* input, const XccyTableInput_& table, bool optional = false, bool greedy = false) {
        REQUIRE(greedy, "XCCY table conversion requires complete selected dimensions");
        ValidateXccyRawInput(input, table);
        const ScriptSettingsInput_ normalized(input);
        return ToCellMatrix(normalized.Get(), optional, true);
    }
    inline void ValidateXccyPhaseInput(const OPER_* input) {
        if (input->xltype == xltypeMulti)
            REQUIRE(input->val.array.rows == 1 && input->val.array.columns == 1 && input->val.array.lparray,
                    "XccyMonteCarloRequest_New; phase; expected scalar phase");
        const auto* cell = ScriptScalarInput(input);
        if (ScriptInputBlank(cell))
            CalibrationInputFailure("InvalidCalibrationPlan", "/phase", "XccyMonteCarloRequest_New; phase; Fit, Validation or HeldOut");
        REQUIRE(cell->xltype == xltypeStr, "XccyMonteCarloRequest_New; phase; expected text Fit, Validation or HeldOut");
    }
    inline void ValidateXccyLabels(const OPER_* input) {
        const bool multi = input->xltype == xltypeMulti;
        const int count = multi ? input->val.array.rows * input->val.array.columns : 1;
        for (int i = 0; i < count; ++i) {
            const auto& cell = multi ? input->val.array.lparray[i] : *input;
            REQUIRE(cell.xltype == xltypeStr, ScriptSettingLocation("XccyParameterLabels_New", "labels", i + 1, 1) + "expected text label");
        }
    }
} // namespace Dal::Excel
#endif

namespace Dal::Excel::Calibration {
    template <> struct Input_<DateTime_> {
        static DateTime_ Read(const Cell_& cell) {
            if (Cell::IsDateTime(cell))
                return Cell::ToDateTime(cell);
            if (Cell::IsDate(cell) || Cell::IsString(cell))
                return DateTime_(Input_<Date_>::Read(cell));
            const double serial = Input_<double>::Read(cell), day = std::floor(serial);
            REQUIRE(day >= 0. && day <= INT_MAX, "InvalidCalibrationInput: valuation_time outside date range");
            return DateTime_(Date::FromExcel(static_cast<int>(day)), serial - day);
        }
    };
} // namespace Dal::Excel::Calibration

namespace Dal::Excel::Calibration {
    class XccyFields_ {
        const Matrix_<Cell_>& cells_;
        String_ function_;
        std::map<std::string, int> rows_;
        std::set<std::string> used_;

        String_ Location(const String_& key, int row, int column) const {
            return Excel::ScriptSettingLocation(function_, "settings", row + 1, column) + "field=" + key + "; ";
        }
        String_ Key(int row) const {
            try {
                return Excel::Calibration::Text(cells_(row, 0));
            } catch (const Exception_& error) {
                THROW(Location("", row, 1) + error.what());
            }
        }
        static bool Empty(const Matrix_<Cell_>& cells) {
            return cells.Rows() == 0 || (cells.Rows() == 1 && cells.Cols() == 1 && Cell::IsEmpty(cells(0, 0)));
        }

    public:
        XccyFields_(const Matrix_<Cell_>& cells, const String_& function, std::initializer_list<const char*> allowed, bool blankRows = false)
            : cells_(cells), function_(function) {
            if (Empty(cells))
                return;
            REQUIRE(cells.Cols() == 2,
                    Location("", 0, cells.Cols() < 2 ? cells.Cols() + 1 : 3) + "InvalidCalibrationInput: expected two field/value columns");
            const std::set<std::string> keys(allowed.begin(), allowed.end());
            std::set<String_> normalized;
            for (int row = 0; row < cells.Rows(); ++row) {
                if (blankRows && Cell::IsEmpty(cells(row, 0)) && Cell::IsEmpty(cells(row, 1)))
                    continue;
                const auto key = Key(row);
                REQUIRE(normalized.insert(key).second, Location(key, row, 1) + "InvalidCalibrationInput: duplicate normalized field " + key);
                REQUIRE(keys.count(Excel::Calibration::Bytes(key)) != 0, Location(key, row, 1) + "InvalidCalibrationInput: unknown key " + key);
                rows_.emplace(Excel::Calibration::Bytes(key), row);
            }
        }
        template <class T_> T_ Required(const char* key) {
            used_.insert(key);
            const auto found = rows_.find(key);
            REQUIRE(found != rows_.end(), Location(key, 0, 2) + "InvalidCalibrationInput: missing key " + String_(key));
            try {
                return Excel::Calibration::Input_<T_>::Read(cells_(found->second, 1));
            } catch (const Exception_& error) {
                THROW(Location(key, found->second, 2) + "InvalidCalibrationInput: field=" + String_(key) + "; cause=" + error.what());
            }
        }
        template <class T_> T_ Default(const char* key, const T_& fallback) {
            used_.insert(key);
            return rows_.count(key) ? Required<T_>(key) : fallback;
        }
        template <class T_> std::optional<T_> Maybe(const char* key) {
            used_.insert(key);
            const auto found = rows_.find(key);
            if (found == rows_.end() || Cell::IsEmpty(cells_(found->second, 1)))
                return std::nullopt;
            return Required<T_>(key);
        }
        void Finish() const {
            for (const auto& [key, row] : rows_)
                REQUIRE(used_.count(key) != 0, Location(String_(key), row, 1) + "InvalidCalibrationInput: unknown key " + String_(key));
        }
    };

} // namespace Dal::Excel::Calibration

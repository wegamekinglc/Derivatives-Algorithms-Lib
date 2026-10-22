#pragma once

#include "__calibrationinstrument.hpp"
#include <climits>
#include <dal-public/src/repository.hpp>
#include <type_traits>

namespace Dal::Excel::Calibration {
    inline std::string Bytes(const String_& value) { return std::string(value.data(), value.size()); }
    inline String_ Text(const Cell_& cell) {
        const auto* value = std::get_if<String_>(&cell.val_);
        REQUIRE(value && value->find('\0') == String_::npos, "InvalidCalibrationInput: expected text without NUL");
        return *value;
    }
    inline Handle_<Storable_> Fetch(const Cell_& cell) {
        ENV_SEED_TYPE(ObjectAccess_);
        return FetchRepository(_env, Text(cell));
    }
    template <class T_, class = void> struct HasToken_ : std::false_type {};
    template <class T_> struct HasToken_<T_, std::void_t<decltype(std::declval<T_>().String())>> : std::true_type {};
    template <class T_> struct Input_ {
        static T_ Read(const Cell_& cell) {
            if constexpr (HasToken_<T_>::value) {
                const auto text = Text(cell);
                const T_ value(text);
                REQUIRE(Bytes(String_(value.String())) == Bytes(text), "InvalidCalibrationInput: expected exact enum token");
                return value;
            } else {
                const auto handle = handle_cast<StorableCalibrationValue_<T_>>(Fetch(cell));
                REQUIRE(handle, "InvalidCalibrationInput: wrong typed settings handle");
                return handle->val_;
            }
        }
    };
    template <> struct Input_<String_> {
        static String_ Read(const Cell_& cell) { return Text(cell); }
    };
    template <> struct Input_<double> {
        static double Read(const Cell_& cell) {
            const auto* value = std::get_if<double>(&cell.val_);
            REQUIRE(value && std::isfinite(*value), "InvalidCalibrationInput: expected finite number excluding bool");
            return *value;
        }
    };
    template <> struct Input_<int> {
        static int Read(const Cell_& cell) {
            const double value = Input_<double>::Read(cell);
            REQUIRE(value >= INT_MIN && value <= INT_MAX && std::trunc(value) == value, "InvalidCalibrationInput: expected exact 32-bit integer");
            return static_cast<int>(value);
        }
    };
    template <> struct Input_<bool> {
        static bool Read(const Cell_& cell) {
            const auto* value = std::get_if<bool>(&cell.val_);
            REQUIRE(value, "InvalidCalibrationInput: expected boolean cell");
            return *value;
        }
    };
    template <> struct Input_<Date_> {
        static Date_ Read(const Cell_& cell) {
            Date_ date;
            if (Cell::IsDate(cell))
                date = Cell::ToDate(cell);
            else if (Cell::IsString(cell)) {
                const auto text = Text(cell);
                date = Date::FromString(text);
                REQUIRE(Bytes(Date::ToString(date)) == Bytes(text), "InvalidCalibrationInput: expected ISO date");
            } else
                date = Date::FromExcel(Input_<int>::Read(cell));
            REQUIRE(date.IsValid(), "InvalidCalibrationInput: expected valid date");
            return date;
        }
    };
    template <> struct Input_<CalibrationValueUnit_> {
        static CalibrationValueUnit_ Read(const Cell_& cell) { return CalibrationValueUnit_::Parse(Text(cell)); }
    };
    template <class T_> struct Input_<Handle_<T_>> {
        static Handle_<T_> Read(const Cell_& cell) {
            const auto base = Fetch(cell);
            const auto handle = handle_cast<T_>(base);
            REQUIRE(handle, "InvalidCalibrationInput: wrong typed handle");
            return handle;
        }
    };
    template <> struct Input_<Handle_<DiscountCurve_>> {
        static Handle_<DiscountCurve_> Read(const Cell_& cell) {
            const auto base = Fetch(cell);
            if (const auto wrapper = handle_cast<StorableDiscountCurve_>(base))
                return wrapper->val_;
            const auto direct = handle_cast<DiscountCurve_>(base);
            REQUIRE(direct, "InvalidCalibrationInput: expected discount curve handle");
            return direct;
        }
    };
    template <> struct Input_<ScriptValuationSettings_> {
        static ScriptValuationSettings_ Read(const Cell_& cell) {
            const auto value = handle_cast<StorableScriptValuationSettings_>(Fetch(cell));
            REQUIRE(value, "InvalidCalibrationInput: expected valuation settings handle");
            return value->val_;
        }
    };
    class Fields_ {
        std::map<std::string, Cell_> values_;
        std::set<std::string> used_;

    public:
        explicit Fields_(const Matrix_<Cell_>& cells) {
            if (cells.Rows() == 0 || (cells.Rows() == 1 && cells.Cols() == 1 && Cell::IsEmpty(cells(0, 0))))
                return;
            REQUIRE(cells.Cols() == 2, "InvalidCalibrationInput: expected two field/value columns");
            for (int row = 0; row < cells.Rows(); ++row) {
                const auto key = Bytes(Text(cells(row, 0)));
                REQUIRE(values_.emplace(key, cells(row, 1)).second, "InvalidCalibrationInput: duplicate key " + String_(key));
            }
        }
        template <class T_> T_ Required(const char* key) {
            used_.insert(key);
            const auto value = values_.find(key);
            REQUIRE(value != values_.end(), "InvalidCalibrationInput: missing key " + String_(key));
            try {
                return Input_<T_>::Read(value->second);
            } catch (const Exception_& error) {
                THROW("InvalidCalibrationInput: field=" + String_(key) + "; cause=" + String_(error.what()));
            }
        }
        template <class T_> T_ Default(const char* key, T_ fallback) {
            used_.insert(key);
            return values_.count(key) ? Required<T_>(key) : fallback;
        }
        template <class T_> std::optional<T_> Maybe(const char* key) {
            used_.insert(key);
            const auto value = values_.find(key);
            if (value == values_.end() || Cell::IsEmpty(value->second))
                return std::nullopt;
            return Required<T_>(key);
        }
        void Finish() const {
            for (const auto& value : values_)
                REQUIRE(used_.count(value.first) != 0, "InvalidCalibrationInput: unknown key " + String_(value.first));
        }
    };
    class Rows_ {
        const Matrix_<Cell_>& cells_;
        std::map<std::string, int> columns_;

    public:
        Rows_(const Matrix_<Cell_>& cells, std::initializer_list<const char*> columns) : cells_(cells) {
            REQUIRE(cells.Rows() >= 1 && cells.Cols() == static_cast<int>(columns.size()), "InvalidCalibrationInput: wrong table shape");
            for (int col = 0; col < cells.Cols(); ++col)
                REQUIRE(columns_.emplace(Bytes(Text(cells(0, col))), col).second, "InvalidCalibrationInput: duplicate column");
            for (const char* column : columns)
                REQUIRE(columns_.count(column) != 0, "InvalidCalibrationInput: missing or unknown column " + String_(column));
        }
        [[nodiscard]] int Size() const { return cells_.Rows() - 1; }
        template <class T_> T_ Get(int row, const char* column) const {
            try {
                return Input_<T_>::Read(cells_(row + 1, columns_.at(column)));
            } catch (const Exception_& error) {
                THROW("InvalidCalibrationInput: row=" + String_(std::to_string(row + 2)) + "; column=" + column + "; cause=" + String_(error.what()));
            }
        }
        template <class T_> std::optional<T_> Maybe(int row, const char* column) const {
            return Cell::IsEmpty(cells_(row + 1, columns_.at(column))) ? std::nullopt : std::optional<T_>(Get<T_>(row, column));
        }
    };
} // namespace Dal::Excel::Calibration

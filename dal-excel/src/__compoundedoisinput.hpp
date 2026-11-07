#pragma once

#include "__compoundedois.hpp"
#include "__xccyinput.hpp"

namespace Dal::Excel::Calibration {
    template <> struct Input_<Handle_<PreparedCompoundedOis_>> {
        static Handle_<PreparedCompoundedOis_> Read(const Cell_& cell) {
            const auto wrapper = handle_cast<StorablePreparedCompoundedOis_>(Fetch(cell));
            REQUIRE(wrapper && wrapper->val_, "InvalidCalibrationInput: expected prepared compounded OIS tables");
            return wrapper->val_;
        }
    };
    class OisRows_ {
        const Matrix_<Cell_>& cells_;
        std::map<std::string, int> columns_;
        String_ function_;

    public:
        OisRows_(const Matrix_<Cell_>& cells, const String_& function, std::initializer_list<const char*> columns)
            : cells_(cells), function_(function) {
            REQUIRE(cells.Rows() >= 1 && cells.Cols() == static_cast<int>(columns.size()), "InvalidCalibrationInput: wrong table shape");
            const std::set<std::string> allowed(columns.begin(), columns.end());
            for (int j = 0; j < cells.Cols(); ++j) {
                const auto location = Excel::ScriptSettingLocation(function_, "rows", 1, j + 1);
                const auto* name = std::get_if<String_>(&cells(0, j).val_);
                REQUIRE(name, location + "expected column name");
                const auto key = Bytes(*name);
                REQUIRE(allowed.count(key), location + "unknown column " + *name);
                REQUIRE(columns_.emplace(key, j).second, location + "duplicate column " + *name);
            }
        }
        [[nodiscard]] int Size() const { return cells_.Rows() - 1; }
        [[nodiscard]] bool Blank(int i) const {
            for (int j = 0; j < cells_.Cols(); ++j)
                if (!Cell::IsEmpty(cells_(i + 1, j)))
                    return false;
            return true;
        }
        template <class T_> T_ Get(int i, const char* column) const {
            const int j = columns_.at(column);
            try {
                const auto value = Input_<T_>::Read(cells_(i + 1, j));
                if constexpr (std::is_same_v<T_, DateTime_>)
                    REQUIRE(value.IsValid() && value.Frac() == 0., "expected midnight fixing_time");
                return value;
            } catch (const Exception_& error) {
                THROW(Excel::ScriptSettingLocation(function_, "rows", i + 2, j + 1) + "field=" + column + "; " + error.what());
            }
        }
        template <class T_, class F_> Vector_<T_> Read(F_ construct) const {
            Vector_<T_> result;
            int blank = -1;
            for (int i = 0; i < Size(); ++i) {
                if (Blank(i)) {
                    if (blank < 0)
                        blank = i;
                    continue;
                }
                result.push_back(construct(i));
            }
            REQUIRE(blank < 0, Excel::ScriptSettingLocation(function_, "rows", blank + 2, 1) + "blank data row");
            return result;
        }
    };
} // namespace Dal::Excel::Calibration

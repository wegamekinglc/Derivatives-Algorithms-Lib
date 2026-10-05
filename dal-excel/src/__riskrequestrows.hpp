//
// Created by Codex on 2026/10/6.
//

#pragma once

#include <cmath>
#include <limits>

#include <dal-public/src/value.hpp>

#include "__scriptsettingsrows.hpp"

namespace Dal::Excel {
    inline Vector_<String_> ListValue(const Cell_& cell, const String_& context) {
        if (Cell::IsEmpty(cell))
            return {};
        const auto text = TextValue(cell, context);
        const auto items = String::Split(text, ';', true);
        for (const auto& item : items)
            REQUIRE(!item.empty() && item.find('\0') == String_::npos, context + "expected semicolon-separated nonempty entries without NUL");
        return items;
    }

    inline Vector_<> FactorList(const Cell_& cell, const String_& context) {
        Vector_<> factors;
        for (const auto& item : ListValue(cell, context)) {
            double factor;
            try {
                factor = String::ToDouble(item);
            } catch (const std::exception& error) {
                THROW(context + "expected numeric report factor; value=" + item + "; " + String_(error.what()));
            }
            REQUIRE(std::isfinite(factor) && factor > 0.0, context + "report factor must be finite and positive");
            factors.push_back(factor);
        }
        return factors;
    }

    inline size_t PayloadBudget(const Cell_& cell, const String_& context, const char* field = "numeric_payload_budget_bytes") {
        const auto* number = std::get_if<double>(&cell.val_);
        REQUIRE(number && std::isfinite(*number) && std::trunc(*number) == *number && *number >= 0.0 && *number <= 9007199254740991.0 &&
                    static_cast<long double>(*number) <= static_cast<long double>((std::numeric_limits<size_t>::max)()),
                context + field + " must be an exactly representable nonnegative size_t integer at most 2^53-1");
        return static_cast<size_t>(*number);
    }

    [[maybe_unused]] static Matrix_<Cell_> NumericCells(const Matrix_<>& source) {
        if (source.Cols() == 0)
            return Matrix_<Cell_>(1, 1);
        Matrix_<Cell_> cells(source.Rows(), source.Cols());
        for (int row = 0; row < source.Rows(); ++row)
            for (int column = 0; column < source.Cols(); ++column)
                cells(row, column).val_.emplace<double>(source(row, column));
        return cells;
    }

    inline Cell_ RiskCell(int value) { return Cell_(double(value)); }
    template <class T_> Cell_ RiskCell(const T_& value) { return Cell_(value); }

    template <class T_> Cell_ OptionalCell(const std::optional<T_>& value) { return value ? RiskCell(*value) : Cell_(); }

    template <class T_> std::pair<String_, Cell_> Field(const String_& key, const T_& value) { return {key, RiskCell(value)}; }

    inline Matrix_<Cell_> FieldCells(const Vector_<std::pair<String_, Cell_>>& fields) {
        if (fields.empty())
            return Matrix_<Cell_>(1, 1);
        Matrix_<Cell_> cells(static_cast<int>(fields.size()), 2);
        for (int row = 0; row < cells.Rows(); ++row) {
            cells(row, 0) = fields[row].first;
            cells(row, 1) = fields[row].second;
        }
        return cells;
    }

    inline Matrix_<Cell_> RiskCoordinateCells(const Vector_<Script::RiskCoordinate_>& axis) {
        const Vector_<String_> headers{"id", "label", "family", "ordinal", "value", "native_unit", "physical_unit", "report_scale"};
        Matrix_<Cell_> cells(static_cast<int>(axis.size()) + 1, static_cast<int>(headers.size()));
        for (int column = 0; column < cells.Cols(); ++column)
            cells(0, column) = headers[static_cast<size_t>(column)];
        for (size_t index = 0; index < axis.size(); ++index) {
            const auto& coordinate = axis[index];
            const int row = static_cast<int>(index) + 1;
            cells(row, 0) = coordinate.id_;
            cells(row, 1) = coordinate.label_;
            cells(row, 2) = coordinate.family_;
            cells(row, 3) = double(coordinate.ordinal_);
            cells(row, 4) = coordinate.value_;
            cells(row, 5) = coordinate.nativeUnit_;
            cells(row, 6) = OptionalCell(coordinate.physicalUnit_);
            cells(row, 7) = coordinate.reportScale_;
        }
        return cells;
    }

} // namespace Dal::Excel

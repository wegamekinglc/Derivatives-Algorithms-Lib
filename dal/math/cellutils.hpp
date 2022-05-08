//
// Created by wegam on 2020/10/25.
//

#pragma once

#include <bitset>
#include <dal/math/cell.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/platform/optionals.hpp>

namespace Dal {
    namespace Cell {
        template<class T_> bool IsType(const Cell_& c) {
            return std::holds_alternative<T_>(c.val_);
        }
        template<int> bool IsType(const Cell_& c) {
            return IsInt(c);
        }

        template<typename... OK_>
        struct TypeCheck_ {
            bool operator()(const Cell_& c) const {
                return (IsType<OK_>(c) || ...);
            }
        };

        Cell_ ConvertString(const String_& src);
        String_ CoerceToString(const Cell_& src);
        Cell_ FromOptionalDouble(const std::optional<double>& src);

        Vector_<String_> ToStringLines(const Matrix_<Cell_>& src);
    } // namespace Cell
} // namespace Dal
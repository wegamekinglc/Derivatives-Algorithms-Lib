#pragma once

#include <array>

#include <dal-public/src/value.hpp>

#include "__curvaturerows.hpp"

namespace Dal::Excel {
    inline Vector_<> ReadMonteCarloPoint(const Matrix_<Cell_>& cells, const char* function) {
        REQUIRE(cells.Rows() == 1 || cells.Cols() == 1, String_(function) + "; point must be a complete row or column vector");
        Vector_<> point;
        for (int row = 0; row < cells.Rows(); ++row)
            for (int column = 0; column < cells.Cols(); ++column)
                point.push_back(CurvatureNumber(cells(row, column), ScriptSettingLocation(function, "point", row + 1, column + 1)));
        return point;
    }

    inline size_t ReadMonteCarloPaths(double paths, const char* function) {
        const auto count = PayloadBudget(Cell_(paths), String_(function) + "; ", "paths");
        REQUIRE(count > 0, String_(function) + "; paths must be positive");
        return count;
    }

    inline Matrix_<Cell_> MonteCarloObservationCells(const Vector_<Script::RiskObservationSnapshot_>& values, const char* function) {
        REQUIRE(values.size() < static_cast<size_t>(CURVATURE_MAX_ROWS), String_(function) + "; rows exceed worksheet limit");
        Matrix_<Cell_> cells(static_cast<int>(values.size()) + 1, 4);
        const std::array<const char*, 4> headers = {{"index", "fixing_time", "historical", "value"}};
        for (int column = 0; column < 4; ++column)
            cells(0, column) = headers[column];
        for (int row = 1; row < cells.Rows(); ++row) {
            const auto& value = values[row - 1];
            cells(row, 0) = value.index_;
            cells(row, 1) = value.fixingTime_;
            cells(row, 2) = value.historical_;
            cells(row, 3) = OptionalCell(value.value_);
        }
        return cells;
    }
} // namespace Dal::Excel

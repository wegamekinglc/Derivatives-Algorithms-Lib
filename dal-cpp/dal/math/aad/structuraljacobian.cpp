//
// Created by Codex on 2026/10/08.
//

#include <dal/platform/strict.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>

#include <dal/math/aad/structuraljacobian.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal::AAD {
    namespace {
        using Supports_ = Vector_<Vector_<size_t>>;
        using ColumnColors_ = std::map<size_t, Vector_<size_t>>;

        size_t CheckedProduct(size_t lhs, size_t rhs) {
            REQUIRE(lhs == 0 || rhs <= std::numeric_limits<size_t>::max() / lhs, "PlanStructuralJacobian: numeric payload byte extent overflow");
            return lhs * rhs;
        }

        size_t PayloadBytes(size_t rows, size_t columns) { return CheckedProduct(CheckedProduct(rows, columns), sizeof(double)); }

        void RequireBudget(size_t bytes, const StructuralJacobianSettings_& settings) {
            REQUIRE(!settings.numericPayloadBudgetBytes_ || bytes <= *settings.numericPayloadBudgetBytes_,
                    "PlanStructuralJacobian: numeric payload exceeds numericPayloadBudgetBytes");
        }

        Supports_ NormalizeSupports(size_t inputs, const Supports_& original) {
            Supports_ supports = original;
            for (auto& row : supports) {
                std::sort(row.begin(), row.end());
                row.erase(std::unique(row.begin(), row.end()), row.end());
                for (const auto column : row)
                    REQUIRE(column < inputs, "PlanStructuralJacobian: support column is outside the input axis");
            }
            return supports;
        }

        void MarkConflicts(const Vector_<size_t>& support, const ColumnColors_& columnColors, size_t generation, Vector_<size_t>* marks) {
            for (const auto column : support) {
                const auto found = columnColors.find(column);
                if (found == columnColors.end())
                    continue;
                for (const auto color : found->second)
                    (*marks)[color] = generation;
            }
        }

        size_t FirstAvailableColor(const Vector_<size_t>& marks, size_t generation) {
            size_t color = 0;
            while (color < marks.size() && marks[color] == generation)
                ++color;
            return color;
        }

        struct Coloring_ {
            Vector_<std::optional<size_t>> rows_;
            Supports_ colors_;
        };

        Coloring_ ColorSupports(const Supports_& supports) {
            Coloring_ result{Vector_<std::optional<size_t>>(supports.size()), {}};
            ColumnColors_ columnColors;
            Vector_<size_t> marks;
            for (size_t row = 0; row < supports.size(); ++row) {
                if (supports[row].empty())
                    continue;
                const size_t generation = row + 1;
                MarkConflicts(supports[row], columnColors, generation, &marks);
                const size_t color = FirstAvailableColor(marks, generation);
                if (color == result.colors_.size()) {
                    marks.push_back(0);
                    result.colors_.emplace_back();
                }
                result.rows_[row] = color;
                result.colors_[color].push_back(row);
                for (const auto column : supports[row])
                    columnColors[column].push_back(color);
            }
            return result;
        }

        void ValidateDirections(const StructuralJacobianPlan_& plan, const Matrix_<>& gradients) {
            REQUIRE(static_cast<size_t>(gradients.Rows()) == plan.ColorCount() && static_cast<size_t>(gradients.Cols()) == plan.Inputs(),
                    "RecoverStructuralJacobian: direction matrix must have ColorCount rows and Inputs columns");
            for (const auto value : gradients)
                REQUIRE(std::isfinite(value), "RecoverStructuralJacobian: every color gradient must be finite");
        }
    } // namespace

    const Vector_<size_t>& StructuralJacobianPlan_::RowSupport(size_t row) const {
        REQUIRE(row < Outputs(), "StructuralJacobianPlan.RowSupport: row index is outside the output axis");
        return supports_[row];
    }

    std::optional<size_t> StructuralJacobianPlan_::RowColor(size_t row) const {
        REQUIRE(row < Outputs(), "StructuralJacobianPlan.RowColor: row index is outside the output axis");
        return rowColors_[row];
    }

    const Vector_<size_t>& StructuralJacobianPlan_::ColorRows(size_t color) const {
        REQUIRE(color < ColorCount(), "StructuralJacobianPlan.ColorRows: color index is outside the color range");
        return colorRows_[color];
    }

    StructuralJacobianPlan_ PlanStructuralJacobian(size_t inputs, const Supports_& rowSupports, const StructuralJacobianSettings_& settings) {
        const auto maximum = static_cast<size_t>(std::numeric_limits<int>::max());
        REQUIRE(inputs <= maximum, "PlanStructuralJacobian: input count exceeds the matrix column limit");
        REQUIRE(rowSupports.size() <= maximum, "PlanStructuralJacobian: output count exceeds the matrix row limit");
        const size_t resultBytes = PayloadBytes(rowSupports.size(), inputs);
        RequireBudget(resultBytes, settings);
        auto supports = NormalizeSupports(inputs, rowSupports);
        auto coloring = ColorSupports(supports);
        const size_t directionBytes = PayloadBytes(coloring.colors_.size(), inputs);
        REQUIRE(directionBytes <= std::numeric_limits<size_t>::max() - resultBytes,
                "PlanStructuralJacobian: combined numeric payload byte extent overflow");
        RequireBudget(resultBytes + directionBytes, settings);
        return {inputs, std::move(supports), std::move(coloring.rows_), std::move(coloring.colors_), resultBytes, directionBytes};
    }

    Matrix_<> RecoverStructuralJacobian(const StructuralJacobianPlan_& plan, const Matrix_<>& colorGradients) {
        ValidateDirections(plan, colorGradients);
        Matrix_<> result(static_cast<int>(plan.Outputs()), static_cast<int>(plan.Inputs()), 0.0);
        for (size_t row = 0; row < plan.Outputs(); ++row)
            if (const auto color = plan.RowColor(row))
                for (const auto column : plan.RowSupport(row))
                    result(static_cast<int>(row), static_cast<int>(column)) = colorGradients(static_cast<int>(*color), static_cast<int>(column));
        return result;
    }
} // namespace Dal::AAD

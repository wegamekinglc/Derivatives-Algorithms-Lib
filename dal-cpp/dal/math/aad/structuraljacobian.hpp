//
// Created by Codex on 2026/10/08.
//

#pragma once

#include <cstddef>
#include <optional>
#include <utility>

#include <dal/math/matrix/matrixs.hpp>

namespace Dal::AAD {
    struct StructuralJacobianSettings_ {
        std::optional<size_t> numericPayloadBudgetBytes_;
    };

    class StructuralJacobianPlan_ {
        size_t inputs_;
        Vector_<Vector_<size_t>> supports_;
        Vector_<std::optional<size_t>> rowColors_;
        Vector_<Vector_<size_t>> colorRows_;
        size_t resultBytes_;
        size_t directionBytes_;

        StructuralJacobianPlan_(size_t inputs,
                                Vector_<Vector_<size_t>> supports,
                                Vector_<std::optional<size_t>> rowColors,
                                Vector_<Vector_<size_t>> colorRows,
                                size_t resultBytes,
                                size_t directionBytes)
            : inputs_(inputs), supports_(std::move(supports)), rowColors_(std::move(rowColors)), colorRows_(std::move(colorRows)),
              resultBytes_(resultBytes), directionBytes_(directionBytes) {}
        friend StructuralJacobianPlan_ PlanStructuralJacobian(size_t, const Vector_<Vector_<size_t>>&, const StructuralJacobianSettings_&);
        friend StructuralJacobianPlan_ PlanDenseJacobian(size_t, size_t, const StructuralJacobianSettings_&);

    public:
        [[nodiscard]] size_t Inputs() const { return inputs_; }
        [[nodiscard]] size_t Outputs() const { return supports_.size(); }
        [[nodiscard]] size_t ColorCount() const { return colorRows_.size(); }
        [[nodiscard]] const Vector_<size_t>& RowSupport(size_t row) const;
        [[nodiscard]] std::optional<size_t> RowColor(size_t row) const;
        [[nodiscard]] const Vector_<size_t>& ColorRows(size_t color) const;
        [[nodiscard]] size_t ResultBytes() const { return resultBytes_; }
        [[nodiscard]] size_t DirectionBytes() const { return directionBytes_; }
        [[nodiscard]] size_t NumericPayloadBytes() const { return resultBytes_ + directionBytes_; }
    };

    [[nodiscard]] StructuralJacobianPlan_
    PlanStructuralJacobian(size_t inputs, const Vector_<Vector_<size_t>>& rowSupports, const StructuralJacobianSettings_& settings = {});
    [[nodiscard]] StructuralJacobianPlan_ PlanDenseJacobian(size_t inputs, size_t outputs, const StructuralJacobianSettings_& settings = {});
    [[nodiscard]] Matrix_<> RecoverStructuralJacobian(const StructuralJacobianPlan_& plan, const Matrix_<>& colorGradients);
} // namespace Dal::AAD

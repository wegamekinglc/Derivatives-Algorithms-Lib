//
// Created by Codex on 2026/10/8.
//

#pragma once

#include <cmath>
#include <limits>

#include <dal/math/matrix/matrixs.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal::PDE::SampledThetaDetail {
    inline double Product(double left, double right, const char* message) {
        const double value = left * right;
        REQUIRE(std::isfinite(value) && (value != 0.0 || left == 0.0 || right == 0.0), message);
        return value;
    }

    inline double Sum(double left, double right, const char* message) {
        const double value = left + right;
        REQUIRE(std::isfinite(value), message);
        return value;
    }

    inline double Quotient(double numerator, double denominator, const char* message) {
        const double value = numerator / denominator;
        REQUIRE(std::isfinite(value) && (value != 0.0 || numerator == 0.0), message);
        return value;
    }

    class TridiagonalFactors_ {
        Vector_<> lower_, diagonal_, upper_, secondUpper_;
        Vector_<int> swaps_;
        double scale_ = 0.0;

        void FactorColumn(int row, double tolerance);
        void ForwardColumn(int column, Matrix_<>* result) const;
        void TransposeColumn(int column, Matrix_<>* result) const;
        void ScaleColumn(int column, Matrix_<>* result) const;
        [[nodiscard]] bool NeedsLateScaling(int column, const Matrix_<>& rhs) const;

    public:
        TridiagonalFactors_() = default;
        TridiagonalFactors_(Vector_<> lower, Vector_<> diagonal, Vector_<> upper, double tolerance);
        [[nodiscard]] Matrix_<> Solve(const Matrix_<>& rhs, bool transpose) const;
    };
} // namespace Dal::PDE::SampledThetaDetail

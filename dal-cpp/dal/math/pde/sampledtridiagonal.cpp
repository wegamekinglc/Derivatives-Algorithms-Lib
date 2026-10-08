//
// Created by Codex on 2026/10/8.
//

#include <dal/platform/platform.hpp>

#include <algorithm>
#include <utility>

#include <dal/math/pde/sampledthetastepinternal.hpp>

namespace Dal::PDE::SampledThetaDetail {
    namespace {
        constexpr const char* FACTOR_RANGE = "Sampled theta-step tridiagonal factorization loses numerical range";
        constexpr const char* SOLVE_RANGE = "Sampled theta-step tridiagonal substitution loses numerical range";

        double MatrixScale(const Vector_<>& lower, const Vector_<>& diagonal, const Vector_<>& upper) {
            double scale = 0.0;
            for (const auto* values : {&lower, &diagonal, &upper})
                for (double value : *values) {
                    REQUIRE(std::isfinite(value), "Sampled theta-step tridiagonal entries must be finite");
                    scale = std::max(scale, std::abs(value));
                }
            REQUIRE(scale > 0.0, "Sampled theta-step tridiagonal matrix is singular");
            return scale;
        }
    } // namespace

    TridiagonalFactors_::TridiagonalFactors_(Vector_<> lower, Vector_<> diagonal, Vector_<> upper, double tolerance)
        : lower_(std::move(lower)), diagonal_(std::move(diagonal)), upper_(std::move(upper)) {
        REQUIRE(!diagonal_.empty() && diagonal_.size() <= static_cast<size_t>(std::numeric_limits<int>::max()),
                "Sampled theta-step tridiagonal size exceeds supported matrix dimensions");
        const int n = static_cast<int>(diagonal_.size());
        REQUIRE(lower_.size() == static_cast<size_t>(n - 1) && upper_.size() == static_cast<size_t>(n - 1),
                "Sampled theta-step tridiagonal shapes are inconsistent");
        REQUIRE(std::isfinite(tolerance) && tolerance > 0.0 && tolerance < 1.0,
                "Sampled theta-step relative pivot tolerance must be finite and strictly between zero and one");
        scale_ = MatrixScale(lower_, diagonal_, upper_);
        secondUpper_ = Vector_<>(std::max(0, n - 2), 0.0);
        swaps_ = Vector_<int>(n - 1, 0);
        for (auto* values : {&lower_, &diagonal_, &upper_})
            for (double& value : *values)
                value = Quotient(value, scale_, FACTOR_RANGE);
        for (int row = 0; row < n - 1; ++row)
            FactorColumn(row, tolerance);
        REQUIRE(std::abs(diagonal_.back()) > tolerance, "Sampled theta-step normalized final pivot is at or below tolerance");
    }

    void TridiagonalFactors_::FactorColumn(int row, double tolerance) {
        REQUIRE(std::max(std::abs(diagonal_[row]), std::abs(lower_[row])) > tolerance,
                "Sampled theta-step normalized pivot is at or below tolerance");
        if (std::abs(diagonal_[row]) >= std::abs(lower_[row])) {
            lower_[row] = Quotient(lower_[row], diagonal_[row], FACTOR_RANGE);
            diagonal_[row + 1] = Sum(diagonal_[row + 1], -Product(lower_[row], upper_[row], FACTOR_RANGE), FACTOR_RANGE);
            return;
        }
        const double multiplier = Quotient(diagonal_[row], lower_[row], FACTOR_RANGE);
        diagonal_[row] = lower_[row];
        lower_[row] = multiplier;
        const double oldUpper = upper_[row];
        upper_[row] = diagonal_[row + 1];
        diagonal_[row + 1] = Sum(oldUpper, -Product(multiplier, diagonal_[row + 1], FACTOR_RANGE), FACTOR_RANGE);
        swaps_[row] = 1;
        if (static_cast<size_t>(row) < secondUpper_.size()) {
            secondUpper_[row] = upper_[row + 1];
            upper_[row + 1] = -Product(multiplier, upper_[row + 1], FACTOR_RANGE);
        }
    }

    void TridiagonalFactors_::ForwardColumn(int column, Matrix_<>* result) const {
        const int n = diagonal_.size();
        for (int row = 0; row < n - 1; ++row) {
            if (swaps_[row] != 0) {
                const double old = (*result)(row, column);
                (*result)(row, column) = (*result)(row + 1, column);
                (*result)(row + 1, column) = Sum(old, -Product(lower_[row], (*result)(row, column), SOLVE_RANGE), SOLVE_RANGE);
            } else {
                (*result)(row + 1, column) = Sum((*result)(row + 1, column), -Product(lower_[row], (*result)(row, column), SOLVE_RANGE), SOLVE_RANGE);
            }
        }
        for (int row = n - 1; row >= 0; --row) {
            double value = (*result)(row, column);
            if (row + 1 < n)
                value = Sum(value, -Product(upper_[row], (*result)(row + 1, column), SOLVE_RANGE), SOLVE_RANGE);
            if (row + 2 < n)
                value = Sum(value, -Product(secondUpper_[row], (*result)(row + 2, column), SOLVE_RANGE), SOLVE_RANGE);
            (*result)(row, column) = Quotient(value, diagonal_[row], SOLVE_RANGE);
        }
    }

    void TridiagonalFactors_::TransposeColumn(int column, Matrix_<>* result) const {
        const int n = diagonal_.size();
        for (int row = 0; row < n; ++row) {
            double value = (*result)(row, column);
            if (row > 0)
                value = Sum(value, -Product(upper_[row - 1], (*result)(row - 1, column), SOLVE_RANGE), SOLVE_RANGE);
            if (row > 1)
                value = Sum(value, -Product(secondUpper_[row - 2], (*result)(row - 2, column), SOLVE_RANGE), SOLVE_RANGE);
            (*result)(row, column) = Quotient(value, diagonal_[row], SOLVE_RANGE);
        }
        for (int row = n - 2; row >= 0; --row) {
            const double lowerValue = (*result)(row + 1, column);
            const double value = Sum((*result)(row, column), -Product(lower_[row], lowerValue, SOLVE_RANGE), SOLVE_RANGE);
            if (swaps_[row] != 0) {
                (*result)(row, column) = lowerValue;
                (*result)(row + 1, column) = value;
            } else {
                (*result)(row, column) = value;
            }
        }
    }

    void TridiagonalFactors_::ScaleColumn(int column, Matrix_<>* result) const {
        for (int row = 0; row < result->Rows(); ++row)
            (*result)(row, column) = Quotient((*result)(row, column), scale_, "Sampled theta-step tridiagonal scaling loses numerical range");
    }

    bool TridiagonalFactors_::NeedsLateScaling(int column, const Matrix_<>& rhs) const {
        const double threshold = scale_ * std::numeric_limits<double>::min();
        return std::any_of(rhs.Col(column).begin(), rhs.Col(column).end(),
                           [threshold](double value) { return value != 0.0 && std::abs(value) < threshold; });
    }

    Matrix_<> TridiagonalFactors_::Solve(const Matrix_<>& rhs, bool transpose) const {
        REQUIRE(static_cast<size_t>(rhs.Rows()) == diagonal_.size() && rhs.Cols() > 0 && !diagonal_.empty(),
                "Sampled theta-step tridiagonal RHS shape is inconsistent");
        for (double value : rhs)
            REQUIRE(std::isfinite(value), "Sampled theta-step tridiagonal RHS entries must be finite");
        Matrix_<> result(rhs);
        for (int column = 0; column < rhs.Cols(); ++column) {
            const bool lateScaling = NeedsLateScaling(column, rhs);
            if (!lateScaling)
                ScaleColumn(column, &result);
            if (transpose)
                TransposeColumn(column, &result);
            else
                ForwardColumn(column, &result);
            if (lateScaling)
                ScaleColumn(column, &result);
        }
        return result;
    }
} // namespace Dal::PDE::SampledThetaDetail

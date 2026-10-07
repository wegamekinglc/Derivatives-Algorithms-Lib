//
// Created by Codex on 2026/10/7.
//

#include <dal/platform/platform.hpp>

#include <algorithm>
#include <cmath>

#include <dal/math/matrix/linearsolvediagnostics.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal {
    namespace {
        struct ScaledProduct_ {
            double fraction_;
            double roundoff_;
            int exponent_;

            ScaledProduct_(double left, double right) : fraction_(0.0), roundoff_(0.0), exponent_(std::numeric_limits<int>::lowest()) {
                if (left != 0.0 && right != 0.0) {
                    int leftExponent, rightExponent;
                    const double leftFraction = std::frexp(left, &leftExponent);
                    const double rightFraction = std::frexp(right, &rightExponent);
                    fraction_ = leftFraction * rightFraction;
                    roundoff_ = std::fma(leftFraction, rightFraction, -fraction_);
                    exponent_ = leftExponent + rightExponent;
                }
            }

            [[nodiscard]] double AtExponent(int exponent) const { return fraction_ == 0.0 ? 0.0 : std::scalbn(fraction_, exponent_ - exponent); }
            [[nodiscard]] double RoundoffAtExponent(int exponent) const {
                return roundoff_ == 0.0 ? 0.0 : std::scalbn(roundoff_, exponent_ - exponent);
            }
        };

        class CompensatedSum_ {
            double sum_ = 0.0;
            double correction_ = 0.0;

        public:
            void Add(double value) {
                const double next = sum_ + value;
                correction_ += std::abs(sum_) >= std::abs(value) ? (sum_ - next) + value : (value - next) + sum_;
                sum_ = next;
            }
            [[nodiscard]] double Value() const { return sum_ + correction_; }
        };

        void ValidateFinite(const Matrix_<>& matrix, const char* message) {
            for (double value : matrix)
                REQUIRE(std::isfinite(value), message);
        }

        double RowBackwardError(const SquareMatrix_<>& matrix, const Matrix_<>& rhs, const Matrix_<>& solution, int row, int column) {
            const ScaledProduct_ rightHandSide(rhs(row, column), 1.0);
            int exponent = rightHandSide.exponent_;
            for (int entry = 0; entry < matrix.Rows(); ++entry)
                exponent = std::max(exponent, ScaledProduct_(matrix(row, entry), solution(entry, column)).exponent_);
            if (exponent == std::numeric_limits<int>::lowest())
                return 0.0;
            const double b = rightHandSide.AtExponent(exponent);
            CompensatedSum_ residual;
            residual.Add(-b);
            double denominator = std::abs(b);
            for (int entry = 0; entry < matrix.Rows(); ++entry) {
                const ScaledProduct_ product(matrix(row, entry), solution(entry, column));
                const double value = product.AtExponent(exponent);
                residual.Add(value);
                residual.Add(product.RoundoffAtExponent(exponent));
                denominator += std::abs(value);
            }
            return std::min(1.0, std::abs(residual.Value()) / denominator);
        }

        double InfinityNorm(const Matrix_<>& matrix, double scale) {
            double norm = 0.0;
            for (int row = 0; row < matrix.Rows(); ++row) {
                double sum = 0.0;
                for (int column = 0; column < matrix.Cols(); ++column)
                    sum += std::abs(matrix(row, column) / scale);
                norm = std::max(norm, sum);
            }
            return norm;
        }
    } // namespace

    Vector_<> LinearSolveBackwardErrors(const SquareMatrix_<>& matrix, const Matrix_<>& rhs, const Matrix_<>& solution) {
        REQUIRE(matrix.Rows() > 0 && rhs.Rows() == matrix.Rows() && rhs.Cols() > 0,
                "Linear solve backward error requires a nonempty square matrix and compatible RHS");
        REQUIRE(solution.Rows() == rhs.Rows() && solution.Cols() == rhs.Cols(), "Linear solve backward error solution shape must match the RHS");
        ValidateFinite(matrix, "Linear solve backward error matrix entries must be finite");
        ValidateFinite(rhs, "Linear solve backward error RHS entries must be finite");
        ValidateFinite(solution, "Linear solve backward error solution entries must be finite");
        Vector_<> errors(rhs.Cols(), 0.0);
        for (int column = 0; column < rhs.Cols(); ++column)
            for (int row = 0; row < matrix.Rows(); ++row)
                errors[column] = std::max(errors[column], RowBackwardError(matrix, rhs, solution, row, column));
        return errors;
    }

    DiagnosedLinearSolve_::DiagnosedLinearSolve_(const SquareMatrix_<>& matrix, const Matrix_<>& rhs, double relativePivotTolerance)
        : solve_(matrix, rhs, relativePivotTolerance) {
        diagnostics_.componentwiseBackwardErrors_ = LinearSolveBackwardErrors(matrix, rhs, solve_.Solution());
        SquareMatrix_<> identity(matrix.Rows());
        for (int row = 0; row < matrix.Rows(); ++row)
            identity(row, row) = solve_.scale_;
        const auto inverse = solve_.Solve(identity, false);
        double inverseScale = 0.0;
        for (double value : inverse)
            inverseScale = std::max(inverseScale, std::abs(value));
        REQUIRE(inverseScale > 0.0, "Linear solve diagnostic inverse norm must be positive");
        const double matrixNorm = InfinityNorm(matrix, solve_.scale_);
        const double inverseNorm = InfinityNorm(inverse, inverseScale);
        diagnostics_.reciprocalConditionInfinity_ = std::min(1.0, ((1.0 / matrixNorm) / inverseNorm) / inverseScale);
    }
} // namespace Dal

//
// Created by Codex on 2026/10/8.
//

#pragma once

#include <algorithm>
#include <cmath>
#include <limits>

namespace Dal::LinearSolveDetail {
    struct ScaledProduct_ {
        double fraction_ = 0.0;
        double roundoff_ = 0.0;
        int exponent_ = std::numeric_limits<int>::lowest();

        ScaledProduct_(double left, double right) {
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
        [[nodiscard]] double RoundoffAtExponent(int exponent) const { return roundoff_ == 0.0 ? 0.0 : std::scalbn(roundoff_, exponent_ - exponent); }
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

    template <class ProductAt_> double PhysicalRowBackwardError(double rhs, int entries, const ProductAt_& productAt) {
        const ScaledProduct_ rightHandSide(rhs, 1.0);
        int exponent = rightHandSide.exponent_;
        for (int entry = 0; entry < entries; ++entry)
            exponent = std::max(exponent, productAt(entry).exponent_);
        if (exponent == std::numeric_limits<int>::lowest())
            return 0.0;
        // Headroom retains representable tiny ratios until the final division.
        exponent -= std::numeric_limits<double>::digits;
        const double b = rightHandSide.AtExponent(exponent);
        CompensatedSum_ residual;
        residual.Add(-b);
        double denominator = std::abs(b);
        for (int entry = 0; entry < entries; ++entry) {
            const auto product = productAt(entry);
            const double value = product.AtExponent(exponent);
            residual.Add(value);
            residual.Add(product.RoundoffAtExponent(exponent));
            denominator += std::abs(value);
        }
        return std::min(1.0, std::abs(residual.Value()) / denominator);
    }
} // namespace Dal::LinearSolveDetail

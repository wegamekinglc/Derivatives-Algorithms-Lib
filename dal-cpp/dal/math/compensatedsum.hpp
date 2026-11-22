#pragma once

#include <cmath>

namespace Dal::Detail {
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
} // namespace Dal::Detail

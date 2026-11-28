#pragma once

#include <cmath>
#include <limits>
#include <string>

#include <dal/math/aad/secondorder.hpp>

namespace Dal::DupireSecondOrderDetail {
    using Reason = AAD::SecondOrderErrorReason_::Value_;
    [[noreturn]] void ValidationError(Reason reason, const std::string& message);

    // The center follows the active kernel's binary64 operation order.
    // The radius is a separate, outward-inflated absolute error estimate.
    struct Bound_ {
        double value_, error_;
        Bound_(double value = 0.0, double error = 0.0) : value_(value), error_(error) {}

        [[nodiscard]] double Lower() const { return std::nextafter(value_ - error_, -std::numeric_limits<double>::infinity()); }
        [[nodiscard]] double Upper() const { return std::nextafter(value_ + error_, std::numeric_limits<double>::infinity()); }
        Bound_& operator-=(Bound_ rhs);
        Bound_& operator*=(Bound_ rhs);
    };

    inline constexpr double EPSILON = std::numeric_limits<double>::epsilon();
    inline double Roundoff(double value) { return EPSILON * std::abs(value) + std::numeric_limits<double>::denorm_min(); }
    inline double Inflate(double radius) { return std::nextafter(radius * (1.0 + 8.0 * EPSILON), std::numeric_limits<double>::infinity()); }
    inline Bound_ operator+(Bound_ a, Bound_ b) {
        const double value = a.value_ + b.value_;
        return {value, Inflate(a.error_ + b.error_ + Roundoff(value))};
    }
    inline Bound_ operator-(Bound_ a, Bound_ b) { return a + Bound_{-b.value_, b.error_}; }
    inline Bound_ operator*(Bound_ a, Bound_ b) {
        const double value = a.value_ * b.value_;
        return {value, Inflate(std::abs(a.value_) * b.error_ + std::abs(b.value_) * a.error_ + a.error_ * b.error_ + Roundoff(value))};
    }
    inline Bound_ operator/(Bound_ a, Bound_ b) {
        const double value = a.value_ / b.value_;
        const double lower = Bound_(std::abs(b.value_), b.error_).Lower();
        if (!(lower > 0.0))
            return {value, std::numeric_limits<double>::infinity()};
        return {value, Inflate((a.error_ + (std::abs(value) + Roundoff(value)) * b.error_) / lower + Roundoff(value))};
    }
    inline Bound_ sqrt(Bound_ arg) {
        const double value = std::sqrt(arg.value_);
        return {value, Inflate(arg.error_ / std::sqrt(arg.Lower()) + Roundoff(value))};
    }
    inline Bound_ erfc(Bound_ arg) {
        const double value = std::erfc(arg.value_);
        // erfc is 2-Lipschitz. The libm allowance is absolute over [0,2].
        return {value, Inflate(2.0 * arg.error_ + 8.0 * EPSILON)};
    }
    inline Bound_& Bound_::operator-=(Bound_ rhs) { return *this = *this - rhs; }
    inline Bound_& Bound_::operator*=(Bound_ rhs) { return *this = *this * rhs; }

    struct Margins_;
    void CertifyCalibration(Bound_ value, const char* field, int strike, int time, double& minimum);
    void CertifyRow(Bound_ lower, Bound_ diagonal, Bound_ upper, int step, int space, Margins_& margins);
    void CertifyActualPivot(double value, int step, int space, double& minimum);
} // namespace Dal::DupireSecondOrderDetail

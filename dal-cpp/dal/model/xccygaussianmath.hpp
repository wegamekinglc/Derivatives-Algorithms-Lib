#pragma once

#include <array>
#include <cmath>
#include <limits>
#include <type_traits>

#include <dal/calibration/protocol.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/math/operators.hpp>

namespace Dal::XccyGaussianDetail {
    void Check(bool valid, const String_& code, const String_& field, const String_& expected, const String_& actual = "");
    void CheckAtDate(bool valid, const String_& field, const Date_& date, double actual);
    [[noreturn]] void RethrowAtDate(const CalibrationError_& error, const Date_& from, const Date_& to);

    inline void NumericalCheck(bool valid, const String_& field) {
        Check(valid, "CalibrationNumericalFailure", "/model/transition/" + field, "finite, reliably positive Gaussian kernels");
    }

    // Wider intermediates protect cancellation in small-reversion covariance kernels.
    inline long double ExpIntegral(long double x) { return x == 0.L ? 1.L : -std::expm1(-x) / x; }

    // Integral of u^n exp(-x*u), u in [0,1]. The positive series avoids subtractive recurrence near zero.
    inline long double ExpMoment(int n, long double x) {
        if (x > 50.L) {
            long double result = ExpIntegral(x);
            for (int i = 1; i <= n; ++i)
                result = (i * result - std::exp(-x)) / x;
            return result;
        }
        long double term = 1.L / (n + 1), sum = term;
        for (int k = 1; k < 512; ++k) {
            term *= x / (n + k + 1);
            sum += term;
            if (term <= std::numeric_limits<long double>::epsilon() * sum)
                break;
        }
        return std::exp(-x) * sum;
    }

    inline long double H(long double a, long double h) { return h * ExpIntegral(a * h); }

    inline long double HMoment(int n, long double x) {
        if (x > .5L)
            return (1.L / (n + 1) - ExpMoment(n, x)) / x;
        long double coefficient = 1.L, sum = 0.L;
        for (int k = 0; k < 24; ++k) {
            sum += coefficient / (n + k + 2);
            coefficient *= -x / (k + 2);
        }
        return sum;
    }

    inline long double ExpH(long double x, long double y) {
        if (y > .5L)
            return (ExpIntegral(x) - std::exp(-x) * ExpIntegral(y)) / (x + y);
        long double coefficient = 1.L, sum = 0.L;
        for (int k = 0; k < 24; ++k) {
            sum += coefficient * ExpMoment(k + 1, x);
            coefficient *= -y / (k + 2);
        }
        return sum;
    }

    inline long double HH(long double x, long double y) {
        if (x < y)
            std::swap(x, y);
        if (y > .5L)
            return (1.L - ExpIntegral(x) - ExpIntegral(y) + ExpIntegral(x + y)) / x / y;
        long double coefficient = 1.L, sum = 0.L;
        for (int k = 0; k < 24; ++k) {
            sum += coefficient * HMoment(k + 1, x);
            coefficient *= -y / (k + 2);
        }
        return sum;
    }

    inline int Driver(int slot) { return slot == 4 ? 2 : slot % 2; }

    inline long double KernelWithConstant(int slot, long double a, long double h) {
        if (slot == 4)
            return h;
        return slot < 2 ? H(a, h) : h * h * HMoment(0, a * h);
    }

    inline long double KernelProduct(int i, int j, long double ad, long double af, long double h) {
        if (i > j)
            std::swap(i, j);
        const std::array<long double, 3> reversions{ad, af, 0.L};
        const long double ai = reversions[Driver(i)], aj = reversions[Driver(j)];
        if (j == 4)
            return KernelWithConstant(i, ai, h);
        if (j < 2)
            return H(ai + aj, h);
        if (i < 2)
            return h * h * ExpH(ai * h, aj * h);
        return h * h * h * HH(ai * h, aj * h);
    }

    template <class T_> std::array<T_, 5> ZeroState() {
        std::array<T_, 5> result;
        for (auto& value : result)
            value = T_(0.);
        return result;
    }

    template <class T_> struct Transition_ {
        Matrix_<> flow_{5, 5, 0.0};
        std::array<T_, 5> shift_ = ZeroState<T_>();
        Matrix_<T_> covariance_{5, 5, T_(0.0)};
    };

    inline void ValidateInterval(double ad, double af, double h) {
        for (const double value : {ad, af, h})
            NumericalCheck(std::isfinite(value) && value >= 0., "interval");
    }

    template <class T_> T_ CovarianceEntry(const std::array<T_, 3>& sigma, const Matrix_<>& correlation, int i, int j, double integral) {
        T_ value;
        if constexpr (std::is_same_v<T_, double>)
            value = static_cast<double>(static_cast<long double>(sigma[Driver(i)]) * sigma[Driver(j)] * correlation(Driver(i), Driver(j)) * integral);
        else
            value = sigma[Driver(i)] * sigma[Driver(j)] * correlation(Driver(i), Driver(j)) * integral;
        NumericalCheck(std::isfinite(Value(value)), "covariance");
        if (i == j && integral > 0. && Value(sigma[Driver(i)]) > 0.)
            NumericalCheck(Value(value) > 0., "varianceUnderflow");
        return value;
    }

    template <class T_>
    Transition_<T_> ConstantTransition(double ad, double af, const std::array<T_, 3>& sigma, const Matrix_<>& correlation, double h) {
        ValidateInterval(ad, af, h);
        Transition_<T_> result;
        for (int i = 0; i < 5; ++i)
            result.flow_(i, i) = 1.;
        result.flow_(0, 0) = std::exp(-ad * h);
        result.flow_(1, 1) = std::exp(-af * h);
        result.flow_(2, 0) = static_cast<double>(H(ad, h));
        result.flow_(3, 1) = static_cast<double>(H(af, h));
        const T_ drift = -correlation(1, 2) * sigma[1] * sigma[2];
        result.shift_[1] = drift * static_cast<double>(H(af, h));
        result.shift_[3] = drift * static_cast<double>(KernelProduct(3, 4, ad, af, h));
        for (int i = 0; i < 5; ++i)
            for (int j = 0; j <= i; ++j) {
                const double integral = static_cast<double>(KernelProduct(i, j, ad, af, h));
                NumericalCheck(std::isfinite(integral) && (h == 0. || integral > 0.), "kernel");
                result.covariance_(i, j) = CovarianceEntry(sigma, correlation, i, j, integral);
                result.covariance_(j, i) = result.covariance_(i, j);
            }
        return result;
    }

    template <class T_> Transition_<T_> Compose(const Transition_<T_>& first, const Transition_<T_>& second) {
        Transition_<T_> result;
        for (int i = 0; i < 5; ++i) {
            result.shift_[i] = second.shift_[i];
            for (int k = 0; k < 5; ++k)
                result.shift_[i] += second.flow_(i, k) * first.shift_[k];
            for (int j = 0; j < 5; ++j) {
                for (int k = 0; k < 5; ++k)
                    result.flow_(i, j) += second.flow_(i, k) * first.flow_(k, j);
            }
            for (int j = 0; j <= i; ++j) {
                result.covariance_(i, j) = second.covariance_(i, j);
                for (int k = 0; k < 5; ++k) {
                    for (int l = 0; l < 5; ++l)
                        result.covariance_(i, j) += second.flow_(i, k) * first.covariance_(k, l) * second.flow_(j, l);
                }
                result.covariance_(j, i) = result.covariance_(i, j);
            }
        }
        return result;
    }

    template <class W_, class T_> std::array<W_, 5> CovarianceScales(const Matrix_<T_>& covariance) {
        std::array<W_, 5> scales{};
        for (int i = 0; i < 5; ++i) {
            NumericalCheck(std::isfinite(Value(covariance(i, i))) && Value(covariance(i, i)) >= 0., "variance");
            if (Value(covariance(i, i)) > 0.)
                scales[i] = Dal::sqrt(W_(covariance(i, i)));
        }
        return scales;
    }

    template <class W_, class T_>
    W_ FactorEntry(const Matrix_<T_>& covariance, const Matrix_<W_>& normalized, const std::array<W_, 5>& scales, int i, int j) {
        NumericalCheck(std::isfinite(Value(covariance(i, j))) && Value(covariance(i, j)) == Value(covariance(j, i)), "symmetry");
        if (scales[i] == 0. || scales[j] == 0.) {
            NumericalCheck(Value(covariance(i, j)) == 0., "nullRow");
            return W_(0.);
        }
        W_ residual = W_(covariance(i, j)) / scales[i] / scales[j];
        for (int k = 0; k < j; ++k)
            residual -= normalized(i, k) * normalized(j, k);
        if (i == j) {
            // Rounded covariance entries cannot certify a pivot at their own cancellation scale.
            NumericalCheck(residual > 64. * std::numeric_limits<double>::epsilon(), "uncertainPivot");
            return Dal::sqrt(residual);
        }
        return residual / normalized(j, j);
    }

    template <class T_> Matrix_<T_> ScaledFactor(const Matrix_<T_>& covariance) {
        NumericalCheck(covariance.Rows() == 5 && covariance.Cols() == 5, "dimensions");
        using W_ = std::conditional_t<std::is_same_v<T_, double>, long double, T_>;
        const auto scales = CovarianceScales<W_>(covariance);
        Matrix_<W_> normalized(5, 5, W_(0.0));
        for (int i = 0; i < 5; ++i)
            for (int j = 0; j <= i; ++j)
                normalized(i, j) = FactorEntry(covariance, normalized, scales, i, j);
        Matrix_<T_> result(5, 5, T_(0.0));
        for (int i = 0; i < 5; ++i)
            for (int j = 0; j <= i; ++j)
                result(i, j) = T_(scales[i] * normalized(i, j));
        return result;
    }
} // namespace Dal::XccyGaussianDetail

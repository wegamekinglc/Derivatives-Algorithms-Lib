#include <cmath>

#include <dal/math/normalcentral.hpp>
#include <dal/math/specialfunctions.hpp>
#include <dal/platform/platform.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal {
    namespace {
        struct Scalar_ {
            using Vector_ = double;
            static double Broadcast(double x) { return x; }
            static double Subtract(double a, double b) { return a - b; }
            static double Multiply(double a, double b) { return a * b; }
            static double Divide(double a, double b) { return a / b; }
            static double MultiplyAdd(double a, double b, double c) {
#if defined(DAL_NORMAL_FMA)
                return std::fma(a, b, c);
#else
                return a * b + c;
#endif
            }
        };
    } // namespace

    double InverseNCDF(double x, bool precise, bool polish) {

        REQUIRE(x >= 0.0 && x <= 1.0, "x should be in bound [0, 1]");

        static const double c1_ = -7.784894002430293e-03;
        static const double c2_ = -3.223964580411365e-01;
        static const double c3_ = -2.400758277161838e+00;
        static const double c4_ = -2.549732539343734e+00;
        static const double c5_ = 4.374664141464968e+00;
        static const double c6_ = 2.938163982698783e+00;
        static const double d1_ = 7.784695709041462e-03;
        static const double d2_ = 3.224671290700398e-01;
        static const double d3_ = 2.445134137142996e+00;
        static const double d4_ = 3.754408661907416e+00;

        static const double x_low_ = 0.02425;
        static const double x_high_ = 1.0 - x_low_;

        double z;
        const auto TailRational = [&](double p) {
            const double r = std::sqrt(-2.0 * std::log(p));
            return (((((c1_ * r + c2_) * r + c3_) * r + c4_) * r + c5_) * r + c6_) / ((((d1_ * r + d2_) * r + d3_) * r + d4_) * r + 1.0);
        };

        if (x < x_low_ || x_high_ < x) {
            z = x < x_low_ ? TailRational(x) : -TailRational(1.0 - x);
        } else {
            z = Math::Detail::CentralInverse<Scalar_>(x);
        }

        if (polish) {
            const double err = NCDF(z, precise) - x;
            z -= err * M_SQRT_2_PI * std::exp(std::min(8.0, 0.5 * Square(z)));
        }
        return z;
    }
} // namespace Dal

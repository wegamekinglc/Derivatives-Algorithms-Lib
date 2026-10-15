//
// Created by Codex on 2026/10/16.
//

#if defined(_MSC_VER)
#pragma fp_contract(off)
#endif

#include <dal/math/normalcentral.hpp>
#include <dal/math/specialfunctions.hpp>
#include <dal/platform/platform.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal {
    double InverseNCDF(double x, bool precise, bool polish) {

        REQUIRE(x >= 0.0 && x <= 1.0, "x should be in bound [0, 1]");

        static const double a1_ = Math::Detail::NORMAL_A[0];
        static const double a2_ = Math::Detail::NORMAL_A[1];
        static const double a3_ = Math::Detail::NORMAL_A[2];
        static const double a4_ = Math::Detail::NORMAL_A[3];
        static const double a5_ = Math::Detail::NORMAL_A[4];
        static const double a6_ = Math::Detail::NORMAL_A[5];
        static const double b1_ = Math::Detail::NORMAL_B[0];
        static const double b2_ = Math::Detail::NORMAL_B[1];
        static const double b3_ = Math::Detail::NORMAL_B[2];
        static const double b4_ = Math::Detail::NORMAL_B[3];
        static const double b5_ = Math::Detail::NORMAL_B[4];
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
            z = x - 0.5;
            double r = z * z;
            z = (((((a1_ * r + a2_) * r + a3_) * r + a4_) * r + a5_) * r + a6_) * z / (((((b1_ * r + b2_) * r + b3_) * r + b4_) * r + b5_) * r + 1.0);
        }

        if (polish) {
            const double err = NCDF(z, precise) - x;
            z -= err * M_SQRT_2_PI * std::exp(std::min(8.0, 0.5 * Square(z)));
        }
        return z;
    }
} // namespace Dal

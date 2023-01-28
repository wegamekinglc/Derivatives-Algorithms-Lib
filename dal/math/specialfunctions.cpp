//
// Created by wegam on 2020/12/16.
//

#include <cmath>
#include <dal/platform/platform.hpp>
#include <dal/math/interp/interpcubic.hpp>
#include <dal/math/specialfunctions.hpp>
#include <dal/math/vectors.hpp>
#include <dal/platform/strict.hpp>


namespace Dal {
    namespace {
        constexpr double MIN_SPLINE_X = -3.734582185;
        constexpr double MIN_SPLINE_F = 9.47235E-05;
        Interp1_* MakeNcdfSpline() {
            const Vector_<> x = {MIN_SPLINE_X, -3.347382781, -3.030883722, -2.75090681,  -2.492289824, -2.243141537,
                                 -1.992179668, -1.494029881, -1.290815576, -1.120050999, -0.954303629, -0.792072249,
                                 -0.629093487, -0.460389924, -0.276889742, 0.0};
            const Vector_<> f = {MIN_SPLINE_F, 0.000408582, 0.001219907, 0.002972237, 0.00634685, 0.012444548,
                                 0.023176395,  0.067583453, 0.098383227, 0.131345731, 0.16996458, 0.214158839,
                                 0.264643073,  0.322617682, 0.39093184,  0.5};
            const Interp::Boundary_ lhs(1, 0.000373538);
            const Interp::Boundary_ rhs(1, 0.39898679); // at x=0
            return Interp::NewCubic(String_(), x, f, lhs, rhs);
        }

        double NcdfBySpline(double z) {
            static const scoped_ptr<Interp1_> SPLINE(MakeNcdfSpline());
            if (z > 0.0)
                return 1.0 - NcdfBySpline(-z);
            return z < MIN_SPLINE_X
                       ? MIN_SPLINE_F * std::exp(-1.1180061 * (Square(z) - Square(MIN_SPLINE_X)))
                       : (*SPLINE)(z);
        }
    } // namespace

    double NCDF(double z, bool precise) { return precise ? 0.5 * erfc(-z / M_SQRT_2) : NcdfBySpline(z); }

    double InverseNCDF(double x, bool precise, bool polish) {

        REQUIRE(x >= 0.0 && x <= 1.0, "x should be in bound [0, 1]");

        static const double INV_NORM = 2.5066282746310002;
        static const double a1_ = -3.969683028665376e+01;
        static const double a2_ = 2.209460984245205e+02;
        static const double a3_ = -2.759285104469687e+02;
        static const double a4_ = 1.383577518672690e+02;
        static const double a5_ = -3.066479806614716e+01;
        static const double a6_ = 2.506628277459239e+00;
        static const double b1_ = -5.447609879822406e+01;
        static const double b2_ = 1.615858368580409e+02;
        static const double b3_ = -1.556989798598866e+02;
        static const double b4_ = 6.680131188771972e+01;
        static const double b5_ = -1.328068155288572e+01;
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
        if (x < x_low_ || x_high_ < x) {
            if (x < x_low_) {
                // Rational approximation for the lower region 0<x<u_low
                z = std::sqrt(-2.0 * std::log(x));
                z = (((((c1_ * z + c2_) * z + c3_) * z + c4_) * z + c5_) * z + c6_) /
                    ((((d1_ * z + d2_) * z + d3_) * z + d4_) * z + 1.0);
            } else {
                // Rational approximation for the upper region u_high<x<1
                z = std::sqrt(-2.0 * std::log(1.0 - x));
                z = -(((((c1_ * z + c2_) * z + c3_) * z + c4_) * z + c5_) * z + c6_) /
                    ((((d1_ * z + d2_) * z + d3_) * z + d4_) * z + 1.0);
            }
        } else {
            z = x - 0.5;
            double r = z * z;
            z = (((((a1_ * r + a2_) * r + a3_) * r + a4_) * r + a5_) * r + a6_) * z /
                (((((b1_ * r + b2_) * r + b3_) * r + b4_) * r + b5_) * r + 1.0);
        }

        if (polish) {
            const double err = NCDF(z, precise) - x;
            z -= err * INV_NORM * std::exp(std::min(8.0, 0.5 * Square(z)));
        }
        return z;
    }
} // namespace Dal
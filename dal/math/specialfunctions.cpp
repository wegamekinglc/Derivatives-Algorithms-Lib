//
// Created by wegam on 2020/12/16.
//

#include <cmath>
#include <dal/math/interp/interpcubic.hpp>
#include <dal/math/specialfunctions.hpp>
#include <dal/math/vectors.hpp>
#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/string/strings.hpp>

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
            return z < MIN_SPLINE_X ? MIN_SPLINE_F * exp(-1.1180061 * (Square(z) - Square(MIN_SPLINE_X)))
                                    : (*SPLINE)(z);
        }
    } // namespace

    double NCDF(double z, bool precise) { return precise ? 0.5 * erfc(-z / M_SQRT_2) : NcdfBySpline(z); }

    double InverseNCDF(double x, bool precise, bool polish) {

        REQUIRE(x >= 0.0 && x <= 1.0, "x should be in bound [0, 1]");

        static const double INV_NORM = sqrt(2.0 * PI);
        static const double a0_ = 2.50662823884;
        static const double a1_ = -18.61500062529;
        static const double a2_ = 41.39119773534;
        static const double a3_ = -25.44106049637;

        static const double b0_ = -8.47351093090;
        static const double b1_ = 23.08336743743;
        static const double b2_ = -21.06224101826;
        static const double b3_ = 3.13082909833;

        static const double c0_ = 0.3374754822726147;
        static const double c1_ = 0.9761690190917186;
        static const double c2_ = 0.1607979714918209;
        static const double c3_ = 0.0276438810333863;
        static const double c4_ = 0.0038405729373609;
        static const double c5_ = 0.0003951896511919;
        static const double c6_ = 0.0000321767881768;
        static const double c7_ = 0.0000002888167364;
        static const double c8_ = 0.0000003960315187;

        double ret_val;
        double temp = x - 0.5;

        if (std::fabs(temp) < 0.42) {
            // Beasley and Springer, 1977
            ret_val = temp * temp;
            ret_val = temp * (((a3_ * ret_val + a2_) * ret_val + a1_) * ret_val + a0_) /
                      ((((b3_ * ret_val + b2_) * ret_val + b1_) * ret_val + b0_) * ret_val + 1.0);
        } else {
            // improved approximation for the tail (Moro 1995)
            if (x < 0.5)
                ret_val = x;
            else
                ret_val = 1.0 - x;
            ret_val = std::log(-std::log(ret_val));
            ret_val =
                c0_ +
                ret_val *
                    (c1_ +
                     ret_val *
                         (c2_ +
                          ret_val *
                              (c3_ +
                               ret_val * (c4_ + ret_val * (c5_ + ret_val * (c6_ + ret_val * (c7_ + ret_val * c8_)))))));
            if (x < 0.5)
                ret_val = -ret_val;
        }

        if (polish) {
            const double err = NCDF(ret_val, precise) - x;
            ret_val -=
                err * INV_NORM * exp(Min(8.0, 0.5 * Square(ret_val))); // cap Exp(x^2) factor in polishing at 4 sigma
        }
        return ret_val;
    }
} // namespace Dal
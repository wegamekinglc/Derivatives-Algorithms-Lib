//
// Created by wegam on 2020/12/16.
//

#include <cmath>
#include <dal/platform/strict.hpp>
#include <dal/math/specialfunctions.hpp>
#include <dal/platform/platform.hpp>
#include <dal/math/interp/interpcubic.hpp>


namespace Dal {
    namespace {
        constexpr double MIN_SPLINE_X = -3.734582185;
        constexpr double MIN_SPLINE_F = 9.47235E-05;
        std::unique_ptr<Interp1_> MakeNcdfSpline() {
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

} // namespace Dal

//
// Created by wegam on 2022/4/3.
//

#include <public/__platform.hpp>
#include <dal/math/smooth.hpp>
#include <dal/math/interp/interp.hpp>
#include <dal/math/interp/interp2d.hpp>
#include <dal/math/interp/interpcubic.hpp>

/*IF--------------------------------------------------------------------------
public Interp1_New_Linear
    Create a linear interpolator
&inputs
name is string
    A name for the object being created
x is number[]
    &IsMonotonic(x)\$ values must be in ascending order
    The x-values (abscissas)
y is number[]
    &$.size() == x.size()\x and y must have the same size
    The values of f(x) at each x
&outputs
f is handle Interp1
    The interpolator
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public Interp1_New_Linear_Smoothed
    Create a linear interpolator
&inputs
name is string
    A name for the object being created
x is number[]
    &IsMonotonic(x)\$ values must be in ascending order
    The x-values (abcissas)
y is number[]
    &$.size() == x.size()\x and y must have the same size
    The values of f(x) at each x
smoothing is number
    &$ >= 0.0
    The weight to put on smoothness of the interpolating function
&optional
fit_weights is number[]
    &$.empty() || $.size() == x.size()\must have one $ for each y
    The weight to attach to accuracy of fit for each y_i; default is 1.0 for all
&outputs
f is handle Interp1
    The interpolator
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public Interp1_New_Cubic
    Create a cubic-spline interpolator
&inputs
name is string
    A name for the object being created
x is number[]
    &IsMonotonic(x)\$ values must be in ascending order
    The x-values (abscissas)
y is number[]
    &$.size() == x.size()\x and y must have the same size
    The values of f(x) at each x
&optional
boundary_order is integer[]
    &$.size() <= 2\Can only specify two boundary conditions
    &$.empty() || ($.front() > 0 && $.back() > 0 && $.front() <= 3 && $.back() <= 3)\Boundary order must be in the range
    (0, 3] The order of the derivatives specified at the boundary (default is 3); can be a two-element vector of (left,
    right), or a single value for both
boundary_value is number[]
    &$.size() <= 2\Can only specify two boundary conditions
    &$.empty() || !boundary_order.empty()\Can't specify boundary value without specifying order
    The value of whichever derivative is specified at the boundary (default is 0.0); can be a two-element vector of
    (left, right), or a single number for bot
&outputs
f is handle Interp1
    The interpolator
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public Interp1_Get
    Interpolate a value at specified abcissas
&inputs
f is handle Interp1
    The interpolant function
x is number[]
    The x-values (abcissas)
&outputs
y is number[]
    The interpolated function values at x-values
-IF-------------------------------------------------------------------------*/


/*IF--------------------------------------------------------------------------
public Interp2_New_Linear
    Create a linear 2D interpolator
&inputs
name is string
    A name for the object being created
x is number[]
    &IsMonotonic(x)\$ values must be in ascending order
    The x-values (abscissas)
y is number[]
    &IsMonotonic(y)\$ values must be in ascending order
    The y-values (abscissas)
z is number[][]
    &$.Rows() == x.size()\z rows and x must have the same size
    &$.Cols() == y.size()\x columns and y must have the same size
    The values of f(x, y) at each x, y
&outputs
f is handle Interp2
    The interpolator
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public Interp2_Get
    Interpolate 2D a value at specified abcissas
&inputs
f is handle Interp2
    The interpolant function
x is number[]
    The x-values (abcissas)
y is number[]
    The y-values (abcissas)
&outputs
z is number[][]
    The interpolated function values at x-values and y-values
-IF-------------------------------------------------------------------------*/

namespace Dal {
    namespace {
        void Interp1_New_Linear(const String_& name, const Vector_<>& x, const Vector_<>& y, Handle_<Interp1_>* f) {
            f->reset(Interp::NewLinear(name, x, y));
        }

        double CheckedInterp(const Interp1_& f, double x) {
            REQUIRE(f.IsInBounds(x), "X (= " + std::to_string(x) + ") is outside interpolation domain");
            return f(x);
        }

        void Interp1_Get(const Handle_<Interp1_>& f, const Vector_<>& x, Vector_<>* y) {
            *y = Apply([&](double x_i) { return CheckedInterp(*f, x_i); }, x);
        }

        void Interp1_New_Linear_Smoothed(const String_& name, const Vector_<>& x, const Vector_<>& y, double smoothing, const Vector_<>& fit_weights, Handle_<Interp1_>* f) {
            Vector_<> z = SmoothedVals(x, y, fit_weights, smoothing);
            f->reset(Interp::NewLinear(name, x, z));
        }

        void Interp1_New_Cubic(const String_& name,
                               const Vector_<>& x,
                               const Vector_<>& y,
                               const Vector_<int>& boundary_order,
                               const Vector_<>& boundary_value,
                               Handle_<Interp1_>* f) {
            Interp::Boundary_ left(3.0, 0), right(3.0, 0);
            if (!boundary_order.empty()) {
                left.order_ = boundary_order.front();
                right.order_ = boundary_order.back();
                if (!boundary_value.empty()) {
                    left.value_ = boundary_value.front();
                    right.value_ = boundary_value.back();
                }
            }
            f->reset(Interp::NewCubic(name, x, y, left, right));
        }

        double CheckedInterp2(const Interp2_& f, double x, double y) {
            REQUIRE(f.IsInBounds(x, y), "X (= " + std::to_string(x) + ")" + " Y (= " + std::to_string(y) + ") is outside interpolation domain");
            return f(x, y);
        }

        [[noreturn]] void Interp2_Get(const Handle_<Interp2_>& f, const Vector_<>& x, const Vector_<>& y, Matrix_<>* z) {
            for (size_t i = 0; i != x.size(); ++i) {
                for (size_t j = 0; j != y.size(); ++j)
                    (*z)(i, j) = CheckedInterp2(*f, x[i], y[j]);
            }
        }

        void Interp2_New_Linear(const String_& name,
                                const Vector_<>& x,
                                const Vector_<>& y,
                                const Matrix_<>& z,
                                Handle_<Interp2_>* f) {
            f->reset(Interp::NewLinear2(name, x, y, z));
        }
    } // namespace
#ifdef _WIN32
#include <public/auto/MG_Interp1_Get_public.inc>
#include <public/auto/MG_Interp1_New_Cubic_public.inc>
#include <public/auto/MG_Interp1_New_Linear_public.inc>
#include <public/auto/MG_Interp1_New_Linear_Smoothed_public.inc>

#include <public/auto/MG_Interp2_Get_public.inc>
#include <public/auto/MG_Interp2_New_Linear_public.inc>
#endif
} // namespace Dal

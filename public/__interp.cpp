//
// Created by wegam on 2022/4/3.
//

#include <public/__platform.hpp>
#include <dal/math/interp/interp.hpp>

/*IF--------------------------------------------------------------------------
public Interp1_New_Linear
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


namespace Dal {
    namespace {
        void Interp1_New_Linear(const String_& name, const Vector_<>& x, const Vector_<>& y, Handle_<Interp1_>* f) {
            f->reset(new Interp1Linear_(name, x, y));
        }

        double CheckedInterp(const Interp1_& f, double x) {
            REQUIRE(f.IsInBounds(x), "X (= " + std::to_string(x) + ") is outside interpolation domain");
            return f(x);
        }

        void Interp1_Get
            (const Handle_<Interp1_>& f,
             const Vector_<>& x,
             Vector_<>* y) {
            *y = Apply([&](double x_i){return CheckedInterp(*f, x_i); }, x);
        }
    }
#ifdef _WIN32
#include <public/auto/MG_Interp1_New_Linear_public.inc>
#include <public/auto/MG_Interp1_Get_public.inc>
#endif
}

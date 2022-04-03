//
// Created by wegam on 2022/4/3.
//

#include <dal/public/__platform.hpp>
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

namespace Dal {
    namespace {
        void Interp1_New_Linear(const String_& name, const Vector_<>& x, const Vector_<>& y, Handle_<Interp1_>* f) {
            f->reset(new Interp1Linear_(name, x, y));
        }
    }
#ifdef _WIN32
#include <dal/auto/MG_Interp1_New_Linear_public.inc>
#endif
}

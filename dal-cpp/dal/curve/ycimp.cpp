//
// Created by wegam on 2023/3/26.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/curve/ycimp.hpp>
#include <dal/curve/ycpwlf.hpp>
#include <dal/curve/fittable.hpp>
#include <dal/curve/yccomponent.hpp>
#include <dal/curve/discount.hpp>
#include <dal/curve/piecewiselinear.hpp>
#include <dal/time/date.hpp>
#include <dal/time/daybasis.hpp>
#include <dal/math/vectors.hpp>
#include <dal/storage/archive.hpp>

/*IF--------------------------------------------------------------------------
storable DiscountPWLF
   Discount curve based on piecewise linear forward rates
version 1
manual
&members
name is ?string
ccy is ?string
knotDates is date[]
leftVals is number[]
rightVals is number[]
base is ?handle DiscountCurve
-IF-------------------------------------------------------------------------*/

namespace Dal {

    DiscountCurve_* NewDiscountPWLF(const String_ &name,
                                    const String_ &ccy,
                                    const PiecewiseLinear_& fwds,
                                    const Handle_ <DiscountCurve_>& base) {
        return new Tape::DiscountPWLF_<double>(name, ccy, fwds.knotDates_, fwds.fLeft_, fwds.fRight_, base);
    }
    #include <dal/auto/MG_DiscountPWLF_v1_Read.inc>

    Storable_ *DiscountPWLF_v1::Reader_::Build() const {
        return new Tape::DiscountPWLF_<double>(name_, ccy_, knotDates_, leftVals_, rightVals_, base_);
    }
} // namespace Dal

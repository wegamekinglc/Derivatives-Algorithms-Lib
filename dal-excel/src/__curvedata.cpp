//
// Created by wegam on 2026/6/20.
//

#pragma once

#include "__platform.hpp"
#include "__curve_storable.hpp"
#include <dal-public/src/curvedata.hpp>

/*IF--------------------------------------------------------------------------
public DiscountPWLF_New
    Build a discount curve from knot dates and forward rates (piecewise-linear forward)
&inputs
name is string
    A name for the curve
ccy is string
    Currency code (e.g. "USD")
knotDates is date[]
    Knot dates for the curve
fwdRates is number[]
    &$.size() == knotDates.size()\must have one rate per knot date
    Forward rates at each knot date
&optional
base is handle StorableDiscountCurve
    Optional base discount curve
&outputs
curve is handle StorableDiscountCurve
    The discount curve
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DiscountZeroRate_New
    Build a discount curve from continuously compounded zero rates
&inputs
name is string
    A name for the curve
ccy is string
    Currency code (e.g. "USD")
anchorDate is date
    Anchor date for the zero rates
nodeDates is date[]
    Future node dates for the curve
zeroRates is number[]
    &$.size() == nodeDates.size()\must have one zero rate per node date
    Continuously compounded zero rates at each node date
&optional
dayCount is string
    Day-count basis used to map zero rates to log discount factors (default "ACT_365F")
logDfScheme is string
    Log discount-factor interpolation scheme (default "LOG_LINEAR")
base is handle StorableDiscountCurve
    Optional base discount curve
&outputs
curve is handle StorableDiscountCurve
    The zero-rate discount curve
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CurveBlock_New_Simple
    Build a CurveBlock from a single discount curve
&inputs
dc is handle StorableDiscountCurve
    The discount curve
&optional
liborBasis is string
    Libor day basis name (default "ACT_365F")
&outputs
block is handle StorableCurveBlock
    The curve block
-IF-------------------------------------------------------------------------*/

namespace Dal {
    namespace {
        void DiscountPWLF_New(const String_& name,
                              const String_& ccy,
                              const Vector_<Date_>& knotDates,
                              const Vector_<>& fwdRates,
                              const Handle_<StorableDiscountCurve_>& base,
                              Handle_<StorableDiscountCurve_>* curve) {
            Handle_<DiscountCurve_> baseCurve;
            if (!base.IsEmpty())
                baseCurve = base->val_;
            auto result = Dal::DiscountPWLFNew(name, ccy, knotDates, fwdRates, baseCurve);
            curve->reset(new StorableDiscountCurve_(result));
        }

        void DiscountZeroRate_New(const String_& name,
                                  const String_& ccy,
                                  const Date_& anchorDate,
                                  const Vector_<Date_>& nodeDates,
                                  const Vector_<>& zeroRates,
                                  const String_& dayCount,
                                  const String_& logDfScheme,
                                  const Handle_<StorableDiscountCurve_>& base,
                                  Handle_<StorableDiscountCurve_>* curve) {
            Handle_<DiscountCurve_> baseCurve;
            if (!base.IsEmpty())
                baseCurve = base->val_;
            const DayBasis_ basis(dayCount.empty() ? "ACT_365F" : dayCount);
            const LogDfScheme_ scheme(logDfScheme.empty() ? "LOG_LINEAR" : logDfScheme);
            auto result = Dal::DiscountZeroRateNew(name, ccy, anchorDate, nodeDates, zeroRates, basis, scheme, baseCurve);
            curve->reset(new StorableDiscountCurve_(result));
        }

        void CurveBlock_New_Simple(const Handle_<StorableDiscountCurve_>& dc,
                                  const String_& liborBasis,
                                  Handle_<StorableCurveBlock_>* block) {
            REQUIRE(dc, "Invalid discount curve handle");
            auto result = Dal::CurveBlockNew(dc->val_, DayBasis_(liborBasis));
            block->reset(new StorableCurveBlock_(result));
        }
    }
#ifdef _WIN32
#include <dal-excel/auto/MG_DiscountPWLF_New_public.inc>
#include <dal-excel/auto/MG_DiscountZeroRate_New_public.inc>
#include <dal-excel/auto/MG_CurveBlock_New_Simple_public.inc>
#endif
}

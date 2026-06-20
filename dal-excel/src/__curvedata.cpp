//
// Created by wegam on 2026/6/20.
//

#pragma once

#include "__platform.hpp"
#include "__curve_storable.hpp"
#include <dal-public/src/curvedata.hpp>

/*IF--------------------------------------------------------------------------
public DiscountPWLFNew
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
public CurveBlockNewSimple
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
        void DiscountPWLFNew(const String_& name,
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

        void CurveBlockNewSimple(const Handle_<StorableDiscountCurve_>& dc,
                                  const String_& liborBasis,
                                  Handle_<StorableCurveBlock_>* block) {
            REQUIRE(dc, "Invalid discount curve handle");
            auto result = Dal::CurveBlockNew(dc->val_, DayBasis_(liborBasis));
            block->reset(new StorableCurveBlock_(result));
        }
    }
#ifdef _WIN32
#include <dal-excel/auto/MG_DiscountPWLFNew_public.inc>
#include <dal-excel/auto/MG_CurveBlockNewSimple_public.inc>
#endif
}

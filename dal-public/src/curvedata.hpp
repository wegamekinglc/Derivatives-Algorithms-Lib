//
// Created by wegamekinglc on 2026/6/20.
//

#pragma once

#include <map>

#include <dal/curve/curveblock.hpp>
#include <dal/curve/discount.hpp>
#include <dal/curve/piecewiselinear.hpp>
#include <dal/curve/ycimp.hpp>
#include <dal/platform/platform.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/time/daybasis.hpp>
#include <dal/time/periodlength.hpp>

namespace Dal {

    FORCE_INLINE Handle_<DiscountCurve_> DiscountPWLFNew(const String_& name,
                                                         const String_& ccy,
                                                         const Vector_<Date_>& knotDates,
                                                         const Vector_<>& fwdRates,
                                                         const Handle_<DiscountCurve_>& base = Handle_<DiscountCurve_>()) {
        return Handle_<DiscountCurve_>(NewDiscountPWLF(name, ccy, PiecewiseLinear_(knotDates, fwdRates, fwdRates), base));
    }

    FORCE_INLINE Handle_<CurveBlock_> CurveBlockNew(const Handle_<DiscountCurve_>& dc,
                                                    const DayBasis_& liborBasis = DayBasis_("ACT_365F")) {
        return Handle_<CurveBlock_>(new CurveBlock_(dc, liborBasis));
    }

    FORCE_INLINE Handle_<CurveBlock_> CurveBlockNew(const String_& name,
                                                    const String_& ccy,
                                                    const std::map<CollateralType_, Handle_<DiscountCurve_>>& discounts,
                                                    const std::map<PeriodLength_, Handle_<DiscountCurve_>>& forwards,
                                                    const DayBasis_& liborBasis) {
        return Handle_<CurveBlock_>(new CurveBlock_(name, ccy, discounts, forwards, liborBasis));
    }

} // namespace Dal

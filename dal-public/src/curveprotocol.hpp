//
// Created by wegamekinglc on 2026/6/20.
//

#pragma once

#include <dal/platform/platform.hpp>
#include <dal/curve/xccyinstrument.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/protocol/rateconvention.hpp>
#include <dal/time/daybasis.hpp>
#include <dal/time/periodlength.hpp>

namespace Dal {

    // --- CollateralType_ ---
    FORCE_INLINE CollateralType_ CollateralType_OIS() {
        return CollateralType_(CollateralType_::Value_::OIS);
    }

    FORCE_INLINE CollateralType_ CollateralType_Libor(const PeriodLength_& tenor) {
        CollateralType_ ct;
        // Libor collateral by tenor is represented via the projection curve
        // mechanism; the collateral type itself remains GC.
        return CollateralType_(CollateralType_::Value_::GC);
    }

    // --- PeriodLength_ ---
    FORCE_INLINE PeriodLength_ PeriodLength_New(const String_& iso) {
        return PeriodLength_(iso);
    }

    // --- DayBasis_ ---
    FORCE_INLINE DayBasis_ DayBasis_New(const String_& name) {
        return DayBasis_(name);
    }

    // --- RateLegConvention_ ---
    FORCE_INLINE RateLegConvention_ RateLegConvention_New(const PeriodLength_& freq,
                                                          const DayBasis_& basis) {
        RateLegConvention_ rlc;
        rlc.paymentFrequency_ = freq;
        rlc.dayBasis_ = basis;
        rlc.paymentLag_ = 0;
        rlc.businessDayConvention_ = BizDayConvention_("Following");
        rlc.paymentConvention_ = BizDayConvention_("Following");
        rlc.accrualHolidays_ = Holidays_("");
        rlc.paymentHolidays_ = Holidays_("");
        rlc.endOfMonth_ = false;
        return rlc;
    }

    // --- RateIndexConvention_ ---
    FORCE_INLINE RateIndexConvention_ RateIndexConvention_New(const PeriodLength_& forecastTenor,
                                                               const DayBasis_& basis,
                                                               const CollateralType_& collateral,
                                                               bool useProjectionCurve = false) {
        RateIndexConvention_ ric;
        ric.forecastTenor_ = forecastTenor;
        ric.dayBasis_ = basis;
        ric.collateral_ = collateral;
        ric.useProjectionCurve_ = useProjectionCurve;
        ric.spotLag_ = 0;
        ric.fixingLag_ = 0;
        ric.businessDayConvention_ = BizDayConvention_("Following");
        ric.fixingHolidays_ = Holidays_("");
        ric.accrualHolidays_ = Holidays_("");
        ric.endOfMonth_ = false;
        return ric;
    }

    // --- CurrencyPair_ ---
    FORCE_INLINE CurrencyPair_ CurrencyPair_New(const String_& domestic, const String_& foreign) {
        return CurrencyPair_(Ccy_(domestic), Ccy_(foreign));
    }

} // namespace Dal

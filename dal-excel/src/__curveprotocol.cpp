//
// Created by wegam on 2026/6/20.
//

#pragma once

#include "__platform.hpp"
#include "__curve_storable.hpp"
#include <dal-public/src/curveprotocol.hpp>

/*IF--------------------------------------------------------------------------
public CollateralTypeOIS
    Create an OIS collateral type handle
&outputs
collateral is handle StorableCollateralType
    The OIS collateral type
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CollateralTypeLibor
    Create a Libor (GC) collateral type handle
&inputs
tenor is string
    The Libor tenor (e.g. "3M", "6M", "12M")
&outputs
collateral is handle StorableCollateralType
    The Libor (GC) collateral type
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public PeriodLengthNew
    Create a period length handle from an ISO string
&inputs
iso is string
    The ISO period string (e.g. "3M", "6M", "12M")
&outputs
period is handle StorablePeriodLength
    The period length
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DayBasisNew
    Create a day basis handle from a name string
&inputs
name is string
    The day basis name (e.g. "ACT_365F", "ACT_360", "30_360")
&outputs
basis is handle StorableDayBasis
    The day basis
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public RateLegConventionNew
    Create a rate leg convention handle
&inputs
freq is string
    Payment frequency as an ISO period (e.g. "3M", "6M")
basis is string
    Day basis name (e.g. "ACT_365F", "ACT_360")
&outputs
convention is handle StorableRateLegConvention
    The rate leg convention
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public RateIndexConventionNew
    Create a rate index convention handle
&inputs
forecastTenor is string
    Forecast tenor as an ISO period (e.g. "3M", "6M")
basis is string
    Day basis name (e.g. "ACT_365F", "ACT_360")
collateral is string
    Collateral type string (e.g. "OIS", "GC")
&optional
useProjectionCurve is boolean (false)
    Whether to use a projection curve
&outputs
convention is handle StorableRateIndexConvention
    The rate index convention
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CurrencyPairNew
    Create a currency pair handle
&inputs
domestic is string
    Domestic currency code (e.g. "USD")
foreign is string
    Foreign currency code (e.g. "EUR")
&outputs
pair is handle StorableCurrencyPair
    The currency pair
-IF-------------------------------------------------------------------------*/

namespace Dal {
    namespace {
        void CollateralTypeOIS(Handle_<StorableCollateralType_>* collateral) {
            collateral->reset(new StorableCollateralType_(CollateralType_OIS()));
        }

        void CollateralTypeLibor(const String_& tenor, Handle_<StorableCollateralType_>* collateral) {
            collateral->reset(new StorableCollateralType_(CollateralType_Libor(PeriodLength_New(tenor))));
        }

        void PeriodLengthNew(const String_& iso, Handle_<StorablePeriodLength_>* period) {
            period->reset(new StorablePeriodLength_(PeriodLength_New(iso)));
        }

        void DayBasisNew(const String_& name, Handle_<StorableDayBasis_>* basis) {
            basis->reset(new StorableDayBasis_(DayBasis_New(name)));
        }

        void RateLegConventionNew(const String_& freq, const String_& basis, Handle_<StorableRateLegConvention_>* convention) {
            convention->reset(new StorableRateLegConvention_(
                RateLegConvention_New(PeriodLength_New(freq), DayBasis_New(basis))));
        }

        void RateIndexConventionNew(const String_& forecastTenor,
                                     const String_& basis,
                                     const String_& collateral,
                                     bool useProjectionCurve,
                                     Handle_<StorableRateIndexConvention_>* convention) {
            convention->reset(new StorableRateIndexConvention_(
                RateIndexConvention_New(PeriodLength_New(forecastTenor),
                                        DayBasis_New(basis),
                                        CollateralType_(collateral),
                                        useProjectionCurve)));
        }

        void CurrencyPairNew(const String_& domestic, const String_& foreign, Handle_<StorableCurrencyPair_>* pair) {
            pair->reset(new StorableCurrencyPair_(CurrencyPair_New(domestic, foreign)));
        }
    }
#ifdef _WIN32
#include <dal-excel/auto/MG_CollateralTypeOIS_public.inc>
#include <dal-excel/auto/MG_CollateralTypeLibor_public.inc>
#include <dal-excel/auto/MG_PeriodLengthNew_public.inc>
#include <dal-excel/auto/MG_DayBasisNew_public.inc>
#include <dal-excel/auto/MG_RateLegConventionNew_public.inc>
#include <dal-excel/auto/MG_RateIndexConventionNew_public.inc>
#include <dal-excel/auto/MG_CurrencyPairNew_public.inc>
#endif
}

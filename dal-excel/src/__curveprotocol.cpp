//
// Created by wegam on 2026/6/20.
//

#pragma once

#include "__platform.hpp"
#include "__curve_storable.hpp"
#include "__xccy_test_api.hpp"
#include <cmath>
#include <dal-public/src/curveprotocol.hpp>

// clang-format off
/*IF--------------------------------------------------------------------------
public CollateralType_OIS
    Create an OIS collateral type handle
&outputs
collateral is handle StorableCollateralType
    The OIS collateral type
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CollateralType_Libor
    Create a Libor (GC) collateral type handle
&inputs
tenor is string
    The Libor tenor (e.g. "3M", "6M", "12M")
&outputs
collateral is handle StorableCollateralType
    The Libor (GC) collateral type
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public PeriodLength_New
    Create a period length handle from an ISO string
&inputs
iso is string
    The ISO period string (e.g. "3M", "6M", "12M")
&outputs
period is handle StorablePeriodLength
    The period length
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DayBasis_New
    Create a day basis handle from a name string
&inputs
name is string
    The day basis name (e.g. "ACT_365F", "ACT_360", "30_360")
&outputs
basis is handle StorableDayBasis
    The day basis
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public RateLegConvention_New
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
public RateIndexConvention_New
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
public CurrencyPair_New
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

/*IF--------------------------------------------------------------------------
public XccyResetConvention_New
    Create an FX reset convention handle for resettable cross-currency swaps
&inputs
fixingLag is integer
    Number of business days before the reset date used for the FX fixing
fixingHolidays is string
    Holiday center used to adjust FX fixing dates (empty string means no holidays)
fixingConvention is string
    Business-day convention used to adjust FX fixing dates
fixingHour is integer
    FX fixing hour in the range 0 to 23
fixingMinute is integer
    FX fixing minute in the range 0 to 59
&outputs
resetConvention is handle StorableFxResetConvention
    The FX reset convention
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MarketFixingSnapshot_New
    Create an immutable market fixing snapshot from parallel input arrays
&inputs
indexNames is string[]
    Index names for the fixing observations
fixingTimes is cell[]
    Fixing timestamps as Excel date-time cells
values is number[]
    Positive finite fixing values
&outputs
snapshot is handle StorableMarketFixingSnapshot
    The immutable market fixing snapshot
-IF-------------------------------------------------------------------------*/

// clang-format on
namespace Dal {
    namespace {
        void CollateralType_OIS(Handle_<StorableCollateralType_>* collateral) {
            collateral->reset(new StorableCollateralType_(Dal::CollateralType_OIS()));
        }

        void CollateralType_Libor(const String_& tenor, Handle_<StorableCollateralType_>* collateral) {
            collateral->reset(new StorableCollateralType_(Dal::CollateralType_Libor(Dal::PeriodLength_New(tenor))));
        }

        void PeriodLength_New(const String_& iso, Handle_<StorablePeriodLength_>* period) {
            period->reset(new StorablePeriodLength_(Dal::PeriodLength_New(iso)));
        }

        void DayBasis_New(const String_& name, Handle_<StorableDayBasis_>* basis) { basis->reset(new StorableDayBasis_(Dal::DayBasis_New(name))); }

        void RateLegConvention_New(const String_& freq, const String_& basis, Handle_<StorableRateLegConvention_>* convention) {
            convention->reset(new StorableRateLegConvention_(Dal::RateLegConvention_New(Dal::PeriodLength_New(freq), Dal::DayBasis_New(basis))));
        }

        void RateIndexConvention_New(const String_& forecastTenor,
                                     const String_& basis,
                                     const String_& collateral,
                                     bool useProjectionCurve,
                                     Handle_<StorableRateIndexConvention_>* convention) {
            convention->reset(new StorableRateIndexConvention_(Dal::RateIndexConvention_New(
                Dal::PeriodLength_New(forecastTenor), Dal::DayBasis_New(basis), CollateralType_(collateral), useProjectionCurve)));
        }

        void CurrencyPair_New(const String_& domestic, const String_& foreign, Handle_<StorableCurrencyPair_>* pair) {
            pair->reset(new StorableCurrencyPair_(Dal::CurrencyPair_New(domestic, foreign)));
        }
    } // namespace

    void XccyResetConvention_New(int fixingLag,
                                 const String_& fixingHolidays,
                                 const String_& fixingConvention,
                                 int fixingHour,
                                 int fixingMinute,
                                 Handle_<StorableFxResetConvention_>* resetConvention) {
        REQUIRE(fixingLag >= 0, "FX reset fixing lag must be non-negative");
        REQUIRE(fixingHour >= 0 && fixingHour < 24, "FX reset fixing hour must be between 0 and 23");
        REQUIRE(fixingMinute >= 0 && fixingMinute < 60, "FX reset fixing minute must be between 0 and 59");
        resetConvention->reset(new StorableFxResetConvention_(
            Dal::FxResetConventionNew(fixingLag, Holidays_(fixingHolidays), BizDayConvention_(fixingConvention), fixingHour, fixingMinute)));
    }

    void MarketFixingSnapshot_New(const Vector_<String_>& indexNames,
                                  const Vector_<Cell_>& fixingTimes,
                                  const Vector_<>& values,
                                  Handle_<StorableMarketFixingSnapshot_>* snapshot) {
        REQUIRE(indexNames.size() == fixingTimes.size() && indexNames.size() == values.size(),
                "Market fixing snapshot requires indexNames, fixingTimes, and values to have equal length");
        MarketFixingSnapshot_::values_t normalized;
        for (int i = 0; i < indexNames.size(); ++i) {
            DateTime_ fixingTime;
            if (Cell::IsDouble(fixingTimes[i])) {
                const double serial = Cell::ToDouble(fixingTimes[i]);
                REQUIRE(std::isfinite(serial), "Market fixing time must be a finite Excel serial date");
                const int dateSerial = static_cast<int>(std::floor(serial));
                fixingTime = DateTime_(Date::FromExcel(dateSerial), serial - dateSerial);
            } else if (Cell::IsDate(fixingTimes[i])) {
                fixingTime = DateTime_(Cell::ToDate(fixingTimes[i]));
            } else {
                fixingTime = Cell::ToDateTime(fixingTimes[i]);
            }
            const auto inserted = normalized[indexNames[i]].emplace(fixingTime, values[i]);
            REQUIRE(inserted.second,
                    "Market fixing snapshot contains a duplicate observation for " + indexNames[i] + " at " + DateTime::ToString(fixingTime));
        }
        snapshot->reset(new StorableMarketFixingSnapshot_(Dal::MarketFixingSnapshotNew(normalized)));
    }
    // clang-format off
#ifdef _WIN32
#include <dal-excel/auto/MG_CollateralType_OIS_public.inc>
#include <dal-excel/auto/MG_CollateralType_Libor_public.inc>
#include <dal-excel/auto/MG_PeriodLength_New_public.inc>
#include <dal-excel/auto/MG_DayBasis_New_public.inc>
#include <dal-excel/auto/MG_RateLegConvention_New_public.inc>
#include <dal-excel/auto/MG_RateIndexConvention_New_public.inc>
#include <dal-excel/auto/MG_CurrencyPair_New_public.inc>
#include <dal-excel/auto/MG_XccyResetConvention_New_public.inc>
#include <dal-excel/auto/MG_MarketFixingSnapshot_New_public.inc>
#endif
    // clang-format on
} // namespace Dal

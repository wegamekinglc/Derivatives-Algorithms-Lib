//
// Created by wegam on 2026/6/20.
//

#pragma once

#include "__platform.hpp"
#include "__curve_storable.hpp"
#include <dal-public/src/curveinstrument.hpp>

// clang-format off
/*IF--------------------------------------------------------------------------
public Deposit_New
    Create a deposit instrument for curve calibration
&inputs
tradeDate is date
    The trade/settlement date
start is date
    The start date of the deposit
maturity is date
    The maturity date of the deposit
marketRate is number
    The market quoted rate
convention is handle StorableRateIndexConvention
    The rate index convention
&outputs
instrument is handle StorableYCInstrument
    The deposit instrument
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public FRA_New
    Create a FRA instrument for curve calibration
&inputs
tradeDate is date
    The trade/settlement date
start is date
    The start date of the FRA
maturity is date
    The maturity date of the FRA
marketRate is number
    The market quoted rate
convention is handle StorableRateIndexConvention
    The rate index convention
&outputs
instrument is handle StorableYCInstrument
    The FRA instrument
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public Future_New
    Create a future instrument for curve calibration
&inputs
tradeDate is date
    The trade/settlement date
start is date
    The start date of the future
maturity is date
    The maturity date of the future
marketRate is number
    The market quoted rate
convention is handle StorableRateIndexConvention
    The rate index convention
&optional
convexityAdjustment is number (0.0)
    Convexity adjustment
&outputs
instrument is handle StorableYCInstrument
    The future instrument
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public Swap_New
    Create a swap instrument for curve calibration
&inputs
tradeDate is date
    The trade/settlement date
start is date
    The start date of the swap
maturity is date
    The maturity date of the swap
marketRate is number
    The market quoted rate
fixedLeg is handle StorableRateLegConvention
    The fixed leg convention
floatIndex is handle StorableRateIndexConvention
    The floating index convention
floatLeg is handle StorableRateLegConvention
    The floating leg convention
&outputs
instrument is handle StorableYCInstrument
    The swap instrument
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public OISSwap_New
    Create an OIS swap instrument for curve calibration
&inputs
tradeDate is date
    The trade/settlement date
start is date
    The start date of the swap
maturity is date
    The maturity date of the swap
marketRate is number
    The market quoted rate
fixedLeg is handle StorableRateLegConvention
    The fixed leg convention
overnightIndex is handle StorableRateIndexConvention
    The overnight index convention
floatLeg is handle StorableRateLegConvention
    The floating leg convention
&outputs
instrument is handle StorableYCInstrument
    The OIS swap instrument
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public BasisSwap_New
    Create a basis swap instrument for curve calibration
&inputs
tradeDate is date
    The trade/settlement date
start is date
    The start date of the swap
maturity is date
    The maturity date of the swap
marketRate is number
    The market quoted spread
spreadIndex is handle StorableRateIndexConvention
    The spread index convention
spreadLeg is handle StorableRateLegConvention
    The spread leg convention
refIndex is handle StorableRateIndexConvention
    The reference index convention
refLeg is handle StorableRateLegConvention
    The reference leg convention
&outputs
instrument is handle StorableYCInstrument
    The basis swap instrument
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CrossCurrencySwap_New
    Create a cross-currency swap instrument for xccy calibration
&inputs
tradeDate is date
    The trade/settlement date
start is date
    The start date of the swap
maturity is date
    The maturity date of the swap
marketRate is number
    The market quoted spread
currencies is handle StorableCurrencyPair
    The currency pair (domestic, foreign)
domesticLeg is handle StorableRateLegConvention
    The domestic leg convention
domesticIndex is handle StorableRateIndexConvention
    The domestic index convention
foreignLeg is handle StorableRateLegConvention
    The foreign leg convention
foreignIndex is handle StorableRateIndexConvention
    The foreign index convention
&optional
domesticNotional is number (100.0)
    Domestic notional
foreignNotional is number (100.0)
    Foreign notional
&outputs
instrument is handle StorableCrossCurrencySwap
    The cross-currency swap instrument
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CrossCurrencySwapConfig_New
    Create a configured cross-currency swap input handle
&inputs
currencies is handle StorableCurrencyPair
    The currency pair (domestic, foreign)
domesticLeg is handle StorableRateLegConvention
    The domestic leg convention
domesticIndex is handle StorableRateIndexConvention
    The domestic rate index convention
foreignLeg is handle StorableRateLegConvention
    The foreign leg convention
foreignIndex is handle StorableRateIndexConvention
    The foreign rate index convention
resetConvention is handle StorableFxResetConvention
    The FX reset convention (created with XCCYRESETCONVENTION.NEW)
notionalMode is string
    Notional behavior: FIXED, RESETTABLE, or MARK_TO_MARKET
domesticRateIndex is string
    Domestic rate fixing index name
domesticRateFixingHour is integer
    Domestic rate fixing hour in the range 0 to 23
domesticRateFixingMinute is integer
    Domestic rate fixing minute in the range 0 to 59
foreignRateIndex is string
    Foreign rate fixing index name
foreignRateFixingHour is integer
    Foreign rate fixing hour in the range 0 to 23
foreignRateFixingMinute is integer
    Foreign rate fixing minute in the range 0 to 59
&optional
domesticNotional is number (100.0)
    Domestic notional
foreignNotional is number (100.0)
    Foreign notional
&outputs
config is handle StorableCrossCurrencySwapConfig
    The configured cross-currency swap input
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CrossCurrencySwap_Config_New
    Create a cross-currency swap instrument from a configuration handle
&inputs
tradeDate is date
    The trade or settlement date
start is date
    The swap start date
maturity is date
    The swap maturity date
marketRate is number
    The market quoted spread
config is handle StorableCrossCurrencySwapConfig
    The cross-currency swap configuration
&outputs
instrument is handle StorableCrossCurrencySwap
    The configured cross-currency swap instrument
-IF-------------------------------------------------------------------------*/

// clang-format on
namespace Dal {
    namespace {
        void Deposit_New(const Date_& tradeDate,
                         const Date_& start,
                         const Date_& maturity,
                         double marketRate,
                         const Handle_<StorableRateIndexConvention_>& convention,
                         Handle_<StorableYCInstrument_>* instrument) {
            REQUIRE(convention, "Invalid rate index convention handle");
            auto result = Dal::DepositNew(tradeDate, start, maturity, marketRate, convention->val_);
            instrument->reset(new StorableYCInstrument_(result));
        }

        void FRA_New(const Date_& tradeDate,
                     const Date_& start,
                     const Date_& maturity,
                     double marketRate,
                     const Handle_<StorableRateIndexConvention_>& convention,
                     Handle_<StorableYCInstrument_>* instrument) {
            REQUIRE(convention, "Invalid rate index convention handle");
            auto result = Dal::FRANew(tradeDate, start, maturity, marketRate, convention->val_);
            instrument->reset(new StorableYCInstrument_(result));
        }

        void Future_New(const Date_& tradeDate,
                        const Date_& start,
                        const Date_& maturity,
                        double marketRate,
                        const Handle_<StorableRateIndexConvention_>& convention,
                        double convexityAdjustment,
                        Handle_<StorableYCInstrument_>* instrument) {
            REQUIRE(convention, "Invalid rate index convention handle");
            auto result = Dal::FutureNew(tradeDate, start, maturity, marketRate, convention->val_, convexityAdjustment);
            instrument->reset(new StorableYCInstrument_(result));
        }

        void Swap_New(const Date_& tradeDate,
                      const Date_& start,
                      const Date_& maturity,
                      double marketRate,
                      const Handle_<StorableRateLegConvention_>& fixedLeg,
                      const Handle_<StorableRateIndexConvention_>& floatIndex,
                      const Handle_<StorableRateLegConvention_>& floatLeg,
                      Handle_<StorableYCInstrument_>* instrument) {
            REQUIRE(fixedLeg, "Invalid fixed leg convention handle");
            REQUIRE(floatIndex, "Invalid float index convention handle");
            REQUIRE(floatLeg, "Invalid float leg convention handle");
            auto result = Dal::SwapNew(tradeDate, start, maturity, marketRate, fixedLeg->val_, floatIndex->val_, floatLeg->val_);
            instrument->reset(new StorableYCInstrument_(result));
        }

        void OISSwap_New(const Date_& tradeDate,
                         const Date_& start,
                         const Date_& maturity,
                         double marketRate,
                         const Handle_<StorableRateLegConvention_>& fixedLeg,
                         const Handle_<StorableRateIndexConvention_>& overnightIndex,
                         const Handle_<StorableRateLegConvention_>& floatLeg,
                         Handle_<StorableYCInstrument_>* instrument) {
            REQUIRE(fixedLeg, "Invalid fixed leg convention handle");
            REQUIRE(overnightIndex, "Invalid overnight index convention handle");
            REQUIRE(floatLeg, "Invalid float leg convention handle");
            auto result = Dal::OISSwapNew(tradeDate, start, maturity, marketRate, fixedLeg->val_, overnightIndex->val_, floatLeg->val_);
            instrument->reset(new StorableYCInstrument_(result));
        }

        void BasisSwap_New(const Date_& tradeDate,
                           const Date_& start,
                           const Date_& maturity,
                           double marketRate,
                           const Handle_<StorableRateIndexConvention_>& spreadIndex,
                           const Handle_<StorableRateLegConvention_>& spreadLeg,
                           const Handle_<StorableRateIndexConvention_>& refIndex,
                           const Handle_<StorableRateLegConvention_>& refLeg,
                           Handle_<StorableYCInstrument_>* instrument) {
            REQUIRE(spreadIndex, "Invalid spread index convention handle");
            REQUIRE(spreadLeg, "Invalid spread leg convention handle");
            REQUIRE(refIndex, "Invalid ref index convention handle");
            REQUIRE(refLeg, "Invalid ref leg convention handle");
            auto result = Dal::BasisSwapNew(tradeDate, start, maturity, marketRate, spreadIndex->val_, spreadLeg->val_, refIndex->val_, refLeg->val_);
            instrument->reset(new StorableYCInstrument_(result));
        }

        void CrossCurrencySwap_New(const Date_& tradeDate,
                                   const Date_& start,
                                   const Date_& maturity,
                                   double marketRate,
                                   const Handle_<StorableCurrencyPair_>& currencies,
                                   const Handle_<StorableRateLegConvention_>& domesticLeg,
                                   const Handle_<StorableRateIndexConvention_>& domesticIndex,
                                   const Handle_<StorableRateLegConvention_>& foreignLeg,
                                   const Handle_<StorableRateIndexConvention_>& foreignIndex,
                                   double domesticNotional,
                                   double foreignNotional,
                                   Handle_<StorableCrossCurrencySwap_>* instrument) {
            REQUIRE(currencies, "Invalid currency pair handle");
            REQUIRE(domesticLeg, "Invalid domestic leg convention handle");
            REQUIRE(domesticIndex, "Invalid domestic index convention handle");
            REQUIRE(foreignLeg, "Invalid foreign leg convention handle");
            REQUIRE(foreignIndex, "Invalid foreign index convention handle");
            auto result = Dal::CrossCurrencySwapNew(tradeDate, start, maturity, marketRate, currencies->val_, domesticNotional, foreignNotional,
                                                    domesticLeg->val_, domesticIndex->val_, foreignLeg->val_, foreignIndex->val_);
            instrument->reset(new StorableCrossCurrencySwap_(result));
        }
    } // namespace

    void CrossCurrencySwapConfig_New(const Handle_<StorableCurrencyPair_>& currencies,
                                     const Handle_<StorableRateLegConvention_>& domesticLeg,
                                     const Handle_<StorableRateIndexConvention_>& domesticIndex,
                                     const Handle_<StorableRateLegConvention_>& foreignLeg,
                                     const Handle_<StorableRateIndexConvention_>& foreignIndex,
                                     const Handle_<StorableFxResetConvention_>& resetConvention,
                                     const String_& notionalMode,
                                     const String_& domesticRateIndex,
                                     int domesticRateFixingHour,
                                     int domesticRateFixingMinute,
                                     const String_& foreignRateIndex,
                                     int foreignRateFixingHour,
                                     int foreignRateFixingMinute,
                                     double domesticNotional,
                                     double foreignNotional,
                                     Handle_<StorableCrossCurrencySwapConfig_>* config) {
        REQUIRE(currencies, "Invalid currency pair handle");
        REQUIRE(domesticLeg, "Invalid domestic leg convention handle");
        REQUIRE(domesticIndex, "Invalid domestic index convention handle");
        REQUIRE(foreignLeg, "Invalid foreign leg convention handle");
        REQUIRE(foreignIndex, "Invalid foreign index convention handle");
        REQUIRE(resetConvention, "Invalid FX reset convention handle");

        CrossCurrencySwapConfigBuilder_ builder;
        builder.pair_ = currencies->val_;
        builder.domesticNotional_ = domesticNotional;
        builder.foreignNotional_ = foreignNotional;
        builder.convention_.domesticLeg_ = domesticLeg->val_;
        builder.convention_.domesticIndex_ = domesticIndex->val_;
        builder.convention_.foreignLeg_ = foreignLeg->val_;
        builder.convention_.foreignIndex_ = foreignIndex->val_;
        builder.convention_.initialNotionalExchange_ = true;
        builder.convention_.finalNotionalExchange_ = true;
        builder.convention_.spreadOnForeignLeg_ = true;
        builder.notionalMode_ = XccyNotionalMode_(notionalMode);
        builder.fxReset_ = resetConvention->val_;
        builder.domesticRateFixing_ = {domesticRateIndex, domesticRateFixingHour, domesticRateFixingMinute};
        builder.foreignRateFixing_ = {foreignRateIndex, foreignRateFixingHour, foreignRateFixingMinute};
        config->reset(new StorableCrossCurrencySwapConfig_(builder.Build()));
    }

    void CrossCurrencySwap_Config_New(const Date_& tradeDate,
                                      const Date_& start,
                                      const Date_& maturity,
                                      double marketRate,
                                      const Handle_<StorableCrossCurrencySwapConfig_>& config,
                                      Handle_<StorableCrossCurrencySwap_>* instrument) {
        REQUIRE(config, "Invalid cross-currency swap configuration handle");
        instrument->reset(new StorableCrossCurrencySwap_(Dal::CrossCurrencySwapNew(tradeDate, start, maturity, marketRate, config->val_)));
    }
    // clang-format off
#ifdef _WIN32
#include <dal-excel/auto/MG_Deposit_New_public.inc>
#include <dal-excel/auto/MG_FRA_New_public.inc>
#include <dal-excel/auto/MG_Future_New_public.inc>
#include <dal-excel/auto/MG_Swap_New_public.inc>
#include <dal-excel/auto/MG_OISSwap_New_public.inc>
#include <dal-excel/auto/MG_BasisSwap_New_public.inc>
#include <dal-excel/auto/MG_CrossCurrencySwap_New_public.inc>
#include <dal-excel/auto/MG_CrossCurrencySwapConfig_New_public.inc>
#include <dal-excel/auto/MG_CrossCurrencySwap_Config_New_public.inc>
#endif
    // clang-format on
} // namespace Dal

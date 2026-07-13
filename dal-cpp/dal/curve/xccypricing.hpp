//
// Created by Codex on 2026/7/13.
//

#pragma once

#include <dal/platform/platform.hpp>

#include <dal/curve/jointycctx.hpp>
#include <dal/curve/xccyinstrument.hpp>
#include <dal/indice/fixingsnapshot.hpp>
#include <dal/protocol/accrualperiod.hpp>
#include <dal/time/datetime.hpp>
#include <dal/time/schedules.hpp>

namespace Dal {
    struct XccyCouponPeriod_ {
        SchedulePeriod_ schedule_;
        AccrualPeriod_ accrual_;
        String_ rateIndexName_;
        DateTime_ rateFixingTime_;
    };

    struct XccyResetEvent_ {
        Date_ effectiveDate_;
        DateTime_ fxFixingTime_;
        int domesticPeriodIndex_ = 0;
    };

    struct XccyCashflowPlan_ {
        Vector_<XccyCouponPeriod_> domesticPeriods_;
        Vector_<XccyCouponPeriod_> foreignPeriods_;
        Vector_<XccyResetEvent_> resets_;
        CrossCurrencySwapConfig_ config_;
        Date_ start_;
        Date_ maturity_;
    };

    XccyCashflowPlan_ BuildXccyCashflowPlan(const Date_& start, const Date_& maturity, const CrossCurrencySwapConfig_& config);
    Vector_<FixingRequest_> RequiredHistoricalFixings(const XccyCashflowPlan_& plan, const DateTime_& valuationTime);
    template <class T_> struct XccyMarketView_ {
        DateTime_ valuationTime_;
        CurrencyPair_ pair_;
        Ccy_ collateralCurrency_;
        T_ fxSpot_;
        const Tape::JointCurveBlock_<T_>* domestic_ = nullptr;
        const Tape::JointCurveBlock_<T_>* foreign_ = nullptr;
        const Tape::DiscountCurve_<T_>* basis_ = nullptr;
    };

    template <class T_> struct XccyResolvedNotionals_ {
        Vector_<T_> domesticNotionals_;
        Vector_<T_> mtmDeltas_;
    };

    template <class T_>
    XccyResolvedNotionals_<T_>
    ResolveXccyNotionals(const XccyCashflowPlan_& plan, const XccyMarketView_<T_>& market, const MarketFixingSnapshot_& fixings);

    template <class T_> T_ PriceXccyParSpread(const XccyCashflowPlan_& plan, const XccyMarketView_<T_>& market, const MarketFixingSnapshot_& fixings);

} // namespace Dal

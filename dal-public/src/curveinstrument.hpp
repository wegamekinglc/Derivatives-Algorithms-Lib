//
// Created by wegamekinglc on 2026/6/20.
//

#pragma once

#include <dal/curve/xccyinstrument.hpp>
#include <dal/curve/ycinstrument.hpp>
#include <dal/platform/platform.hpp>
#include <dal/protocol/rateconvention.hpp>
#include <dal/time/date.hpp>

namespace Dal {

    struct CrossCurrencySwapConfigBuilder_ {
        CurrencyPair_ pair_;
        double domesticNotional_ = 100.0;
        double foreignNotional_ = 100.0;
        CrossCurrencyConvention_ convention_;
        XccyNotionalMode_ notionalMode_ = XccyNotionalMode_::Value_::FIXED;
        FxResetConvention_ fxReset_;
        FixingIdentity_ domesticRateFixing_;
        FixingIdentity_ foreignRateFixing_;

        [[nodiscard]] CrossCurrencySwapConfig_ Build() const {
            CrossCurrencySwapConfig_ result;
            result.pair_ = pair_;
            result.domesticNotional_ = domesticNotional_;
            result.foreignNotional_ = foreignNotional_;
            result.convention_ = convention_;
            result.notionalMode_ = notionalMode_;
            result.fxReset_ = fxReset_;
            result.domesticRateFixing_ = domesticRateFixing_;
            result.foreignRateFixing_ = foreignRateFixing_;
            return result;
        }
    };

    FORCE_INLINE Handle_<YCInstrument_> DepositNew(const Date_& tradeDate,
                                                    const Date_& start,
                                                    const Date_& maturity,
                                                    double marketRate,
                                                    const RateIndexConvention_& convention) {
        return Handle_<YCInstrument_>(new Deposit_(tradeDate, start, maturity, marketRate, convention));
    }

    FORCE_INLINE Handle_<YCInstrument_> FRANew(const Date_& tradeDate,
                                                const Date_& start,
                                                const Date_& maturity,
                                                double marketRate,
                                                const RateIndexConvention_& convention) {
        return Handle_<YCInstrument_>(new FRA_(tradeDate, start, maturity, marketRate, convention));
    }

    FORCE_INLINE Handle_<YCInstrument_> FutureNew(const Date_& tradeDate,
                                                   const Date_& start,
                                                   const Date_& maturity,
                                                   double marketRate,
                                                   const RateIndexConvention_& convention,
                                                   double convexityAdjustment = 0.0) {
        return Handle_<YCInstrument_>(new Future_(tradeDate, start, maturity, marketRate, convention, convexityAdjustment));
    }

    FORCE_INLINE Handle_<YCInstrument_> SwapNew(const Date_& tradeDate,
                                                 const Date_& start,
                                                 const Date_& maturity,
                                                 double marketRate,
                                                 const RateLegConvention_& fixedLeg,
                                                 const RateIndexConvention_& floatIndex,
                                                 const RateLegConvention_& floatLeg) {
        return Handle_<YCInstrument_>(new Swap_(tradeDate, start, maturity, marketRate, fixedLeg, floatIndex, floatLeg));
    }

    FORCE_INLINE Handle_<YCInstrument_> OISSwapNew(const Date_& tradeDate,
                                                    const Date_& start,
                                                    const Date_& maturity,
                                                    double marketRate,
                                                    const RateLegConvention_& fixedLeg,
                                                    const RateIndexConvention_& overnightIndex,
                                                    const RateLegConvention_& floatLeg) {
        return Handle_<YCInstrument_>(new OISSwap_(tradeDate, start, maturity, marketRate, fixedLeg, overnightIndex, floatLeg));
    }

    FORCE_INLINE Handle_<YCInstrument_> BasisSwapNew(const Date_& tradeDate,
                                                      const Date_& start,
                                                      const Date_& maturity,
                                                      double marketRate,
                                                      const RateIndexConvention_& spreadIndex,
                                                      const RateLegConvention_& spreadLeg,
                                                      const RateIndexConvention_& refIndex,
                                                      const RateLegConvention_& refLeg) {
        return Handle_<YCInstrument_>(
            new BasisSwap_(tradeDate, start, maturity, marketRate, spreadIndex, spreadLeg, refIndex, refLeg));
    }

    FORCE_INLINE Handle_<CrossCurrencySwap_> CrossCurrencySwapNew(
        const Date_& tradeDate, const Date_& start, const Date_& maturity, double marketRate, const CrossCurrencySwapConfig_& config) {
        return Handle_<CrossCurrencySwap_>(new CrossCurrencySwap_(tradeDate, start, maturity, marketRate, config));
    }

    FORCE_INLINE Handle_<CrossCurrencySwap_> CrossCurrencySwapNew(const Date_& tradeDate,
                                                                   const Date_& start,
                                                                   const Date_& maturity,
                                                                   double marketRate,
                                                                   const CurrencyPair_& currencies,
                                                                   double domesticNotional = 100.0,
                                                                   double foreignNotional = 100.0,
                                                                   const RateLegConvention_& domesticLeg = RateLegConvention_(),
                                                                   const RateIndexConvention_& domesticIndex = RateIndexConvention_(),
                                                                   const RateLegConvention_& foreignLeg = RateLegConvention_(),
                                                                   const RateIndexConvention_& foreignIndex = RateIndexConvention_()) {
        CrossCurrencyConvention_ convention;
        convention.domesticLeg_ = domesticLeg;
        convention.domesticIndex_ = domesticIndex;
        convention.foreignLeg_ = foreignLeg;
        convention.foreignIndex_ = foreignIndex;
        convention.initialNotionalExchange_ = true;
        convention.finalNotionalExchange_ = true;
        convention.spreadOnForeignLeg_ = true;
        return Handle_<CrossCurrencySwap_>(
            new CrossCurrencySwap_(tradeDate, start, maturity, marketRate, currencies, domesticNotional, foreignNotional, convention));
    }

} // namespace Dal

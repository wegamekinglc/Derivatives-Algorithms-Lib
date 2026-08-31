//
// Created by GitHub Copilot on 2026/6/6.
//

#pragma once

#include <dal/platform/platform.hpp>

#include <dal/currency/currency.hpp>
#include <dal/curve/xccynotionalmode.hpp>
#include <dal/protocol/rateconvention.hpp>
#include <dal/time/date.hpp>
#include <dal/time/datetime.hpp>

namespace Dal {
    class CrossCurrencyMarket_;
    namespace Tape {
        template <class T_> class DiscountCurve_;
    }
    using DiscountCurve_ = Tape::DiscountCurve_<double>;

    struct CurrencyPair_ {
        Ccy_ domestic_;
        Ccy_ foreign_;

        CurrencyPair_();
        CurrencyPair_(const Ccy_& domestic, const Ccy_& foreign);
        [[nodiscard]] CurrencyPair_ Reversed() const;
        [[nodiscard]] bool operator<(const CurrencyPair_& rhs) const;
        [[nodiscard]] bool operator==(const CurrencyPair_& rhs) const;
    };

    struct FixingIdentity_ {
        String_ indexName_;
        int fixingHour_ = -1;
        int fixingMinute_ = -1;
    };

    inline bool ValidFixingIdentity(const FixingIdentity_& identity) {
        return !identity.indexName_.empty() && identity.fixingHour_ >= 0 && identity.fixingHour_ < 24 && identity.fixingMinute_ >= 0 &&
               identity.fixingMinute_ < 60;
    }

    inline DateTime_ FixingDateTime(const Date_& date, const FixingIdentity_& identity) {
        if (identity.fixingHour_ < 0 || identity.fixingMinute_ < 0)
            return DateTime_(date);
        return DateTime_(date, identity.fixingHour_, identity.fixingMinute_);
    }

    struct FxResetConvention_ {
        int fixingLag_ = -1;
        Holidays_ fixingHolidays_ = Holidays_("");
        BizDayConvention_ fixingConvention_ = BizDayConvention_("Preceding");
        int fixingHour_ = -1;
        int fixingMinute_ = -1;
    };

    struct CrossCurrencySwapConfig_ {
        CurrencyPair_ pair_;
        double domesticNotional_ = 100.0;
        double foreignNotional_ = 100.0;
        CrossCurrencyConvention_ convention_;
        XccyNotionalMode_ notionalMode_ = XccyNotionalMode_::Value_::FIXED;
        FxResetConvention_ fxReset_;
        FixingIdentity_ domesticRateFixing_;
        FixingIdentity_ foreignRateFixing_;
    };

    class CrossCurrencySwap_ {
    public:
        struct Rate_ : noncopyable {
            virtual ~Rate_() = default;
            virtual double operator()(const CrossCurrencyMarket_& market) const = 0;
        };

    private:
        Date_ tradeDate_;
        Date_ start_;
        Date_ maturity_;
        double marketRate_;
        CrossCurrencySwapConfig_ config_;

    public:
        CrossCurrencySwap_(const Date_& tradeDate,
                           const Date_& start,
                           const Date_& maturity,
                           double marketRate,
                           const CurrencyPair_& pair,
                           double domesticNotional,
                           double foreignNotional,
                           const CrossCurrencyConvention_& convention = CrossCurrencyConvention_());
        CrossCurrencySwap_(
            const Date_& tradeDate, const Date_& start, const Date_& maturity, double marketRate, const CrossCurrencySwapConfig_& config);
        [[nodiscard]] String_ Name() const;
        [[nodiscard]] pair<Date_, Date_> TimeSpan() const;
        [[nodiscard]] Date_ TradeDate() const { return tradeDate_; }
        [[nodiscard]] double MarketRate() const { return marketRate_; }
        [[nodiscard]] const CrossCurrencySwapConfig_& Config() const { return config_; }
        [[nodiscard]] Handle_<Rate_> Precompute() const;
    };
} // namespace Dal

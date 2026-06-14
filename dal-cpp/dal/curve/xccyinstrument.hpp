//
// Created by GitHub Copilot on 2026/6/6.
//

#pragma once

#include <dal/platform/platform.hpp>
#include <dal/currency/currency.hpp>
#include <dal/protocol/rateconvention.hpp>
#include <dal/time/date.hpp>

namespace Dal {
    class CrossCurrencyMarket_;
    template <class T_> class DiscountCurveT_;
    using DiscountCurve_ = DiscountCurveT_<double>;

    struct CurrencyPair_ {
        Ccy_ domestic_;
        Ccy_ foreign_;

        CurrencyPair_();
        CurrencyPair_(const Ccy_& domestic, const Ccy_& foreign);
        [[nodiscard]] CurrencyPair_ Reversed() const;
        [[nodiscard]] bool operator<(const CurrencyPair_& rhs) const;
        [[nodiscard]] bool operator==(const CurrencyPair_& rhs) const;
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
        CurrencyPair_ pair_;
        double domesticNotional_;
        double foreignNotional_;
        CrossCurrencyConvention_ convention_;

    public:
        CrossCurrencySwap_(const Date_& tradeDate,
                           const Date_& start,
                           const Date_& maturity,
                           double marketRate,
                           const CurrencyPair_& pair,
                           double domesticNotional,
                           double foreignNotional,
                           const CrossCurrencyConvention_& convention = CrossCurrencyConvention_());
        [[nodiscard]] String_ Name() const;
        [[nodiscard]] pair<Date_, Date_> TimeSpan() const;
        [[nodiscard]] double MarketRate() const { return marketRate_; }
        [[nodiscard]] Handle_<Rate_> Precompute() const;
    };
} // namespace Dal

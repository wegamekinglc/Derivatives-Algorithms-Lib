//
// Created by wegam on 2026/4/19.
//

#pragma once

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/time/date.hpp>
#include <dal/time/daybasis.hpp>

namespace Dal {
    class YieldCurve_;
    class String_;

    class YCInstrument_ : noncopyable {
    public:
        virtual ~YCInstrument_() = default;
        [[nodiscard]] virtual String_ Name() const = 0;
        [[nodiscard]] virtual double MarketRate() const = 0;
        [[nodiscard]] virtual pair<Date_, Date_> TimeSpan() const = 0;

        struct Rate_ : noncopyable {
            virtual ~Rate_() = default;
            virtual double operator()(const YieldCurve_& yc) const = 0;
        };

        [[nodiscard]] virtual Handle_<Rate_> Precompute(const Handle_<YCInstrument_>& self,
                                                        const Handle_<YieldCurve_>& funding_yc) const = 0;
    };

    class Deposit_ : public YCInstrument_ {
        Date_ today_;
        Date_ maturity_;
        double marketRate_;
        DayBasis_ basis_;
    public:
        Deposit_(const Date_& today, const Date_& maturity, double marketRate, const DayBasis_& basis);
        ~Deposit_() override;
        [[nodiscard]] String_ Name() const override;
        [[nodiscard]] pair<Date_, Date_> TimeSpan() const override;
        [[nodiscard]] double MarketRate() const override { return marketRate_; }
        [[nodiscard]] Handle_<Rate_> Precompute(const Handle_<YCInstrument_>& self,
                                                const Handle_<YieldCurve_>& funding_yc) const override;
    };

    class Swap_ : public YCInstrument_ {
        Date_ today_;
        Date_ maturity_;
        double marketRate_;
        int freqMonths_;
        DayBasis_ basis_;
    public:
        Swap_(const Date_& today, const Date_& maturity, double marketRate, int freqMonths, const DayBasis_& basis);
        ~Swap_() override;
        [[nodiscard]] String_ Name() const override;
        [[nodiscard]] pair<Date_, Date_> TimeSpan() const override;
        [[nodiscard]] double MarketRate() const override { return marketRate_; }
        [[nodiscard]] Handle_<Rate_> Precompute(const Handle_<YCInstrument_>& self,
                                                const Handle_<YieldCurve_>& funding_yc) const override;
    };
} // namespace Dal

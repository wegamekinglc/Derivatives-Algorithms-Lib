//
// Created by wegam on 2026/4/19.
//

#pragma once

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

namespace Dal {
    class YieldCurve_;
    class String_;
    class Date_;

    class YCInstrument_ : noncopyable {
    public:
        virtual ~YCInstrument_();
        [[nodiscard]] virtual String_ Name() const = 0;
        [[nodiscard]] virtual double MarketRate() const = 0;
        [[nodiscard]] virtual pair<Date_, Date_> TimeSpan() const = 0;

        struct Rate_ : noncopyable {
            virtual ~Rate_();
            virtual double operator()(const YieldCurve_& yc) const = 0;
        };

        [[nodiscard]] virtual Handle_<Rate_> Precompute(const Handle_<YCInstrument_>& self,
                                                        const Handle_<YieldCurve_>& funding_yc) const = 0;
    };
} // namespace Dal

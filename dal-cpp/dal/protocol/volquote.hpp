//
// Created by ZCode on 2026/10/3.
//

#pragma once

#include <cmath>

#include <dal/protocol/rateoption.hpp>
#include <dal/utilities/exceptions.hpp>

/*IF--------------------------------------------------------------------------
enumeration VolConvention
    Quoting convention of a volatility-quoted rate option
switchable
alternative NORMAL
alternative BLACK
alternative SHIFTED_BLACK
-IF-------------------------------------------------------------------------*/

namespace Dal {
#include <dal/auto/MG_VolConvention_enum.hpp>

    struct CalibrationQuote_ {
        String_ name_;
        EuropeanRateOption_ option_;
        double price_ = 0.0;
        double priceScale_ = 1.0;
    };

    struct VolQuote_ {
        String_ name_;
        EuropeanRateOption_ option_;
        double volatility_ = 0.0, priceScale_ = 1.0;
        VolConvention_ convention_ = VolConvention_("NORMAL");
        double shift_ = 0.0;
    };

    struct VolQuoteValue_ {
        double forward_ = 0.0, annuity_ = 0.0, price_ = 0.0, vega_ = 0.0;
    };

    inline void ValidateQuote(const CalibrationQuote_& quote) {
        REQUIRE(!quote.name_.empty(), "InvalidCalibrationQuote: name must be nonempty");
        REQUIRE(std::isfinite(quote.price_), "InvalidCalibrationQuote: price must be finite");
        REQUIRE(std::isfinite(quote.priceScale_) && quote.priceScale_ > 0.0, "InvalidCalibrationQuote: price scale must be finite and positive");
    }

    inline void ValidateQuote(const VolQuote_& quote) {
        REQUIRE(!quote.name_.empty(), "InvalidVolQuote: name must be nonempty");
        REQUIRE(std::isfinite(quote.volatility_) && quote.volatility_ >= 0.0, "InvalidVolQuote: volatility must be finite and nonnegative");
        REQUIRE(std::isfinite(quote.priceScale_) && quote.priceScale_ > 0.0, "InvalidVolQuote: price scale must be finite and positive");
        REQUIRE(std::isfinite(quote.shift_), "InvalidVolQuote: shift must be finite");
        REQUIRE(quote.convention_ == VolConvention_::Value_::SHIFTED_BLACK || quote.shift_ == 0.0,
                "InvalidVolQuote: a nonzero shift requires the SHIFTED_BLACK convention");
    }
} // namespace Dal

//
// Created by Codex on 2026/10/10.
//

#pragma once

#include "ratejacobianfixtures.hpp"

namespace RateTradeCurvatureFixtures {
    struct SingleCurveTerms_ {
        const Dal::String_& key_;
        void operator()(Dal::DepositTradeTerms_& terms) const {
            terms.notional_ = 1.0;
            terms.discountComponentKey_ = key_;
        }
        void operator()(Dal::FraTradeTerms_& terms) const {
            terms.notional_ = 1.0;
            terms.discountComponentKey_ = key_;
            terms.forecastComponentKey_ = key_;
        }
        void operator()(Dal::FutureTradeTerms_& terms) const {
            terms.contractCount_ = 1.0;
            terms.contractValuePerPricePoint_ = 0.01;
            terms.forecastComponentKey_ = key_;
        }
        void Bind(Dal::FixedFloatTradeTerms_& terms) const {
            terms.notional_ = 1.0;
            terms.discountComponentKey_ = key_;
            terms.forecastComponentKey_ = key_;
        }
        void operator()(Dal::IrsTradeTerms_& terms) const { Bind(terms.value_); }
        void operator()(Dal::OisTradeTerms_& terms) const { Bind(terms.value_); }
        void operator()(Dal::BasisTradeTerms_& terms) const {
            terms.notional_ = 1.0;
            terms.discountComponentKey_ = key_;
            terms.referenceForecastComponentKey_ = key_;
            terms.spreadForecastComponentKey_ = key_;
        }
        void operator()(Dal::XccyTradeTerms_&) const {}
    };

    inline Dal::Vector_<Dal::RateTradeDefinition_> SingleFamilyTrades(const Dal::CurveCalibrationSpec_& spec) {
        auto trades = RateJacobianFixtures::ClosedFamilyTrades();
        trades.pop_back();
        for (auto& trade : trades) {
            trade.tradeDate_ = spec.today_;
            trade.startDate_ = Dal::Date::AddMonths(spec.today_, 6);
            trade.maturityDate_ = Dal::Date::AddMonths(spec.today_, 18);
            std::visit(SingleCurveTerms_{spec.curveName_}, trade.terms_);
        }
        return trades;
    }
} // namespace RateTradeCurvatureFixtures

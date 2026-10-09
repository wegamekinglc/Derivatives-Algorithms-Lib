//
// Created by Codex on 2026/10/09.
//

#pragma once

#include <array>

#include <dal/curve/curveblock.hpp>
#include <dal/curve/piecewiseconstant.hpp>
#include <dal/curve/ratestructuraljacobian.hpp>
#include <dal/curve/ycconst.hpp>

namespace RateJacobianFixtures {
    using namespace Dal;
    inline Handle_<DiscountCurve_> FlatCurve(const String_& name, double rate, const Handle_<DiscountCurve_>& base = {}) {
        return Handle_<DiscountCurve_>(NewDiscountPWC(name, "USD", PiecewiseConstant_({Date_(2028, 10, 13)}, {rate}), base));
    }

    inline RatePricingMarket_ ComponentMarket() {
        RatePricingMarket_ market;
        market.valuationTime_ = DateTime_(Date_(2026, 10, 12));
        market.resultCurrency_ = Ccy_("USD");
        market.curveComponents_["A"] = FlatCurve("A", 0.020);
        market.curveComponents_["B"] = FlatCurve("B", 0.005);
        market.curveComponents_["C"] = FlatCurve("C", 0.010, market.curveComponents_.at("B"));
        market.curveComponents_["D"] = FlatCurve("D", 0.030);
        market.curveComponents_["E"] = FlatCurve("E", 0.004, market.curveComponents_.at("A"));
        return market;
    }

    inline RateTradeDefinition_ Deposit(const String_& id, const String_& key) {
        DepositTradeTerms_ terms;
        terms.notional_ = 1000000.0;
        terms.contractRate_ = 0.025;
        terms.discountComponentKey_ = key;
        RateTradeDefinition_ trade;
        trade.instrumentId_ = id;
        trade.instrumentType_ = RateInstrumentType_("DEPOSIT");
        trade.tradeDate_ = Date_(2026, 10, 12);
        trade.startDate_ = Date_(2026, 10, 13);
        trade.maturityDate_ = Date_(2027, 10, 13);
        trade.currencyOrPair_ = Ccy_("USD");
        trade.terms_ = terms;
        return trade;
    }

    inline RateTradeDefinition_ Fra() {
        auto trade = Deposit("fra-E", "A");
        FraTradeTerms_ terms;
        terms.notional_ = 750000.0;
        terms.contractRate_ = 0.022;
        terms.settleAtStart_ = false;
        terms.index_.forecastTenor_ = PeriodLength_("12M");
        terms.fixingIdentity_ = {"USD-12M", 10, 30};
        terms.forecastComponentKey_ = "E";
        terms.discountComponentKey_ = "A";
        trade.instrumentType_ = RateInstrumentType_("FRA");
        trade.terms_ = terms;
        return trade;
    }

    inline RateTradeDefinition_ Irs(double contractRate = 0.025) {
        auto trade = Deposit("delayed-payment-irs", "A");
        IrsTradeTerms_ terms;
        auto& value = terms.value_;
        value.notional_ = 1000000.0;
        value.contractRate_ = contractRate;
        value.fixedLeg_.paymentFrequency_ = PeriodLength_("12M");
        value.floatLeg_.paymentFrequency_ = PeriodLength_("12M");
        value.fixedLeg_.paymentLag_ = 2;
        value.floatLeg_.paymentLag_ = 2;
        value.floatIndex_.forecastTenor_ = PeriodLength_("12M");
        value.fixingIdentity_ = {"USD-12M", 10, 30};
        value.forecastComponentKey_ = "C";
        value.discountComponentKey_ = "A";
        trade.instrumentType_ = RateInstrumentType_("IRS");
        trade.terms_ = terms;
        return trade;
    }

    inline RatePricingMarket_ MarketAt(const std::array<double, 5>& parameters) {
        auto market = ComponentMarket();
        market.curveComponents_["A"] = FlatCurve("A", parameters[2]);
        market.curveComponents_["B"] = FlatCurve("B", parameters[4]);
        market.curveComponents_["C"] = FlatCurve("C", parameters[0], market.curveComponents_.at("B"));
        market.curveComponents_["D"] = FlatCurve("D", parameters[1]);
        market.curveComponents_["E"] = FlatCurve("E", parameters[3], market.curveComponents_.at("A"));
        return market;
    }

    inline RateTradeDefinition_ Xccy() {
        auto trade = Deposit("xccy", "A");
        XccyTradeTerms_ terms;
        terms.positionCount_ = 1.0;
        terms.contractSpread_ = 0.001;
        terms.config_.pair_ = CurrencyPair_(Ccy_("USD"), Ccy_("EUR"));
        terms.config_.domesticNotional_ = 1000000.0;
        terms.config_.foreignNotional_ = 900000.0;
        terms.config_.convention_.domesticLeg_.paymentFrequency_ = PeriodLength_("12M");
        terms.config_.convention_.foreignLeg_.paymentFrequency_ = PeriodLength_("12M");
        terms.config_.convention_.domesticIndex_.forecastTenor_ = PeriodLength_("12M");
        terms.config_.convention_.foreignIndex_.forecastTenor_ = PeriodLength_("12M");
        terms.config_.domesticRateFixing_ = {"USD-12M", 10, 30};
        terms.config_.foreignRateFixing_ = {"EUR-12M", 10, 30};
        trade.instrumentType_ = RateInstrumentType_("XCCY");
        trade.terms_ = terms;
        return trade;
    }

    inline RatePricingMarket_ XccyMarket() {
        auto market = ComponentMarket();
        market.curveComponents_["F"] = Handle_<DiscountCurve_>(NewDiscountPWC("F", "EUR", PiecewiseConstant_({Date_(2028, 10, 13)}, {0.010})));
        const auto domestic = Handle_<CurveBlock_>(new CurveBlock_(market.curveComponents_.at("A")));
        const auto foreign = Handle_<CurveBlock_>(new CurveBlock_(market.curveComponents_.at("F")));
        auto native = std::make_shared<CrossCurrencyMarket_>(domestic, foreign, 1.2, market.valuationTime_, Ccy_("USD"));
        native->SetBasisCurve(market.curveComponents_.at("B"));
        market.xccyMarket_ = native;
        return market;
    }

    class DerivedCurve_ : public Tape::DiscountPWC_<double> {
    public:
        using Tape::DiscountPWC_<double>::DiscountPWC_;
    };

    struct ReferencePoint_ {
        std::array<double, 5> parameters_;
        double swapRate_;
        std::array<double, 3> pv_;
        std::array<std::array<double, 5>, 3> jacobian_;
    };
    inline std::array<ReferencePoint_, 3> ReferencePoints() {
        const std::array<ReferencePoint_, 3> points = {{{{{0.010, 0.030, 0.020, 0.004, 0.005}},
                                                         0.025,
                                                         {{-9689.568010118917, -2646.446531297031, 1683.6326620064187}},
                                                         {{{{994848.9289403273, 0.0, 9769.208295133594, 0.0, 994848.9289403273}},
                                                           {{0.0, -497305.2087286761, 0.0, 0.0, 0.0}},
                                                           {{0.0, 0.0, 751276.5031810036, 752964.7485352347, 0.0}}}}},
                                                        {{{-0.005, 0.030, 0.020, 0.004, 0.005}},
                                                         0.0,
                                                         {{0.0, -2646.446531297031, 1683.6326620064187}},
                                                         {{{{980037.5580004354, 0.0, 0.0, 0.0, 980037.5580004354}},
                                                           {{0.0, -497305.2087286761, 0.0, 0.0, 0.0}},
                                                           {{0.0, 0.0, 751276.5031810036, 752964.7485352347, 0.0}}}}},
                                                        {{{0.005, 0.031, 0.022, 0.006, 0.005}},
                                                         0.0,
                                                         {{9829.70022476586, -3143.5018048536767, 4692.104053521708}},
                                                         {{{{987893.0639921811, 0.0, -9910.492281407773, 0.0, 987893.0639921811}},
                                                           {{0.0, -496805.4219112989, 0.0, 0.0, 0.0}},
                                                           {{0.0, 0.0, 749763.0917502991, 754468.0508834195, 0.0}}}}}}};
        return points;
    }
    inline Vector_<Dal::RateTradeDefinition_> ClosedFamilyTrades() {
        const auto irs = Irs();
        auto ois = irs;
        ois.instrumentId_ = "ois";
        ois.instrumentType_ = RateInstrumentType_("OIS");
        ois.terms_ = OisTradeTerms_{std::get<IrsTradeTerms_>(irs.terms_).value_};
        auto future = Fra();
        future.instrumentId_ = "future";
        future.instrumentType_ = RateInstrumentType_("FUTURE");
        FutureTradeTerms_ futureTerms;
        futureTerms.contractCount_ = 10.0;
        futureTerms.referencePrice_ = 99.0;
        futureTerms.contractValuePerPricePoint_ = 25.0;
        futureTerms.index_.forecastTenor_ = PeriodLength_("12M");
        futureTerms.fixingIdentity_ = {"USD-12M", 10, 30};
        futureTerms.forecastComponentKey_ = "E";
        future.terms_ = futureTerms;
        auto basis = irs;
        basis.instrumentId_ = "basis";
        basis.instrumentType_ = RateInstrumentType_("BASIS_SWAP");
        BasisTradeTerms_ basisTerms;
        basisTerms.notional_ = 1000000.0;
        basisTerms.contractSpread_ = 0.001;
        basisTerms.spreadLeg_ = std::get<IrsTradeTerms_>(irs.terms_).value_.floatLeg_;
        basisTerms.referenceLeg_ = basisTerms.spreadLeg_;
        basisTerms.spreadIndex_.forecastTenor_ = PeriodLength_("12M");
        basisTerms.referenceIndex_.forecastTenor_ = PeriodLength_("12M");
        basisTerms.spreadFixingIdentity_ = {"USD-12M", 10, 30};
        basisTerms.referenceFixingIdentity_ = {"USD-12M", 10, 30};
        basisTerms.spreadForecastComponentKey_ = "C";
        basisTerms.referenceForecastComponentKey_ = "E";
        basisTerms.discountComponentKey_ = "A";
        basis.terms_ = basisTerms;
        return {Deposit("deposit-D", "D"), Fra(), future, ois, irs, basis, Xccy()};
    }

} // namespace RateJacobianFixtures

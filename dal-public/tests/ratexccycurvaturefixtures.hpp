//
// Created by Codex on 2026/10/10.
//

#pragma once

#include <algorithm>

#include <dal/curve/calibration_internal.hpp>
#include <dal/curve/curveparameterization.hpp>
#include <dal/curve/xccyjointcalibration.hpp>
#include <dal/curve/ycconst.hpp>
#include <dal/time/holidays.hpp>

namespace RateXccyCurvatureFixtures {
    inline Dal::RateIndexConvention_ XccyIndex(const char* tenor) {
        Dal::RateIndexConvention_ index;
        index.forecastTenor_ = Dal::PeriodLength_(tenor);
        index.useProjectionCurve_ = true;
        index.collateral_ = Dal::CollateralType_(Dal::CollateralType_::Value_::OIS);
        index.dayBasis_ = Dal::DayBasis::Act365F();
        index.businessDayConvention_ = Dal::BizDayConvention_("Unadjusted");
        index.fixingHolidays_ = Dal::Holidays::None();
        index.accrualHolidays_ = Dal::Holidays::None();
        index.fixingLag_ = 0;
        return index;
    }

    inline Dal::RateLegConvention_ XccyLeg() {
        Dal::RateLegConvention_ leg;
        leg.paymentFrequency_ = Dal::PeriodLength_("6M");
        leg.dayBasis_ = Dal::DayBasis::Act365F();
        leg.businessDayConvention_ = Dal::BizDayConvention_("Unadjusted");
        leg.accrualHolidays_ = Dal::Holidays::None();
        leg.paymentHolidays_ = Dal::Holidays::None();
        return leg;
    }

    inline Dal::Handle_<Dal::DiscountCurve_> FixedCurve(const Dal::Date_& today, const char* ccy, const char* name, double rate) {
        return Dal::Handle_<Dal::DiscountCurve_>(new Dal::Tape::DiscountPWC_<double>(name, ccy, {today}, {rate}));
    }

    inline Dal::Handle_<Dal::CurveBlock_>
    FixedBlock(const Dal::Date_& today, const char* ccy, const char* tenor, double discount, double projection) {
        const Dal::CollateralType_ ois(Dal::CollateralType_::Value_::OIS);
        return Dal::Handle_<Dal::CurveBlock_>(new Dal::CurveBlock_("fixed", ccy, {{ois, FixedCurve(today, ccy, "discount", discount)}},
                                                                   {{Dal::PeriodLength_(tenor), FixedCurve(today, ccy, "projection", projection)}},
                                                                   Dal::DayBasis_("ACT/360")));
    }

    inline Dal::CrossCurrencyCalibrationSpec_ StagedSpec(Dal::XccyNotionalMode_ mode = Dal::XccyNotionalMode_::Value_::FIXED) {
        Dal::CrossCurrencyCalibrationSpec_ spec;
        spec.today_ = Dal::Date_(2025, 1, 16);
        spec.valuationTime_ = Dal::DateTime_(spec.today_, 9, 0);
        spec.basisPair_ = Dal::CurrencyPair_(Dal::Ccy_("USD"), Dal::Ccy_("EUR"));
        spec.collateralCurrency_ = spec.basisPair_.domestic_;
        spec.domesticCurveBlock_ = FixedBlock(spec.today_, "USD", "6M", 0.02, 0.03);
        spec.foreignCurveBlock_ = FixedBlock(spec.today_, "EUR", "3M", 0.01, 0.023);
        spec.fxSpot_ = 1.1;
        spec.tolerance_ = 1.0e-14;
        spec.initialGuess_ = 0.001;
        spec.knotDates_ = {Dal::Date::AddMonths(spec.today_, 6), Dal::Date::AddMonths(spec.today_, 18)};
        spec.fixings_ = Dal::Handle_<Dal::MarketFixingSnapshot_>(new Dal::MarketFixingSnapshot_());
        Dal::CrossCurrencySwapConfig_ config;
        config.pair_ = spec.basisPair_;
        config.domesticNotional_ = 110.0;
        config.foreignNotional_ = 100.0;
        config.notionalMode_ = mode;
        config.fxReset_.fixingLag_ = 0;
        config.fxReset_.fixingHour_ = 10;
        config.fxReset_.fixingMinute_ = 0;
        config.fxReset_.fixingHolidays_ = Dal::Holidays::None();
        config.domesticRateFixing_ = {"USD-AAD-XCCY-CURVATURE", 10, 0};
        config.foreignRateFixing_ = {"EUR-AAD-XCCY-CURVATURE", 10, 0};
        config.convention_.domesticIndex_ = XccyIndex("6M");
        config.convention_.foreignIndex_ = XccyIndex("3M");
        config.convention_.domesticLeg_ = XccyLeg();
        config.convention_.foreignLeg_ = XccyLeg();
        Dal::CrossCurrencyMarket_ market(spec.domesticCurveBlock_, spec.foreignCurveBlock_, spec.fxSpot_, spec.valuationTime_,
                                         spec.collateralCurrency_, spec.fixings_);
        market.SetBasisCurve(
            Dal::Handle_<Dal::DiscountCurve_>(new Dal::Tape::DiscountPWC_<double>("basis", "USD", spec.knotDates_, {0.001, 0.0014})));
        const auto start = Dal::Date::AddMonths(spec.today_, 1);
        for (int months : {24, 12}) {
            const auto maturity = Dal::Date::AddMonths(start, months);
            const Dal::CrossCurrencySwap_ prototype(spec.today_, start, maturity, 0.0, config);
            spec.instruments_.push_back(Dal::Handle_<Dal::CrossCurrencySwap_>(
                new Dal::CrossCurrencySwap_(spec.today_, start, maturity, (*prototype.Precompute())(market), config)));
        }
        return spec;
    }

    inline Dal::Handle_<Dal::YCInstrument_>
    CurrencyInstrument(const Dal::Date_& today, const Dal::Date_& maturity, const Dal::RateIndexConvention_& index, double quote, bool discount) {
        if (discount)
            return Dal::Handle_<Dal::YCInstrument_>(new Dal::Deposit_(today, today, maturity, quote, index));
        return Dal::Handle_<Dal::YCInstrument_>(new Dal::Swap_(today, today, maturity, quote, XccyLeg(), index, XccyLeg()));
    }

    inline Dal::JointCurveDeclaration_ CurrencyDeclaration(const Dal::CrossCurrencyCalibrationSpec_& staged, bool domestic, int block, bool layered) {
        const auto& market = domestic ? staged.domesticCurveBlock_ : staged.foreignCurveBlock_;
        Dal::JointCurveDeclaration_ curve;
        curve.curveName_ = block == 0 ? "discount" : "projection";
        curve.calibrateDiscountCurve_ = block == 0;
        curve.targetCollateral_ = Dal::CollateralType_(Dal::CollateralType_::Value_::OIS);
        curve.targetTenor_ = Dal::PeriodLength_(domestic ? "6M" : "3M");
        curve.baseLayeredOverDiscount_ = layered && block != 0;
        curve.parameterization_ = Dal::CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD;
        auto index = XccyIndex(domestic ? "6M" : "3M");
        index.useProjectionCurve_ = block != 0;
        const int count = domestic == (block == 0) ? 2 : 1;
        for (int ordinal = 0; ordinal < count; ++ordinal) {
            const auto maturity = Dal::Date::AddMonths(staged.today_, 12 * (ordinal + 1));
            curve.knotDates_.push_back(Dal::Date::AddMonths(maturity, -6));
            const auto prototype = CurrencyInstrument(staged.today_, maturity, index, 0.0, block == 0);
            const double quote = (*prototype->Precompute(Dal::Handle_<Dal::YieldCurve_>()))(*market);
            curve.instruments_.push_back(CurrencyInstrument(staged.today_, maturity, index, quote, block == 0));
        }
        std::reverse(curve.instruments_.begin(), curve.instruments_.end());
        return curve;
    }

    inline Dal::JointCurrencyCurveSpec_ CurrencySpec(const Dal::CrossCurrencyCalibrationSpec_& staged, bool domestic, bool layered) {
        Dal::JointCurrencyCurveSpec_ currency;
        currency.ccy_ = domestic ? staged.basisPair_.domestic_ : staged.basisPair_.foreign_;
        currency.liborBasis_ = Dal::DayBasis::Act365F();
        for (int block = 0; block < 2; ++block)
            currency.curves_.push_back(CurrencyDeclaration(staged, domestic, block, layered));
        return currency;
    }

    inline Dal::JointXccyCalibrationSpec_ JointSpec(bool layered = true, Dal::XccyNotionalMode_ mode = Dal::XccyNotionalMode_::Value_::FIXED) {
        const auto staged = StagedSpec(mode);
        Dal::JointXccyCalibrationSpec_ spec;
        spec.valuationTime_ = staged.valuationTime_;
        spec.pair_ = staged.basisPair_;
        spec.collateralCurrency_ = staged.collateralCurrency_;
        spec.fxSpot_ = staged.fxSpot_;
        spec.domestic_ = CurrencySpec(staged, true, layered);
        spec.foreign_ = CurrencySpec(staged, false, layered);
        spec.basis_.instruments_ = staged.instruments_;
        spec.basis_.knotDates_ = staged.knotDates_;
        spec.fixings_ = staged.fixings_;
        spec.tolerance_ = 1.0e-14;
        spec.initialGuess_ = 0.015;
        return spec;
    }

    inline Dal::Handle_<Dal::CrossCurrencySwap_> QuoteXccy(const Dal::Handle_<Dal::CrossCurrencySwap_>& instrument, double quote) {
        const auto span = instrument->TimeSpan();
        return Dal::Handle_<Dal::CrossCurrencySwap_>(
            new Dal::CrossCurrencySwap_(instrument->TradeDate(), span.first, span.second, quote, instrument->Config()));
    }

    inline Dal::Handle_<Dal::YCInstrument_> QuoteYc(const Dal::Handle_<Dal::YCInstrument_>& instrument, double quote) {
        const auto span = instrument->TimeSpan();
        if (const auto* deposit = dynamic_cast<const Dal::Deposit_*>(instrument.get()))
            return Dal::Handle_<Dal::YCInstrument_>(
                new Dal::Deposit_(deposit->TradeDate(), span.first, span.second, quote, deposit->FloatConvention()));
        const auto& swap = dynamic_cast<const Dal::Swap_&>(*instrument);
        return Dal::Handle_<Dal::YCInstrument_>(new Dal::Swap_(swap.TradeDate(), span.first, span.second, quote, swap.FixedLegConvention(),
                                                               swap.FloatConvention(), swap.FloatLegConvention()));
    }

    inline Dal::JointCurrencyCurveSpec_ QuoteCurrency(Dal::JointCurrencyCurveSpec_ currency, const Dal::Vector_<>& quotes, size_t* offset) {
        for (auto& curve : currency.curves_) {
            curve.instruments_ = Dal::OrderInstruments(curve.instruments_);
            for (auto& instrument : curve.instruments_)
                instrument = QuoteYc(instrument, quotes[(*offset)++]);
        }
        return currency;
    }

} // namespace RateXccyCurvatureFixtures

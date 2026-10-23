#pragma once

#include <dal/platform/platform.hpp>

#include <dal/currency/currencydata.hpp>
#include <dal/curve/xccybasiscalibration.hpp>

namespace Dal::TestXccy {
    inline const Date_ TODAY(2026, 10, 21), START(2027, 5, 4), END(2027, 11, 4);
    inline Handle_<DiscountCurve_> Flat(const String_& ccy, double rate) {
        const auto definition = MakeCurveDefinition(ccy, ccy, CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD,
                                                    LogDfScheme_::Value_::LOG_LINEAR, {END}, TODAY, DayBasis::Act365F());
        return Handle_<DiscountCurve_>(BuildDiscountCurveT<double>(definition, {rate}));
    }
    inline Handle_<CurveBlock_> Block(const String_& ccy, double rate, const char* tenor) {
        const auto curve = Flat(ccy, rate);
        return Handle_<CurveBlock_>(new CurveBlock_(ccy, ccy, {{CollateralType_("OIS"), curve}}, {{PeriodLength_(tenor), curve}}));
    }
    inline CrossCurrencySwapConfig_ Config(XccyNotionalMode_ mode = XccyNotionalMode_::Value_::FIXED) {
        CrossCurrencySwapConfig_ config;
        config.domesticNotional_ = 110.;
        config.foreignNotional_ = 100.;
        config.notionalMode_ = mode;
        config.convention_.initialNotionalExchange_ = config.convention_.finalNotionalExchange_ = true;
        config.convention_.domesticLeg_.paymentFrequency_ = PeriodLength_("3M");
        config.convention_.foreignLeg_.paymentFrequency_ = PeriodLength_("6M");
        config.convention_.foreignLeg_.paymentLag_ = 2;
        for (auto* leg : {&config.convention_.domesticLeg_, &config.convention_.foreignLeg_}) {
            leg->dayBasis_ = DayBasis_("ACT_360");
            leg->businessDayConvention_ = leg->paymentConvention_ = BizDayConvention_("Unadjusted");
        }
        for (auto* index : {&config.convention_.domesticIndex_, &config.convention_.foreignIndex_}) {
            index->useProjectionCurve_ = true;
            index->fixingLag_ = 2;
            index->dayBasis_ = DayBasis_("ACT_360");
        }
        config.convention_.domesticIndex_.forecastTenor_ = PeriodLength_("3M");
        config.convention_.foreignIndex_.forecastTenor_ = PeriodLength_("6M");
        config.convention_.domesticIndex_.fixingHolidays_ = Ccy::Conventions::LiborFixHolidays()(Ccy_("USD"));
        config.convention_.foreignIndex_.fixingHolidays_ = Ccy::Conventions::LiborFixHolidays()(Ccy_("EUR"));
        config.domesticRateFixing_ = {"IR:USD,LIBOR_3M_LCH", 0, 0};
        config.foreignRateFixing_ = {"IR:EUR,LIBOR_6M_LCH", 0, 0};
        config.fxReset_.fixingLag_ = config.fxReset_.fixingHour_ = config.fxReset_.fixingMinute_ = 0;
        return config;
    }
    inline XccyInstrumentCalibrationSpec_ Spec(XccyNotionalMode_ mode = XccyNotionalMode_::Value_::FIXED) {
        XccyInstrumentCalibrationSpec_ spec;
        spec.context_.valuationTime_ = DateTime_(TODAY);
        spec.context_.pair_ = CurrencyPair_(Ccy_("USD"), Ccy_("EUR"));
        spec.context_.collateralCurrency_ = Ccy_("USD");
        spec.context_.discountRoute_ = "USD.OIS";
        spec.context_.fixings_.reset(new MarketFixingSnapshot_({}));
        spec.domesticCurveBlock_ = Block("USD", .04, "3M");
        spec.foreignCurveBlock_ = Block("EUR", .02, "6M");
        spec.fxSpot_ = 1.1;
        spec.knotDates_ = {END};
        const CrossCurrencySwap_ native(TODAY, START, END, .001, Config(mode));
        const auto instrument = NewXccyScriptInstrument(native, {"xccy", "USD.OIS", true, 1e-12});
        CrossCurrencyMarket_ market(spec.domesticCurveBlock_, spec.foreignCurveBlock_, spec.fxSpot_, spec.context_.valuationTime_, Ccy_("USD"),
                                    spec.context_.fixings_);
        market.SetBasisCurve(Flat("USD", .003));
        const auto quote = NewCalibrationMarketQuote("basis", (*native.Precompute())(market), CalibrationValueUnit_::Parse("BasisSpread"));
        spec.observations_ = {NewCalibrationObservation("basis-row", instrument, "BasisSpread", quote, 1e-10)};
        return spec;
    }
} // namespace Dal::TestXccy

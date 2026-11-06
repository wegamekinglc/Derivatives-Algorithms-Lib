#pragma once

#include <dal-public/src/calibrationinstrument.hpp>
#include <dal-public/src/curvedata.hpp>
#include <dal-public/src/global.hpp>

namespace Dal::Test {
    inline CurveInstrumentCalibrationSpec_ ScriptQuoteRiskSpec() {
        const Date_ today(2026, 10, 21), first(2027, 10, 21), last(2028, 10, 23);
        CurveInstrumentCalibrationSpec_ spec;
        spec.geometry_.today_ = today;
        spec.geometry_.ccy_ = "USD";
        spec.geometry_.parameterization_ = CurveParameterization_("LOG_DISCOUNT");
        spec.geometry_.knotDates_ = {today, first, last};
        spec.valuation_.evaluationDate_ = today;
        spec.valuation_.fixings_.reset(new MarketFixingSnapshot_());
        spec.market_.routes_ = {{"USD.OIS"}};
        for (int i = 0; i < 2; ++i) {
            const auto end = i == 0 ? first : last;
            const String_ id = i == 0 ? "first" : "last";
            const Handle_<YCInstrument_> native(new Deposit_(today, end, .025, DayBasis_("ACT_360")));
            const auto instrument = NewDepositScriptInstrument(native, {id, Ccy_("USD"), 100., "USD.OIS", 1e-12});
            const auto quote = NewCalibrationMarketQuote(id, .035 + .005 * i, CalibrationValueUnit_::Parse("Rate"));
            spec.observations_.push_back(NewCalibrationObservation(id, instrument, "ParRate", quote, 1e-12));
        }
        return spec;
    }
    inline CurveInstrumentCalibrationSpec_ ScriptBasisQuoteRiskSpec() {
        InitGlobalData(1);
        auto spec = ScriptQuoteRiskSpec();
        const auto today = spec.geometry_.today_;
        const Date_ end(2027, 10, 21);
        spec.geometry_.knotDates_ = {today, end};
        spec.geometry_.calibrateDiscountCurve_ = false;
        spec.geometry_.targetTenor_ = PeriodLength_("3M");
        spec.market_.discountCurves_[CollateralType_("OIS")] = DiscountPWCNew("discount", "USD", {today}, {.02}, {});
        spec.market_.forwardCurves_[PeriodLength_("6M")] = DiscountPWCNew("six-month", "USD", {today}, {.045}, {});
        RateIndexConvention_ spread;
        spread.dayBasis_ = DayBasis_("ACT_360");
        spread.businessDayConvention_ = BizDayConvention_("Unadjusted");
        spread.useProjectionCurve_ = true;
        spread.forecastTenor_ = PeriodLength_("3M");
        auto reference = spread;
        reference.forecastTenor_ = PeriodLength_("6M");
        RateLegConvention_ spreadLeg;
        spreadLeg.paymentFrequency_ = PeriodLength_("3M");
        spreadLeg.dayBasis_ = DayBasis_("ACT_360");
        spreadLeg.businessDayConvention_ = spreadLeg.paymentConvention_ = BizDayConvention_("Unadjusted");
        auto referenceLeg = spreadLeg;
        referenceLeg.paymentFrequency_ = PeriodLength_("6M");
        const Handle_<YCInstrument_> native(new BasisSwap_(today, today, end, -.001, spread, spreadLeg, reference, referenceLeg));
        const auto instrument = NewBasisSwapScriptInstrument(native, {"basis", Ccy_("USD"), 100., "USD.OIS", 1e-12});
        const auto quote = NewCalibrationMarketQuote("basis-q", -.001, CalibrationValueUnit_::Parse("BasisSpread"));
        spec.observations_ = {NewCalibrationObservation("basis-o", instrument, "BasisSpread", quote, 1e-13)};
        spec.market_.routes_.push_back(
            {CurveScriptProjectionRoute(Ccy_("USD"), PeriodLength_("3M")), true, CollateralType_("OIS"), PeriodLength_("3M")});
        spec.market_.routes_.push_back(
            {CurveScriptProjectionRoute(Ccy_("USD"), PeriodLength_("6M")), true, CollateralType_("OIS"), PeriodLength_("6M")});
        return spec;
    }
} // namespace Dal::Test

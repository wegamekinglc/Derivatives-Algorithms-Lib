#pragma once

#include <dal/curve/yclogdf.hpp>
#include <dal/model/xccygaussiandata.hpp>

#include "xccyinstrumentfixture.hpp"

namespace Dal::TestXccyMoments {
    inline double Native(const PreparedCalibrationInstrument_& prepared, const XccyGaussianModelSpec_& spec) {
        Tape::JointCurveBlock_<double> domestic, foreign;
        const auto fill = [](const CurveBlock_& source, Tape::JointCurveBlock_<double>* target) {
            for (const auto& [key, curve] : source.DiscountCurves())
                target->discountCurves[key] = curve.get();
            for (const auto& [key, curve] : source.ForwardCurves())
                target->forwardCurves[key] = curve.get();
        };
        fill(*spec.market_.domesticBlock_, &domestic);
        fill(*spec.market_.foreignBlock_, &foreign);
        return EvaluateXccyScript(prepared,
                                  XccyMarketView_<double>{spec.market_.valuationTime_, *spec.market_.pair_, spec.market_.collateralCurrency_,
                                                          spec.market_.fxSpot_, &domestic, &foreign, spec.market_.basis_.get()});
    }
    inline Handle_<DiscountCurve_> Curve(const String_& currency, double rate, const Date_& valuation = TestXccy::TODAY) {
        return Handle_<DiscountCurve_>(NewDiscountLogDF("curve", currency, {valuation, TestXccy::TODAY.AddDays(1095)},
                                                        {0., -rate * (TestXccy::TODAY.AddDays(1095) - valuation) / 365.}, DayBasis_("ACT365F"),
                                                        LogDfScheme_::Value_::LOG_LINEAR));
    }
    inline XccyGaussianModelSpec_ ModelSpec(const Date_& valuation = TestXccy::TODAY, bool zero = false) {
        XccyGaussianModelSpec_ spec;
        auto& market = spec.market_;
        market.valuationTime_ = DateTime_(valuation);
        market.pair_ = CurrencyPair_(Ccy_("USD"), Ccy_("EUR"));
        market.collateralCurrency_ = Ccy_("USD");
        market.fxSpot_ = 1.1;
        market.discountRoute_ = "USD.OIS";
        market.domesticBlock_.reset(new CurveBlock_("domestic", "USD", {{CollateralType_("OIS"), Curve("USD", .04, valuation)}},
                                                    {{PeriodLength_("3M"), Curve("USD", .045, valuation)}}));
        market.foreignBlock_.reset(new CurveBlock_("foreign", "EUR", {{CollateralType_("OIS"), Curve("EUR", .02, valuation)}},
                                                   {{PeriodLength_("6M"), Curve("EUR", .026, valuation)}}));
        market.basis_ = Curve("USD", .003, valuation);
        auto& dynamics = spec.dynamics_;
        dynamics.domesticA_ = .03;
        dynamics.foreignA_ = .07;
        dynamics.bucketEndDates_ = {TestXccy::TODAY.AddDays(1095)};
        dynamics.domesticSigma_ = {zero ? 0. : .01};
        dynamics.foreignSigma_ = {zero ? 0. : .009};
        dynamics.fxSigma_ = {zero ? 0. : .12};
        dynamics.factorNames_ = {"rate:USD", "rate:EUR", "fx:EUR/USD"};
        dynamics.correlation_ = Matrix_<>(3, 3, 0.0);
        const double correlation[3][3] = {{1., .30, -.20}, {.30, 1., -.25}, {-.20, -.25, 1.}};
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                dynamics.correlation_(i, j) = correlation[i][j];
        return spec;
    }
    inline Handle_<DiscountCurve_> Nonflat(const String_& currency, double rate, double quadratic) {
        Vector_<Date_> dates;
        Vector_<> values;
        for (const int days : {0, 180, 365, 730, 1095}) {
            const double t = days / 365.;
            dates.push_back(TestXccy::TODAY.AddDays(days));
            values.push_back(-rate * t - quadratic * t * t);
        }
        return Handle_<DiscountCurve_>(NewDiscountLogDF("nonflat", currency, dates, values, DayBasis_("ACT365F"), LogDfScheme_::Value_::LOG_LINEAR));
    }
    inline XccyGaussianModelSpec_ NonflatSpec() {
        auto spec = ModelSpec();
        spec.dynamics_.bucketEndDates_ = {TestXccy::TODAY.AddDays(180), TestXccy::TODAY.AddDays(365), TestXccy::TODAY.AddDays(1095)};
        spec.dynamics_.domesticSigma_ = {.008, .012, .010};
        spec.dynamics_.foreignSigma_ = {.006, .011, .009};
        spec.dynamics_.fxSigma_ = {.10, .14, .12};
        spec.market_.domesticBlock_.reset(new CurveBlock_("domestic", "USD", {{CollateralType_("OIS"), Nonflat("USD", .04, .002)}},
                                                          {{PeriodLength_("3M"), Nonflat("USD", .045, .0015)}}));
        spec.market_.foreignBlock_.reset(new CurveBlock_("foreign", "EUR", {{CollateralType_("OIS"), Nonflat("EUR", .02, .001)}},
                                                         {{PeriodLength_("6M"), Nonflat("EUR", .026, .0008)}}));
        spec.market_.basis_ = Nonflat("USD", .003, -.0002);
        return spec;
    }
    inline PreparedCalibrationInstrument_
    Prepare(const Handle_<CalibrationInstrument_>& instrument,
            const XccyGaussianModelSpec_& model,
            const String_& measure = "Price",
            const Handle_<MarketFixingSnapshot_>& fixings = Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_({}))) {
        XccyScriptContext_ context;
        context.valuationTime_ = model.market_.valuationTime_;
        context.pair_ = *model.market_.pair_;
        context.collateralCurrency_ = model.market_.collateralCurrency_;
        context.discountRoute_ = model.market_.discountRoute_;
        context.fixings_ = fixings;
        CalibrationPreparationContext_ preparation;
        preparation.xccy_ = std::make_shared<const XccyScriptContext_>(context);
        return PrepareCalibrationInstrument(instrument, measure, preparation);
    }
    inline Handle_<CalibrationInstrument_> Instrument(XccyNotionalMode_ mode = XccyNotionalMode_::Value_::FIXED,
                                                      bool foreignSpread = false,
                                                      bool receive = true,
                                                      const Date_& maturity = TestXccy::END) {
        auto config = TestXccy::Config(mode);
        config.convention_.spreadOnForeignLeg_ = foreignSpread;
        const CrossCurrencySwap_ native(TestXccy::TODAY, TestXccy::START, maturity, .001, config);
        return NewXccyScriptInstrument(native, {"xccy-" + Date::ToString(maturity), "USD.OIS", receive, 1e-12});
    }
    inline XccyInstrumentCalibrationSpec_ SyntheticSpec() {
        const auto model = ModelSpec();
        auto spec = TestXccy::Spec();
        spec.domesticCurveBlock_ = model.market_.domesticBlock_;
        spec.foreignCurveBlock_ = model.market_.foreignBlock_;
        spec.gaussianDynamics_.reset(new XccyGaussianDynamics_("dynamics", model.dynamics_));
        spec.basisParameterization_ = CurveParameterization_::Value_::LOG_DISCOUNT;
        spec.knotDates_ = {Date_(2027, 11, 8), Date_(2028, 5, 8), Date_(2028, 11, 8)};
        const Vector_<Date_> maturities{Date_(2027, 11, 4), Date_(2028, 5, 4), Date_(2028, 11, 4)};
        const Vector_<> quotes{-.06705259701832678066767811569223010914, -.05451280603364857706446589746300234964,
                               -.04991219154599001675595706358737932267};
        spec.observations_.clear();
        for (size_t i = 0; i < maturities.size(); ++i) {
            const String_ id = "maturity-" + String::FromInt(i);
            const auto quote = NewCalibrationMarketQuote(id, quotes[i], CalibrationValueUnit_::Parse("BasisSpread"));
            spec.observations_.push_back(NewCalibrationObservation(
                id, Instrument(XccyNotionalMode_::Value_::MARK_TO_MARKET, false, true, maturities[i]), "BasisSpread", quote, 1e-4));
            spec.solver_.initialGuessPerNode_.push_back(-.003 * (spec.knotDates_[i] - TestXccy::TODAY) / 365. + .001);
        }
        return spec;
    }
} // namespace Dal::TestXccyMoments

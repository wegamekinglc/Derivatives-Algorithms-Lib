#include <iomanip>
#include <iostream>

#include "../../../dal-cpp/examples/floatformat.hpp"
#include "../xccyexample.hpp"
#include <dal-public/src/global.hpp>
#include <dal/curve/yclogdf.hpp>

int main() {
    using namespace Dal;
    InitGlobalData(1);
    const Date_ valuation(2026, 10, 21), start(2027, 5, 4), end(2027, 11, 4), horizon = valuation.AddDays(1095);
    auto contract = Examples::XccyContract();
    contract.notionalMode_ = XccyNotionalMode_::Value_::MARK_TO_MARKET;
    contract.fxReset_.fixingLag_ = contract.fxReset_.fixingHour_ = contract.fxReset_.fixingMinute_ = 0;
    Handle_<CrossCurrencySwap_> native(new CrossCurrencySwap_(valuation, start, end, .001, contract));
    const auto instrument = NewXccyScriptInstrument(native, {"eur-usd-mtm", "USD.OIS", true, 1e-12});
    native.reset();
    const auto curve = [&](const String_& currency, double rate) {
        return Handle_<DiscountCurve_>(
            NewDiscountLogDF(currency, currency, {valuation, horizon}, {0., -rate * 3.}, DayBasis_("ACT365F"), LogDfScheme_::Value_::LOG_LINEAR));
    };
    XccyGaussianModelSpec_ modelSpec;
    auto& market = modelSpec.market_;
    market.valuationTime_ = DateTime_(valuation);
    market.pair_ = CurrencyPair_(Ccy_("USD"), Ccy_("EUR"));
    market.collateralCurrency_ = Ccy_("USD");
    market.fxSpot_ = 1.1;
    market.discountRoute_ = "USD.OIS";
    market.domesticBlock_.reset(
        new CurveBlock_("usd", "USD", {{CollateralType_("OIS"), curve("USD", .04)}}, {{PeriodLength_("3M"), curve("USD", .045)}}));
    market.foreignBlock_.reset(
        new CurveBlock_("eur", "EUR", {{CollateralType_("OIS"), curve("EUR", .02)}}, {{PeriodLength_("6M"), curve("EUR", .026)}}));
    market.basis_ = curve("USD", .003);
    auto& dynamics = modelSpec.dynamics_;
    dynamics.domesticA_ = .03;
    dynamics.foreignA_ = .07;
    dynamics.bucketEndDates_ = {horizon};
    dynamics.domesticSigma_ = {.01};
    dynamics.foreignSigma_ = {.009};
    dynamics.fxSigma_ = {.12};
    dynamics.factorNames_ = {"rate:USD", "rate:EUR", "fx:EUR/USD"};
    dynamics.correlation_ = Matrix_<>(3, 3, 0.);
    for (int i = 0; i < 3; ++i)
        dynamics.correlation_(i, i) = 1.;
    dynamics.correlation_(0, 1) = dynamics.correlation_(1, 0) = .30;
    dynamics.correlation_(0, 2) = dynamics.correlation_(2, 0) = -.20;
    dynamics.correlation_(1, 2) = dynamics.correlation_(2, 1) = -.25;
    const auto initialModel = NewXccyGaussianModel("xccy", modelSpec);
    const Handle_<MarketFixingSnapshot_> fixings(new MarketFixingSnapshot_({}));
    const auto target = ValueXccyInstrumentByGaussian(instrument, initialModel, "BasisSpread", fixings);
    const auto quote = NewCalibrationMarketQuote("synthetic-par", target.value_, CalibrationValueUnit_::Parse("BasisSpread"));
    XccyInstrumentCalibrationSpec_ spec;
    spec.context_ = {market.valuationTime_, *market.pair_, market.collateralCurrency_, market.discountRoute_};
    spec.context_.fixings_ = fixings;
    spec.domesticCurveBlock_ = market.domesticBlock_;
    spec.foreignCurveBlock_ = market.foreignBlock_;
    spec.fxSpot_ = market.fxSpot_;
    spec.knotDates_ = {Date_(2027, 11, 8)};
    spec.observations_ = {NewCalibrationObservation("fit-par", instrument, "BasisSpread", quote, 1e-7)};
    spec.gaussianDynamics_.reset(new XccyGaussianDynamics_("frozen", dynamics));
    spec.basisParameterization_ = CurveParameterization_::Value_::LOG_DISCOUNT;
    spec.solver_.initialGuess_ = -.002;
    const auto fit = CalibrateXccyInstrumentMarket(spec);
    REQUIRE(fit.fitWithinTolerance_, "synthetic staged fit within explicit scale");
    const auto fitted = XccyInstrumentResultGaussianModel(fit);
    const auto fast = ValueXccyInstrumentByGaussian(instrument, fitted, "Price", fixings);
    XccyMonteCarloRequest_ request;
    request.measureId_ = "Price";
    request.phase_ = XccyMonteCarloPhase_::Value_::HeldOut;
    request.sampling_ = {"PseudoRandom", 256, 4, 1024, "dal.mrg32-normal-paths/1"};
    request.fixings_ = fixings;
    request.derivativeMethod_ = CalibrationDerivative_::Value_::AAD;
    request.parameterLabels_ = {"fxspot:EUR/USD"};
    const auto mc = ValueXccyInstrumentByMonteCarlo(instrument, fitted, request);
    const auto& details = XccyMonteCarloDetails(mc);
    std::cout << "Same-source stochastic XCCY (synthetic fit; raw partial holds fitted curves fixed)\n"
              << std::left << std::setw(22) << "Method / phase" << std::right << std::setw(18) << "Price USD" << std::setw(18) << "Batch SE USD"
              << '\n'
              << std::string(58, '-') << '\n'
              << std::left << std::setw(22) << "GaussianFast" << std::right << std::setw(18) << ExampleFloat(fast.value_) << '\n'
              << std::left << std::setw(22) << "ScriptMC / HeldOut" << std::right << std::setw(18) << ExampleFloat(mc.mean_) << std::setw(18)
              << ExampleFloat(*mc.standardError_) << '\n'
              << "Par target / fitted: " << ExampleFloat(target.value_) << " / " << ExampleFloat(fit.modelMeasures_[0])
              << "; scaled residual: " << ExampleFloat(fit.scaledResiduals_[0]) << "; fit accepted: " << fit.fitWithinTolerance_ << '\n'
              << "Raw dPrice/dSpot: " << ExampleFloat(details.estimate_.gradient_[0])
              << "; SE: " << ExampleFloat(details.estimate_.gradientStandardError_[0]) << '\n'
              << "Phase prefix: [" << details.reservation_.ranges_.front().firstPath_ << ',' << details.reservation_.ranges_.back().pathEnd_ << ")"
              << "; dimension: " << details.mapping_.dimension_ << "; numerical acceptance: unavailable; quote risk: unavailable\n";
}

#include "../xccyexample.hpp"
#include <iomanip>
#include <iostream>

#include "../../../dal-cpp/examples/floatformat.hpp"
#include <dal-public/src/calibrationinstrument.hpp>
#include <dal-public/src/global.hpp>
#include <dal-public/src/xccycalibration.hpp>
#include <dal/currency/currencydata.hpp>

using Dal::Examples::XccyContract;

int main() {
    using namespace Dal;
    InitGlobalData(1);
    const Date_ today(2026, 10, 21), start(2027, 5, 4), end(2027, 11, 4);
    const Handle_<CrossCurrencySwap_> native(new CrossCurrencySwap_(today, start, end, .001, XccyContract()));
    const auto instrument = NewXccyScriptInstrument(native, {"eur-usd", "USD.OIS", true, 1e-12});
    const auto quote = NewCalibrationMarketQuote("eur-usd-basis", -.002983490837363863, CalibrationValueUnit_::Parse("BasisSpread"));
    XccyInstrumentCalibrationSpec_ spec;
    spec.context_ = {DateTime_(today), CurrencyPair_(Ccy_("USD"), Ccy_("EUR")), Ccy_("USD"), "USD.OIS"};
    spec.context_.fixings_.reset(new MarketFixingSnapshot_());
    const auto block = [&](const String_& ccy, double rate, const char* tenor) {
        const auto def = MakeCurveDefinition(ccy, ccy, CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD, LogDfScheme_::Value_::LOG_LINEAR,
                                             {end}, today, DayBasis::Act365F());
        const Handle_<DiscountCurve_> curve(BuildDiscountCurveT<double>(def, {rate}));
        return Handle_<CurveBlock_>(new CurveBlock_(ccy, ccy, {{CollateralType_("OIS"), curve}}, {{PeriodLength_(tenor), curve}}));
    };
    spec.domesticCurveBlock_ = block("USD", .04, "3M");
    spec.foreignCurveBlock_ = block("EUR", .02, "6M");
    spec.fxSpot_ = 1.1;
    spec.knotDates_ = {end};
    spec.observations_ = {NewCalibrationObservation("fit-basis", instrument, "BasisSpread", quote, 1e-10)};
    CurveInstrumentCalibrationOptions_ options;
    options.jacobianMode_ = CalibrationDerivative_::Value_::AAD;
    options.computeQuoteJacobian_ = true;
    const auto fit = CalibrateXccyInstrumentMarket(spec, options);
    REQUIRE(fit.fitWithinTolerance_, "XCCY example fit must pass");
    const auto priceQuote = NewCalibrationMarketQuote("contract-price", 0., CalibrationValueUnit_::Parse("Price:USD"));
    spec.observations_ = {NewCalibrationObservation("price", instrument, "Price", priceQuote, 1e-8)};
    const auto price = PreparedXccyInstruments_(spec, options).Evaluate(fit.parameters_);
    std::cout << "XCCY script calibration (decimal spread; basis forward in Rate)\n"
              << std::left << std::setw(18) << "Observation" << std::right << std::setw(18) << "Market" << std::setw(18) << "Fitted" << std::setw(18)
              << "Raw residual" << '\n'
              << std::string(72, '-') << '\n'
              << std::left << std::setw(18) << "fit-basis" << std::right << std::setw(18) << ExampleFloat(quote->Value()) << std::setw(18)
              << ExampleFloat(fit.modelMeasures_[0]) << std::setw(18) << ExampleFloat(fit.rawResiduals_[0]) << '\n'
              << "Basis forward: " << ExampleFloat(fit.parameters_[0]) << "; fit accepted: " << fit.fitWithinTolerance_ << '\n'
              << "Same bound Price:USD: " << ExampleFloat(price.modelMeasures_[0])
              << "; raw dPrice/dBasisForward: " << ExampleFloat(price.quoteJacobian_(0, 0)) << '\n';
    const auto capabilities = XccyInstrumentCapabilities(instrument, "Price", spec.context_);
    std::cout << "MC capability: " << capabilities->Data().plans_.back().reasonCode_.value_or("") << '\n';
}

#include <iomanip>
#include <iostream>

#include "../../../dal-cpp/examples/floatformat.hpp"
#include <dal-public/src/global.hpp>
#include <dal-public/src/gsr.hpp>

int main() {
    using namespace Dal;
    InitGlobalData(1);
    const Date_ today(2026, 10, 21), expiry(2027, 10, 21), maturity(2028, 10, 21);
    const Handle_<GSRCurveData_> curve(new GSRCurveData_("curve", today, "USD", {today, maturity}, {0., -.08}, {}, {}));
    const auto model = [&](double g) {
        const Handle_<MultiFactorGSRVolData_> vol(
            new MultiFactorGSRVolData_("vol", {"level"}, {today}, Matrix_<>(1, 1, g), {today}, Matrix_<>(1, 1, 1.), Matrix_<>(1, 1, 1.)));
        return Handle_<ModelData_>(new MultiFactorGSRModelData_("model", curve, vol));
    };
    const BondOption_ option{expiry, maturity, .96, OptionType_("Call")};
    const auto instrument = NewEuropeanRateOptionScriptInstrument(option, {"usd-bond", Ccy_("USD"), 100., "USD.OIS"});
    const double target = 100. * PriceGSREuropeanOption(model(.024), option).price_;
    const auto quote = NewCalibrationMarketQuote("market-price", target, CalibrationValueUnit_::Parse("Price:USD"));
    const auto row = NewCalibrationObservation("fit-bond", instrument, "Price", quote, 1e-6);
    GSRInstrumentCalibrationSettings_ settings;
    settings.valuation_.evaluationDate_ = today;
    settings.valuation_.fixings_.reset(new MarketFixingSnapshot_());
    const auto fit = CalibrateGSRInstruments(model(.018), {row}, {{0, 0, .001, .1}}, settings);
    REQUIRE(fit.converged_ && fit.fitWithinTolerance_ && fit.numericalValidationPassed_, "example fast calibration must be accepted");
    CalibrationMonteCarloRequest_ request;
    request.measureId_ = "Price";
    request.valuation_ = settings.valuation_;
    request.sampling_ = {"PseudoRandom", 128, 2, 1024, "dal.mrg32-normal-paths/1"};
    request.simulation_.smooth_ = 0.;
    const Handle_<ModelData_> fitted(fit.model_);
    const auto plain = ValueCalibrationInstrumentByMonteCarlo(instrument, fitted, request);
    request.derivativeMethod_ = CalibrationDerivative_::Value_::AAD;
    const auto aad = ValueCalibrationInstrumentByMonteCarlo(instrument, fitted, request);
    const auto rebound = BindCalibrationInstrumentParameters(instrument, {{{"K", .97, CalibrationValueUnit_::Parse("BondPrice")}}, {}});
    const auto changed = ValueCalibrationInstrumentByMonteCarlo(rebound, fitted, request);
    REQUIRE(std::abs(plain.mean_ - aad.mean_) < 1e-10, "same-path plain and AAD prices must agree");
    REQUIRE(fit.report_->Data().observations_[0].instrument_->InstrumentIdentity() == instrument->InstrumentIdentity(),
            "fit retains original binding");
    REQUIRE(aad.report_->Data().observations_[0].instrument_->InstrumentIdentity() == instrument->InstrumentIdentity(),
            "MC retains original binding");
    REQUIRE(rebound->BindingIdentity() != instrument->BindingIdentity(), "explicit rebind creates a new binding identity");
    std::cout << "GSR script calibration (Price:USD; native g in Rate)\n"
              << "Instrument      Market           Fitted           Residual          g              Accepted\n"
              << "----------------------------------------------------------------------------------------\n"
              << std::left << std::setw(16) << "usd-bond" << std::setw(17) << ExampleFloat(target, 6) << std::setw(17)
              << ExampleFloat(fit.modelPrices_[0], 6) << std::setw(18) << ExampleFloat(fit.residuals_[0], 6) << std::setw(15)
              << ExampleFloat(fit.parameters_[0], 6) << "true\n\n"
              << "Same contract pricing (Price:USD; MC sampling errors)\n"
              << "Method          Price            Standard error\n"
              << "-----------------------------------------------\n";
    for (const auto& value : {std::make_pair("MC plain", plain), std::make_pair("MC AAD", aad), std::make_pair("Rebound K=.97", changed)})
        std::cout << std::setw(16) << value.first << std::setw(17) << ExampleFloat(value.second.mean_, 6)
                  << ExampleFloat(*value.second.standardError_, 6) << '\n';
    const auto partials = CalibrationReportQuoteJacobian(aad.report_);
    std::cout << "\nNative MC partials (calibrated inputs held fixed; no calibrated quote risk)\n";
    for (size_t j = 0; j < aad.report_->Data().parameters_.size(); ++j)
        std::cout << std::setw(32) << aad.report_->Data().parameters_[j].id_ << ExampleFloat(partials(0, static_cast<int>(j)), 6) << '\n';
    std::cout << "Original binding: " << instrument->BindingIdentity() << '\n' << "Rebound binding:  " << rebound->BindingIdentity() << '\n';
}

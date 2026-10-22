#include <cmath>
#include <iomanip>
#include <iostream>

#include <dal-public/src/calibrationinstrument.hpp>
#include <dal-public/src/global.hpp>

int main() {
    using namespace Dal;
    InitGlobalData(1);
    const Date_ today(2026, 10, 21), maturity(2027, 10, 21);
    const Handle_<YCInstrument_> native(new Deposit_(today, maturity, .025, DayBasis_("ACT_360")));
    const auto instrument = NewDepositScriptInstrument(native, {"usd-deposit", Ccy_("USD"), 100., "USD.OIS", 1e-12});
    const auto quote = NewCalibrationMarketQuote("usd-1y", .035, CalibrationValueUnit_::Parse("Rate"));
    CurveInstrumentCalibrationSpec_ spec;
    spec.geometry_.today_ = today;
    spec.geometry_.ccy_ = "USD";
    spec.geometry_.knotDates_ = {today, maturity};
    spec.geometry_.parameterization_ = CurveParameterization_("LOG_DISCOUNT");
    spec.market_.routes_ = {{"USD.OIS"}};
    spec.valuation_.evaluationDate_ = today;
    spec.valuation_.fixings_.reset(new MarketFixingSnapshot_());
    spec.observations_ = {NewCalibrationObservation("fit-1y", instrument, "ParRate", quote, 1e-10)};
    const auto fit = CalibrateCurveInstruments(spec);
    const Handle_<GSRCurveData_> curve(
        new GSRCurveData_("matched-curve", today, "USD", {today, maturity}, {0., std::log((*fit.curve_)(today, maturity))}, {}, {}));
    const Handle_<GSRVolData_> volatility(new GSRVolData_("deterministic", {today}, {0.}, {today}, {1.}));
    const Handle_<ModelData_> model(new GSRModelData_("matched-model", curve, volatility));
    CalibrationMonteCarloRequest_ request;
    request.measureId_ = "Price";
    request.valuation_ = spec.valuation_;
    request.sampling_ = {"PseudoRandom", 16, 2, 1024, "dal.mrg32-normal-paths/1"};
    request.simulation_.smooth_ = 0.;
    const auto price = ValueCalibrationInstrumentByMonteCarlo(instrument, model, request);
    const auto rebound = BindCalibrationInstrumentParameters(instrument, {{{"K", quote->Value(), quote->Unit()}}, {}});
    const auto parPrice = ValueCalibrationInstrumentByMonteCarlo(rebound, model, request);
    REQUIRE(std::abs(parPrice.mean_) < 1e-8, "rebound Deposit must have zero PV on the calibrated curve");
    REQUIRE(std::abs(price.mean_ - parPrice.mean_) > .1, "fixed coupon must retain its original contract");
    std::cout << std::setprecision(12) << "ParRate (decimal): " << fit.modelMeasures_[0] << '\n'
              << "Fixed K=0.025 Price:USD: " << price.mean_ << '\n'
              << "Rebound K=0.035 Price:USD: " << parPrice.mean_ << '\n'
              << "Quote risk available: " << std::boolalpha << fit.report_->QuoteRiskAvailable() << '\n';
}

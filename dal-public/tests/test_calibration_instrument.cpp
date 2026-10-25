#include <gtest/gtest.h>
#include <rapidjson/document.h>

#include <dal-public/src/calibrationinstrument.hpp>

TEST(PublicCalibrationInstrumentTest, TestImportedBindingAndOwningReportJson) {
    const Dal::Date_ today(2026, 10, 21), end(2027, 10, 21);
    Dal::Handle_<Dal::YCInstrument_> native(new Dal::Deposit_(today, end, .025, Dal::DayBasis_("ACT_360")));
    const auto instrument = Dal::NewDepositScriptInstrument(native, {"deposit", Dal::Ccy_("USD"), 100., "USD.OIS", 1e-12});
    const auto rebound = Dal::BindCalibrationInstrumentParameters(instrument, {{{"K", .03, Dal::CalibrationValueUnit_::Parse("Rate")}}, {}});
    ASSERT_NE(instrument->BindingIdentity(), rebound->BindingIdentity());
    ASSERT_EQ(Dal::LoadCalibrationInstrumentJson(Dal::DumpCalibrationInstrumentJson(rebound))->BindingIdentity(), rebound->BindingIdentity());
    Dal::CurveInstrumentCalibrationSpec_ spec;
    spec.geometry_.today_ = today;
    spec.geometry_.ccy_ = "USD";
    spec.geometry_.knotDates_ = {today, end};
    spec.geometry_.parameterization_ = Dal::CurveParameterization_("LOG_DISCOUNT");
    spec.valuation_.evaluationDate_ = today;
    spec.valuation_.fixings_.reset(new Dal::MarketFixingSnapshot_());
    spec.market_.routes_ = {{"USD.OIS"}};
    const auto quote = Dal::NewCalibrationMarketQuote("q", .035, Dal::CalibrationValueUnit_::Parse("Rate"));
    spec.observations_ = {Dal::NewCalibrationObservation("o", rebound, "ParRate", quote, 1e-10)};
    const auto result = Dal::CalibrateCurveInstruments(spec);
    rapidjson::Document json;
    json.Parse(Dal::DumpCalibrationReportJson(result.report_).c_str());
    ASSERT_FALSE(json.HasParseError());
    ASSERT_FALSE(json["summary"]["quote_risk_available"].GetBool());
    ASSERT_EQ(json["observations"].Size(), 1u);
    ASSERT_STREQ(json["observations"][0]["derivative_method"].GetString(), "FiniteDifference");
    ASSERT_DOUBLE_EQ(json["observations"][0]["model_value"].GetDouble(), result.modelMeasures_[0]);
    ASSERT_TRUE(json["quote_jacobian"].IsNull());
    ASSERT_DOUBLE_EQ(json["quote_map"][0][0].GetDouble(), 1.);
}

TEST(PublicCalibrationInstrumentTest, TestSameBoundInstrumentMonteCarloUsesOwnedRequest) {
    const Dal::Date_ today(2026, 10, 21), end(2027, 10, 21);
    Dal::Handle_<Dal::YCInstrument_> native(new Dal::Deposit_(today, end, .025, Dal::DayBasis_("ACT_360")));
    const auto instrument = Dal::NewDepositScriptInstrument(native, {"deposit", Dal::Ccy_("USD"), 100., "USD.OIS", 1e-12});
    const Dal::Handle_<Dal::GSRCurveData_> curve(new Dal::GSRCurveData_("curve", today, "USD", {today, end}, {0., -.04}, {}, {}));
    const Dal::Handle_<Dal::GSRVolData_> vol(new Dal::GSRVolData_("vol", {today}, {0.}, {today}, {1.}));
    const Dal::Handle_<Dal::ModelData_> model(new Dal::GSRModelData_("model", curve, vol));
    Dal::CalibrationMonteCarloRequest_ request;
    request.measureId_ = "Price";
    request.valuation_.evaluationDate_ = today;
    request.valuation_.fixings_.reset(new Dal::MarketFixingSnapshot_());
    request.sampling_ = {"PseudoRandom", 8, 2, 1024, "dal.mrg32-normal-paths/1"};
    request.simulation_.smooth_ = 0.;
    const auto value = Dal::ValueCalibrationInstrumentByMonteCarlo(instrument, model, request);
    ASSERT_NEAR(value.mean_, -100. + 100. * (1. + .025 * 365. / 360.) * std::exp(-.04), 1e-10);
    ASSERT_TRUE(value.standardError_);
    ASSERT_EQ(value.report_->Data().observations_[0].instrument_->BindingIdentity(), instrument->BindingIdentity());
    ASSERT_EQ(value.report_->Data().observations_[0].derivativeMethod_, "None");
    ASSERT_FALSE(value.report_->Data().quoteJacobian_);
    ASSERT_FALSE(value.report_->QuoteRiskAvailable());
    const auto rebound = Dal::BindCalibrationInstrumentParameters(instrument, {{{"K", .03, Dal::CalibrationValueUnit_::Parse("Rate")}}, {}});
    const auto changed = Dal::ValueCalibrationInstrumentByMonteCarlo(rebound, model, request);
    ASSERT_NEAR(changed.mean_ - value.mean_, 100. * .005 * 365. / 360. * std::exp(-.04), 1e-10);
}

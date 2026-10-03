//
// Created by Codex on 2026/10/2.
//

#include <gtest/gtest.h>

#include <dal-public/src/gsr.hpp>
#include <dal-public/src/models.hpp>
#include <dal/platform/initall.hpp>

using namespace Dal;

TEST(PublicGSREuropeanTest, TestPricingAndCalibrationFacade) {
    RegisterAll_::Init();
    const Date_ today(2026, 10, 2);
    const auto curve = NewGSRCurveData("curve", today, "USD", {today, today.AddDays(1095)}, {0.0, -0.09}, {}, Matrix_<>(0, 0));
    MultiFactorGSRVolSettings_ settings;
    settings.factorNames_ = {"level"};
    settings.gKnotDates_ = settings.hKnotDates_ = {today};
    settings.gValues_ = Matrix_<>(1, 1, 0.02);
    settings.hValues_ = settings.correlations_ = Matrix_<>(1, 1, 1.0);
    const auto model = NewMultiFactorGSRModelData("rates", curve, NewMultiFactorGSRVolData("vol", settings));
    const BondOption_ option{today.AddDays(365), today.AddDays(730), 0.97, OptionType_("CALL")};
    const auto priced = PriceGSREuropeanOption(model, option);
    ASSERT_GT(priced.price_, 0.0);
    const Vector_<CalibrationQuote_> quotes{{"bond", option, priced.price_, 1e-6}};
    const auto fitted = CalibrateGSRVolatility(model, quotes, {{0, 0, 0.0, 0.1}});
    ASSERT_TRUE(fitted.converged_);
    ASSERT_TRUE(fitted.fitWithinTolerance_);
    ASSERT_NEAR(PriceGSREuropeanOption(Handle_<ModelData_>(fitted.model_), option).price_, priced.price_, 1e-12);
    ASSERT_THROW(PriceGSREuropeanOption(Handle_<ModelData_>(), option), Exception_);
    const auto legacy = NewGSRModelData("legacy", curve, NewGSRVolData("vol", {today}, {0.02}, {today}, {1.0}));
    ASSERT_NEAR(PriceGSREuropeanOption(legacy, option).price_, priced.price_, 1e-12);
    ASSERT_THROW(CalibrateGSRVolatility(legacy, quotes, {{0, 0, 0.0, 0.1}}), Exception_);
    const auto smile = NewGSRSLVModelData("smile", model, NewGSRLeverageData("leverage", {0.0}, {0.0}, Matrix_<>(1, 1, 1.0)));
    const Caplet_ caplet{option.expiry_, option.expiry_, option.maturity_, option.maturity_, 1.0, 1.0, "12M", 0.03, OptionType_("CALL")};
    const Vector_<VolQuote_> marketQuotes{{"normal", caplet, 0.02, 0.01}};
    const auto converted = ConvertVolQuotes(Handle_<Storable_>(curve), marketQuotes);
    ASSERT_GT(converted[0].price_, 0.0);
    GSRSLVCalibrationSettings_ calibration;
    calibration.pricing_.paths_ = calibration.validation_.paths_ = 512;
    calibration.solver_.priorWeight_ = 1.0;
    calibration.useAADJacobian_ = true;
    const Vector_<GSRSLVCalibrationParameter_> parameters{{"leverage:0:0", 0.2, 2.0, 1.0}};
    ASSERT_TRUE(CalibrateGSRSLVMarket(smile, marketQuotes, parameters, calibration).converged_);
    ASSERT_GT(GSRSLVMarketQuoteRisk(smile, marketQuotes, parameters, {caplet}, calibration).sensitivities_(0, 0), 0.0);
    ASSERT_THROW(ConvertVolQuotes(Handle_<Storable_>(), marketQuotes), Exception_);
    ASSERT_THROW(CalibrateGSRSLVMarket(model, marketQuotes, parameters, calibration), Exception_);
}

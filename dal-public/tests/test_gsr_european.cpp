//
// Created by Codex on 2026/10/2.
//

#include <gtest/gtest.h>

#include <dal-public/src/gsr.hpp>
#include <dal-public/src/models.hpp>

using namespace Dal;

TEST(PublicGSREuropeanTest, TestPricingAndCalibrationFacade) {
    const Date_ today(2026, 10, 2);
    const auto curve = NewGSRCurveData("curve", today, "USD", {today, today.AddDays(1095)}, {0.0, -0.09}, {}, Matrix_<>(0, 0));
    MultiFactorGSRVolSettings_ settings;
    settings.factorNames_ = {"level"};
    settings.gKnotDates_ = settings.hKnotDates_ = {today};
    settings.gValues_ = Matrix_<>(1, 1, 0.02);
    settings.hValues_ = settings.correlations_ = Matrix_<>(1, 1, 1.0);
    const auto model = NewMultiFactorGSRModelData("rates", curve, NewMultiFactorGSRVolData("vol", settings));
    const GSRBondOption_ option{today.AddDays(365), today.AddDays(730), 0.97, OptionType_("CALL")};
    const auto priced = PriceGSREuropeanOption(model, option);
    ASSERT_GT(priced.price_, 0.0);
    const Vector_<GSRCalibrationQuote_> quotes{{"bond", option, priced.price_, 1e-6}};
    const auto fitted = CalibrateGSRVolatility(model, quotes, {{0, 0, 0.0, 0.1}});
    ASSERT_TRUE(fitted.converged_);
    ASSERT_TRUE(fitted.fitWithinTolerance_);
    ASSERT_NEAR(PriceGSREuropeanOption(Handle_<ModelData_>(fitted.model_), option).price_, priced.price_, 1e-12);
    ASSERT_THROW(PriceGSREuropeanOption(Handle_<ModelData_>(), option), Exception_);
    const auto legacy = NewGSRModelData("legacy", curve, NewGSRVolData("vol", {today}, {0.02}, {today}, {1.0}));
    ASSERT_NEAR(PriceGSREuropeanOption(legacy, option).price_, priced.price_, 1e-12);
    ASSERT_THROW(CalibrateGSRVolatility(legacy, quotes, {{0, 0, 0.0, 0.1}}), Exception_);
}

//
// Created by Codex on 2026/10/2.
//

#include <gtest/gtest.h>

#include <dal-excel/src/__gsr_test_api.hpp>
#include <dal-public/src/models.hpp>

using namespace Dal;

TEST(ExcelGSREuropeanTest, TestPricingCalibrationAndResultViews) {
    const Date_ today(2026, 10, 2);
    const auto curve = NewGSRCurveData("curve", today, "USD", {today, today.AddDays(1095)}, {0.0, -0.09}, {}, Matrix_<>(0, 0));
    MultiFactorGSRVolSettings_ settings;
    settings.factorNames_ = {"level"};
    settings.gKnotDates_ = settings.hKnotDates_ = {today};
    settings.gValues_ = Matrix_<>(1, 1, 0.02);
    settings.hValues_ = settings.correlations_ = Matrix_<>(1, 1, 1.0);
    const auto model = NewMultiFactorGSRModelData("rates", curve, NewMultiFactorGSRVolData("vol", settings));
    Handle_<StorableGSREuropeanOption_> option;
    GSRBondOption_New(today.AddDays(365), today.AddDays(730), 0.97, "CALL", &option);
    Matrix_<Cell_> price;
    GSR_EuropeanOptionPrice(model, option, Matrix_<Cell_>(0, 0), &price);
    ASSERT_EQ(price.Cols(), 2);
    const double target = Cell::ToDouble(price(0, 0));
    ASSERT_GT(target, 0.0);
    Matrix_<Cell_> pricingSettings(2, 2);
    pricingSettings(0, 0) = Cell_("quadratureOrder");
    pricingSettings(0, 1) = Cell_(24.0);
    pricingSettings(1, 0) = Cell_("estimateError");
    pricingSettings(1, 1) = Cell_(false);
    GSR_EuropeanOptionPrice(model, option, pricingSettings, &price);
    ASSERT_NEAR(Cell::ToDouble(price(0, 0)), target, 1e-12);
    pricingSettings(0, 1) = Cell_(24.5);
    ASSERT_THROW(GSR_EuropeanOptionPrice(model, option, pricingSettings, &price), Exception_);
    pricingSettings(0, 1) = Cell_(24.0);
    pricingSettings(1, 0) = Cell_("unknown");
    ASSERT_THROW(GSR_EuropeanOptionPrice(model, option, pricingSettings, &price), Exception_);
    Handle_<StorableGSRCalibrationQuote_> quote;
    GSRCalibrationQuote_New("bond", option, target, 1e-6, &quote);
    Matrix_<> parameters(1, 4);
    parameters(0, 0) = parameters(0, 1) = parameters(0, 2) = 0.0;
    parameters(0, 3) = 0.1;
    Handle_<StorableGSRCalibrationResult_> result;
    const Vector_<Handle_<Storable_>> quotes{Handle_<Storable_>(quote)};
    Calibrate_GSRVolatility(model, quotes, parameters, Matrix_<Cell_>(0, 0), &result);
    Matrix_<Cell_> diagnostics;
    GSRCalibrationResult_Get(result, "converged", &diagnostics);
    ASSERT_TRUE(Cell::ToBool(diagnostics(0, 0)));
    GSRCalibrationResult_Get(result, "parameters", &diagnostics);
    ASSERT_NEAR(Cell::ToDouble(diagnostics(0, 0)), 0.02, 1e-12);
    Handle_<ModelData_> calibrated;
    GSRCalibrationResult_Get_Model(result, &calibrated);
    ASSERT_NEAR(PriceGSREuropeanOption(calibrated, option->option_).price_, target, 1e-12);
    ASSERT_THROW(GSRCalibrationResult_Get(result, "misspelled", &diagnostics), Exception_);
    parameters(0, 0) = 0.5;
    ASSERT_THROW(Calibrate_GSRVolatility(model, quotes, parameters, Matrix_<Cell_>(0, 0), &result), Exception_);
}

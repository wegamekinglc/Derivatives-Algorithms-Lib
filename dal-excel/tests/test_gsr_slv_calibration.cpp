//
// Created by Codex on 2026/10/2.
//

#include <gtest/gtest.h>

#include <dal-excel/src/__gsrslv_test_api.hpp>
#include <dal-public/src/models.hpp>
#include <dal-public/src/global.hpp>

using namespace Dal;

TEST(ExcelGSRSLVCalibrationTest, TestPricingFitDiagnosticsAndQuoteRisk) {
    InitGlobalData(1);
    const Date_ today(2026, 10, 2);
    const auto curve = NewGSRCurveData("curve", today, "USD", {today, today.AddDays(1095)}, {0.0, -0.09}, {}, Matrix_<>(0, 0));
    MultiFactorGSRVolSettings_ vol;
    vol.factorNames_ = {"level"};
    vol.gKnotDates_ = vol.hKnotDates_ = {today};
    vol.gValues_ = Matrix_<>(1, 1, 0.02);
    vol.hValues_ = vol.correlations_ = Matrix_<>(1, 1, 1.0);
    const auto gaussian = NewMultiFactorGSRModelData("rates", curve, NewMultiFactorGSRVolData("vol", vol));
    const auto initial = NewGSRSLVModelData("smile", gaussian, NewGSRLeverageData("leverage", {0.0}, {0.0}, Matrix_<>(1, 1, 1.0)));
    Handle_<StorableGSREuropeanOption_> option;
    GSRBondOption_New(today.AddDays(365), today.AddDays(730), 0.97, "CALL", &option);
    const Vector_<Handle_<Storable_>> options{Handle_<Storable_>(option)};
    Matrix_<Cell_> settings(3, 2);
    settings(0, 0) = Cell_("paths");
    settings(0, 1) = Cell_(512.0);
    settings(1, 0) = Cell_("validationPaths");
    settings(1, 1) = Cell_(1024.0);
    settings(2, 0) = Cell_("priorWeight");
    settings(2, 1) = Cell_(1.0);
    Matrix_<Cell_> prices;
    Matrix_<Cell_> pricing(1, 2);
    pricing(0, 0) = Cell_("paths");
    pricing(0, 1) = Cell_(512.0);
    GSRSLV_EuropeanOptionPrices(initial, options, pricing, &prices);
    ASSERT_EQ(prices.Cols(), 2);
    Handle_<StorableGSRCalibrationQuote_> quote;
    GSRCalibrationQuote_New("bond", option, Cell::ToDouble(prices(0, 0)), 0.01, &quote);
    const Vector_<Handle_<Storable_>> quotes{Handle_<Storable_>(quote)};
    Matrix_<Cell_> parameters(1, 4);
    parameters(0, 0) = Cell_("leverage:0:0");
    parameters(0, 1) = Cell_(0.2);
    parameters(0, 2) = Cell_(2.0);
    parameters(0, 3) = Cell_(1.0);
    Handle_<StorableGSRSLVCalibrationResult_> result;
    Calibrate_GSRSLV(initial, quotes, parameters, settings, {}, &result);
    Matrix_<Cell_> value;
    GSRSLVCalibrationResult_Get(result, "converged", &value);
    ASSERT_TRUE(Cell::ToBool(value(0, 0)));
    GSRSLVCalibrationResult_Get(result, "parameters", &value);
    ASSERT_NEAR(Cell::ToDouble(value(0, 0)), 1.0, 1e-10);
    Handle_<ModelData_> fitted;
    GSRSLVCalibrationResult_Get_Model(result, &fitted);
    ASSERT_TRUE(fitted);
    Handle_<StorableGSRSLVQuoteRiskResult_> risk;
    GSRSLV_QuoteRisk(initial, quotes, parameters, options, settings, Matrix_<Cell_>(0, 0), {}, &risk);
    GSRSLVQuoteRiskResult_Get(risk, "sensitivities", &value);
    ASSERT_GT(Cell::ToDouble(value(0, 0)), 0.0);
    ASSERT_THROW(GSRSLVCalibrationResult_Get(result, "unknown", &value), Exception_);
    ASSERT_THROW(GSRSLV_QuoteRisk({}, quotes, parameters, options, settings, Matrix_<Cell_>(0, 0), {}, &risk), Exception_);
    settings(2, 0) = Cell_("unknown");
    ASSERT_THROW(Calibrate_GSRSLV(initial, quotes, parameters, settings, {}, &result), Exception_);
}

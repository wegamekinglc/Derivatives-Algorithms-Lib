//
// Created by Codex on 2026/10/2.
//

#include <gtest/gtest.h>

#include <cmath>

#include <dal-excel/src/__curvepricing_test_api.hpp>
#include <dal-excel/src/__gsrslv_test_api.hpp>
#include <dal-excel/src/__script_test_api.hpp>
#include <dal-public/src/curveinstrument.hpp>
#include <dal-public/src/curveprotocol.hpp>
#include <dal-public/src/models.hpp>
#include <dal-public/src/global.hpp>

using namespace Dal;

TEST(ExcelGSRSLVCalibrationTest, TestPricingFitDiagnosticsAndNativeCurveQuoteRisk) {
    InitGlobalData(1);
    Excel::ScriptTestInitialize(1);
    const Date_ today(2026, 10, 2);
    CurveCalibrationSpecBuilder_ builder;
    builder.today_ = today;
    builder.ccy_ = "USD";
    builder.curveName_ = "curve";
    builder.parameterization_ = CurveParameterization_::Value_::LOG_DISCOUNT;
    builder.knotDates_ = {today, today.AddDays(1095)};
    builder.tolerance_ = 1e-10;
    const auto index = RateIndexConvention_New(PeriodLength_New("12M"), DayBasis_New("ACT_365F"), CollateralType_OIS());
    builder.instruments_ = {DepositNew(today, today, builder.knotDates_.back(), std::expm1(0.09) / 3.0, index)};
    const auto spec = builder.Build();
    CurveCalibrationOptions_ curveOptions;
    curveOptions.computeEffJacobianInverse_ = true;
    const auto calibrated = CalibrateSingleCurve(spec, curveOptions);
    const Handle_<StorableCurveCalibrationResult_> curveResult(new StorableCurveCalibrationResult_(calibrated, spec, curveOptions));
    Handle_<StorableRatePricingMarket_> market;
    const Handle_<Storable_> discount(new StorableDiscountCurve_(calibrated.curve_));
    RatePricingMarket_New(Cell_(DateTime_(today, 9, 0)), "USD", {"discount"}, {discount}, {}, {}, {}, 0.0, "", {}, &market);
    Handle_<StorableRateQuoteRiskProvenance_> provenance;
    SingleCurveQuoteRiskProvenance_New(curveResult, "curve-fit", {"curve"}, {"discount"}, market, &provenance);
    const auto curve = NewGSRCurveData("curve", today, "USD", builder.knotDates_,
                                       {0.0, std::log((*calibrated.curve_)(today, builder.knotDates_.back()))}, {}, Matrix_<>(0, 0));
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
    Handle_<StorableGSRCurveQuoteRisk_> bridge;
    GSRCurveQuoteRisk_New(curve, market, provenance, "discount", {}, &bridge);
    GSRCurveQuoteRisk_Get(bridge, "quoteNames", &value);
    ASSERT_EQ(value.Rows(), 1);
    GSRCurveQuoteRisk_Get(bridge, "quoteUnits", &value);
    ASSERT_EQ(Cell::ToString(value(0, 0)), "DECIMAL_QUOTE");
    GSRCurveQuoteRisk_Get(bridge, "logDFQuoteJacobian", &value);
    ASSERT_EQ(value.Rows(), 2);
    ASSERT_EQ(value.Cols(), 1);
    ASSERT_NEAR(Cell::ToDouble(value(0, 0)), 0.0, 1e-12);
    ASSERT_NEAR(Cell::ToDouble(value(1, 0)), -3.0 * std::exp(-0.09), 1e-7);
    GSRSLV_QuoteRisk(initial, quotes, parameters, options, settings, Matrix_<Cell_>(0, 0), bridge, &risk);
    GSRSLVQuoteRiskResult_Get(risk, "curveRiskIncluded", &value);
    ASSERT_TRUE(Cell::ToBool(value(0, 0)));
    GSRSLVQuoteRiskResult_Get(risk, "quoteUnits", &value);
    ASSERT_EQ(value.Rows(), 2);
    ASSERT_EQ(Cell::ToString(value(1, 0)), "DECIMAL_QUOTE");
    GSRSLVQuoteRiskResult_Get(risk, "sensitivities", &value);
    ASSERT_EQ(value.Rows(), 1);
    ASSERT_EQ(value.Cols(), 2);
    ASSERT_GT(std::abs(Cell::ToDouble(value(0, 1))), 1e-4);
    ASSERT_THROW(GSRCurveQuoteRisk_New(curve, market, {}, "discount", {}, &bridge), Exception_);
    ASSERT_THROW(GSRCurveQuoteRisk_Get(bridge, "unknown", &value), Exception_);
    ASSERT_THROW(GSRSLVCalibrationResult_Get(result, "unknown", &value), Exception_);
    ASSERT_THROW(GSRSLV_QuoteRisk({}, quotes, parameters, options, settings, Matrix_<Cell_>(0, 0), {}, &risk), Exception_);
    settings(2, 0) = Cell_("unknown");
    ASSERT_THROW(Calibrate_GSRSLV(initial, quotes, parameters, settings, {}, &result), Exception_);
}

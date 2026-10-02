//
// Created by Codex on 2026/9/27.
//

#include <gtest/gtest.h>

#include <cmath>

#include <dal/storage/globals.hpp>

#include <dal-excel/src/__curve_storable.hpp>
#include <dal-excel/src/__models_test_api.hpp>
#include <dal-excel/src/__script_test_api.hpp>
#include <dal-public/src/curvedata.hpp>
#include <dal-public/src/global.hpp>
#include <dal-public/src/script.hpp>
#include <dal-public/src/value.hpp>

namespace {
    struct ExcelDateScope_ {
        const Dal::Date_ previous_;
        explicit ExcelDateScope_(const Dal::Date_& date) : previous_(Dal::Excel::ScriptTestSetDate(date)) {}
        ~ExcelDateScope_() { Dal::Excel::ScriptTestSetDate(previous_); }
    };
} // namespace

TEST(HybridExcelContractTest, TestTypedFactoriesValueNamedEquities) {
    Dal::InitGlobalData(1);
    Dal::Excel::ScriptTestInitialize(1);
    const auto restore = Dal::XGLOBAL::SetEvaluationDateInScope(Dal::Date_(2026, 9, 27));
    const ExcelDateScope_ restoreExcel(Dal::Date_(2026, 9, 27));
    Dal::Matrix_<> correlations(2, 2, 0.0);
    correlations(0, 0) = correlations(1, 1) = 1.0;
    Dal::Handle_<Dal::ModelData_> correlated;
    Dal::CorrelatedBSModelData_New("correlated", {"EQ[A]", "EQ[B]"}, {100.0, 120.0}, {0.0, 0.0}, {0.0, 0.0}, 0.0, correlations, &correlated);
    Dal::Handle_<Dal::HybridComponentData_> a, b, rate;
    Dal::HybridBSEquityData_New("A", "EQ[A]", "USD", "FA", 100.0, 0.0, 0.0, &a);
    Dal::HybridBSEquityData_New("B", "EQ[B]", "USD", "FB", 120.0, 0.0, 0.0, &b);
    Dal::HybridDeterministicRateData_New("RATE", "USD", 0.0, &rate);
    Dal::Handle_<Dal::HybridCorrelationData_> provider;
    Dal::HybridConstantCorrelationData_New("joint", {"FA", "FB"}, correlations, &provider);
    Dal::Handle_<Dal::ModelData_> hybrid;
    Dal::HybridModelData_New("hybrid", "USD",
                             {Dal::handle_cast<Dal::Storable_>(a), Dal::handle_cast<Dal::Storable_>(b), Dal::handle_cast<Dal::Storable_>(rate)},
                             provider, &hybrid);
    const auto product = Dal::NewScriptProduct("joint", {Dal::Cell_(Dal::Date_(2027, 9, 27))}, {"pay PAYS FIX(EQ[B]) + 2 * FIX(EQ[A])"});
    for (const auto& model : {correlated, hybrid})
        ASSERT_NEAR(Dal::ValueByMonteCarlo(product, model, 16).at("PV"), 320.0, 1.0e-10);
}

TEST(HybridExcelContractTest, TestGsrFactoriesValueDatedBondObservation) {
    Dal::InitGlobalData(1);
    Dal::Excel::ScriptTestInitialize(1);
    const Dal::Date_ today(2026, 9, 28);
    const Dal::Date_ exercise(2027, 9, 28);
    const Dal::Date_ maturity(2028, 9, 28);
    const auto restore = Dal::XGLOBAL::SetEvaluationDateInScope(today);
    const ExcelDateScope_ restoreExcel(today);
    Dal::Handle_<Dal::GSRCurveData_> curve;
    Dal::GSRCurveData_New("curve", today, "USD", {today, exercise, maturity}, {0.0, -0.03, -0.06}, {}, Dal::Matrix_<>(0, 0), &curve);
    Dal::Handle_<Dal::GSRVolData_> vol;
    Dal::GSRVolData_New("vol", {today}, {0.0}, {today}, {1.0}, &vol);
    Dal::Handle_<Dal::ModelData_> model;
    Dal::GSRModelData_New("gsr", curve, vol, &model);
    const auto product = Dal::NewScriptProduct("bond", {Dal::Cell_(exercise)}, {"pay PAYS FIX(IR[USD,DF,2028-09-28])"});
    ASSERT_NEAR(Dal::ValueByMonteCarlo(product, model, 16).at("PV"), std::exp(-0.06), 1e-10);
}

TEST(HybridExcelContractTest, TestMultiFactorGsrFactoriesValueDatedBondAndRejectInvalidCorrelation) {
    Dal::InitGlobalData(1);
    Dal::Excel::ScriptTestInitialize(1);
    const Dal::Date_ today(2026, 10, 2), exercise(2027, 10, 2), maturity(2028, 10, 1);
    const auto restore = Dal::XGLOBAL::SetEvaluationDateInScope(today);
    const ExcelDateScope_ restoreExcel(today);
    Dal::Handle_<Dal::GSRCurveData_> curve;
    Dal::GSRCurveData_New("curve", today, "USD", {today, exercise, maturity}, {0.0, -0.03, -0.06}, {}, Dal::Matrix_<>(0, 0), &curve);
    Dal::Matrix_<> correlation(2, 2, 0.3);
    correlation(0, 0) = correlation(1, 1) = 1.0;
    Dal::Handle_<Dal::MultiFactorGSRVolData_> vol;
    Dal::MultiFactorGSRVolData_New("vol", {"level", "slope"}, {today}, Dal::Matrix_<>(2, 1, 0.0), {today}, Dal::Matrix_<>(2, 1, 1.0), correlation,
                                   &vol);
    Dal::Handle_<Dal::ModelData_> model;
    Dal::MultiFactorGSRModelData_New("rates", curve, vol, &model);
    const auto product = Dal::NewScriptProduct("bond", {Dal::Cell_(exercise)}, {"pay PAYS FIX(IR[USD,DF,2028-10-01])"});
    ASSERT_NEAR(Dal::ValueByMonteCarlo(product, model, 16).at("PV"), std::exp(-0.06), 1e-12);
    correlation(0, 1) = correlation(1, 0) = 1.01;
    ASSERT_THROW(Dal::MultiFactorGSRVolData_New("invalid", {"level", "slope"}, {today}, Dal::Matrix_<>(2, 1, 0.0), {today}, Dal::Matrix_<>(2, 1, 1.0),
                                                correlation, &vol),
                 Dal::Exception_);
}

TEST(HybridExcelContractTest, TestTwoStateBermudanThroughSettingsTable) {
    Dal::InitGlobalData(1);
    Dal::Excel::ScriptTestInitialize(1);
    const auto restore = Dal::XGLOBAL::SetEvaluationDateInScope(Dal::Date_(2026, 9, 27));
    const ExcelDateScope_ restoreExcel(Dal::Date_(2026, 9, 27));
    Dal::Matrix_<> correlations(2, 2, 0.0);
    correlations(0, 0) = correlations(1, 1) = 1.0;
    Dal::Handle_<Dal::ModelData_> model;
    Dal::CorrelatedBSModelData_New("correlated", {"EQ[A]", "EQ[B]"}, {100.0, 120.0}, {0.0, 0.0}, {0.0, 0.0}, 0.0, correlations, &model);
    Dal::Matrix_<Dal::Cell_> productRows(1, 2);
    productRows(0, 0) = "regression_features";
    productRows(0, 1) = "EQ[A];EQ[B]";
    Dal::Handle_<Dal::StorableScriptProductSettings_> settings;
    Dal::ScriptProductSettings_New("states", productRows, &settings);
    Dal::Handle_<Dal::ScriptProductData_> product;
    Dal::Product_NewWithSettings("two-state", {Dal::Cell_(Dal::Date_(2027, 3, 27)), Dal::Cell_(Dal::Date_(2027, 9, 27))},
                                 {"EXERCISE MAX(FIX(EQ[A]) - FIX(EQ[B]), 0)", "EXERCISE MAX(FIX(EQ[B]) - FIX(EQ[A]), 0)"}, settings, &product);
    Dal::Matrix_<Dal::Cell_> simulationRows(2, 2);
    simulationRows(0, 0) = "compiled";
    simulationRows(1, 0) = "enable_aad";
    for (const bool compiled : {false, true})
        for (const bool aad : {false, true}) {
            simulationRows(0, 1) = compiled;
            simulationRows(1, 1) = aad;
            Dal::Handle_<Dal::StorableMonteCarloSettings_> simulation;
            Dal::MonteCarloSettings_New("simulation", simulationRows, &simulation);
            Dal::Matrix_<Dal::Cell_> cells;
            Dal::MonteCarlo_ValueWithSettings(product, model, 128, {}, simulation, &cells);
            bool found = false;
            for (int row = 0; row < cells.Rows(); ++row)
                if (Dal::Cell::ToString(cells(row, 0)) == "PV") {
                    ASSERT_NEAR(Dal::Cell::ToDouble(cells(row, 1)), 20.0, 1e-10);
                    found = true;
                }
            ASSERT_TRUE(found);
        }
}

TEST(HybridExcelContractTest, TestLogDfRateFactoryValuesAHybridModel) {
    Dal::InitGlobalData(1);
    Dal::Excel::ScriptTestInitialize(1);
    const auto restore = Dal::XGLOBAL::SetEvaluationDateInScope(Dal::Date_(2026, 9, 27));
    const ExcelDateScope_ restoreExcel(Dal::Date_(2026, 9, 27));
    Dal::Handle_<Dal::HybridComponentData_> equity, rate;
    Dal::HybridBSEquityData_New("A", "EQ[A]", "USD", "FA", 100.0, 0.0, 0.0, &equity);
    Dal::HybridLogDfRateData_New("RATE", "USD", {0.0, 1.0}, {0.0, -0.08}, "LOG_LINEAR", &rate);
    Dal::Matrix_<> correlations(1, 1, 1.0);
    Dal::Handle_<Dal::HybridCorrelationData_> provider;
    Dal::HybridConstantCorrelationData_New("corr", {"FA"}, correlations, &provider);
    Dal::Handle_<Dal::ModelData_> model;
    Dal::HybridModelData_New("hybrid_curve", "USD", {Dal::handle_cast<Dal::Storable_>(equity), Dal::handle_cast<Dal::Storable_>(rate)}, provider,
                             &model);
    const auto product = Dal::NewScriptProduct("rate", {Dal::Cell_(Dal::Date_(2027, 9, 27))}, {"pay PAYS FIX(EQ[A]) + 25"});
    const auto result = Dal::ValueByMonteCarlo(product, model, 16);
    ASSERT_NEAR(result.at("PV"), 100.0 + 25.0 * std::exp(-0.08), 1e-10);
}

TEST(HybridExcelContractTest, TestLogDfRateSnapshotFactoryUsesCurveDateAxis) {
    const Dal::Date_ today(2026, 9, 27), maturity(2027, 9, 27);
    const auto source = Dal::DiscountZeroRateNew("source", "USD", today, {maturity}, {0.07});
    const Dal::Handle_<Dal::StorableDiscountCurve_> curve(new Dal::StorableDiscountCurve_(source));
    Dal::Handle_<Dal::HybridComponentData_> rate;
    Dal::HybridLogDfRateDataFromCurve_New("RATE", curve, today, {today, maturity}, "LOG_LINEAR", &rate);
    const auto* typed = dynamic_cast<const Dal::HybridLogDfRateData_*>(rate.get());
    ASSERT_NE(typed, nullptr);
    ASSERT_EQ(typed->currency_, "USD");
    ASSERT_NEAR(typed->times_[1], 1.0, 1e-14);
    ASSERT_NEAR(typed->logDF_[1], std::log((*source)(today, maturity)), 1e-14);
}

TEST(HybridExcelContractTest, TestLogDfRateSnapshotFactoryRejectsEmptyCurveValue) {
    const Dal::Date_ today(2026, 9, 27), maturity(2027, 9, 27);
    const Dal::Handle_<Dal::StorableDiscountCurve_> curve(new Dal::StorableDiscountCurve_({}));
    Dal::Handle_<Dal::HybridComponentData_> rate;
    ASSERT_THROW(Dal::HybridLogDfRateDataFromCurve_New("RATE", curve, today, {today, maturity}, "LOG_LINEAR", &rate), Dal::Exception_);
}

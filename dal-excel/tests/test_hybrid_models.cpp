//
// Created by Codex on 2026/9/27.
//

#include <gtest/gtest.h>

#include <dal/storage/globals.hpp>

#include <dal-excel/src/__models_test_api.hpp>
#include <dal-public/src/global.hpp>
#include <dal-public/src/script.hpp>
#include <dal-public/src/value.hpp>

TEST(HybridExcelContractTest, TestTypedFactoriesValueNamedEquities) {
    Dal::InitGlobalData(1);
    const auto restore = Dal::XGLOBAL::SetEvaluationDateInScope(Dal::Date_(2026, 9, 27));
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

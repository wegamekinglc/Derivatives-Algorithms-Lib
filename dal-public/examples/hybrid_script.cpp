//
// Created by Codex on 2026/9/27.
//

#include <iomanip>
#include <iostream>

#include <dal/storage/globals.hpp>

#include <dal-public/src/global.hpp>
#include <dal-public/src/models.hpp>
#include <dal-public/src/script.hpp>
#include <dal-public/src/value.hpp>

int main() {
    using namespace Dal;
    InitGlobalData(1);
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 27));
    HybridSettings_ settings;
    settings.domesticCurrency_ = "USD";
    settings.components_ = {Handle_<HybridComponentData_>(new HybridBSEquityData_("A", "EQ[A]", "USD", "FA", 100.0, 0.20, 0.0)),
                            Handle_<HybridComponentData_>(new HybridBSEquityData_("B", "EQ[B]", "USD", "FB", 120.0, 0.30, 0.0)),
                            Handle_<HybridComponentData_>(new HybridDeterministicRateData_("RATE", "USD", 0.0))};
    Matrix_<> correlations(2, 2, 0.35);
    correlations(0, 0) = correlations(1, 1) = 1.0;
    settings.correlation_ = Handle_<HybridCorrelationData_>(new HybridConstantCorrelationData_("joint", {"FA", "FB"}, correlations));
    const auto model = NewHybridModelData("two-equity", settings);
    const auto product = NewScriptProduct("cross", {Cell_(Date_(2027, 9, 27))}, {"pay PAYS FIX(EQ[A]) * FIX(EQ[B])"});
    MonteCarloSettings_ simulation;
    simulation.useBb_ = true;
    simulation.enableAad_ = true;
    simulation.compiled_ = true;
    const auto result = ValueByMonteCarlo(product, model, 16384, ScriptValuationSettings_(), simulation);
    std::cout << std::fixed << std::setprecision(4) << "PV=" << result.at("PV") << ", d_spot:EQ[A]=" << result.at("d_spot:EQ[A]")
              << ", d_spot:EQ[B]=" << result.at("d_spot:EQ[B]") << '\n';

    ScriptProductSettings_ exerciseSettings;
    exerciseSettings.regressionFeatures_ = {"VAR[a]", "VAR[b]"};
    const auto bermudan =
        NewScriptProduct("exchange-bermudan", {Cell_(Date_(2027, 3, 27)), Cell_(Date_(2027, 9, 27))},
                         {"a = FIX(EQ[A])\nb = FIX(EQ[B])\nEXERCISE MAX(a - b, 0)", "EXERCISE MAX(b - 0.9 * a, 0)"}, exerciseSettings);
    simulation.lsmcTrainingPaths_ = 8192;
    simulation.lsmcValidationPaths_ = 2048;
    const auto exercise = ValueByMonteCarlo(bermudan, model, 32768, ScriptValuationSettings_(), simulation);
    std::cout << "Bermudan PV=" << exercise.at("PV") << ", d_spot:EQ[A]=" << exercise.at("d_spot:EQ[A]")
              << ", d_spot:EQ[B]=" << exercise.at("d_spot:EQ[B]") << '\n';
}

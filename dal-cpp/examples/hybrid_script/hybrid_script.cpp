//
// Created by Codex on 2026/9/27.
//

#include <cmath>
#include <iomanip>
#include <iostream>

#include <dal/platform/platform.hpp>

#include <dal/model/hybriddata.hpp>
#include <dal/platform/consts.hpp>
#include <dal/platform/initall.hpp>
#include <dal/script/event.hpp>
#include <dal/script/settings.hpp>
#include <dal/script/simulation.hpp>
#include <dal/storage/globals.hpp>

int main() {
    using namespace Dal;
    RegisterAll_::Init(1);
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 27));
    HybridSettings_ settings;
    settings.domesticCurrency_ = "USD";
    settings.components_ = {Handle_<HybridComponentData_>(new HybridBSEquityData_("A", "EQ[A]", "USD", "FA", 100.0, 0.20, 0.0)),
                            Handle_<HybridComponentData_>(new HybridBSEquityData_("B", "EQ[B]", "USD", "FB", 120.0, 0.30, 0.0)),
                            Handle_<HybridComponentData_>(new HybridDeterministicRateData_("RATE", "USD", 0.0))};
    Matrix_<> correlations(2, 2, 0.35);
    correlations(0, 0) = correlations(1, 1) = 1.0;
    settings.correlation_ = Handle_<HybridCorrelationData_>(new HybridConstantCorrelationData_("joint", {"FA", "FB"}, correlations));
    const Handle_<ModelData_> model(new HybridModelData_("two-equity", settings));
    const Script::ScriptProductData_ product("cross", {Cell_(Date_(2027, 9, 27))}, {"pay PAYS FIX(EQ[A]) * FIX(EQ[B])"});
    Script::MonteCarloSettings_ simulation;
    simulation.useBb_ = true;
    simulation.enableAad_ = true;
    simulation.compiled_ = true;
    constexpr size_t PATHS = 16384;
    const auto result = Script::MCSimulation<AAD::Number_>(product, model, PATHS, {}, simulation);
    std::cout << std::fixed << std::setprecision(4) << "PV=" << result.aggregated_ / PATHS << ", d_spot:EQ[A]=" << result["spot:EQ[A]"]
              << ", d_spot:EQ[B]=" << result["spot:EQ[B]"] << '\n';

    auto termSettings = settings;
    const double middleTime = (Date_(2027, 3, 27) - Date_(2026, 9, 27)) / DAYS_PER_YEAR;
    termSettings.components_[2] =
        Handle_<HybridComponentData_>(new HybridLogDfRateData_("RATE_CURVE", "USD", {0.0, middleTime, 1.0}, {0.0, -0.015, -0.06}));
    const Handle_<ModelData_> termModel(new HybridModelData_("two-equity-term-rate", termSettings));
    const Script::ScriptProductData_ termProduct("term-rate", {Cell_(Date_(2027, 9, 27))}, {"pay PAYS FIX(EQ[A]) + 25"});
    const auto termResult = Script::MCSimulation<AAD::Number_>(termProduct, termModel, PATHS, {}, simulation);
    std::cout << "Term rate PV=" << termResult.aggregated_ / PATHS << ", d_logdf:USD:2=" << termResult["logdf:USD:2"] << '\n';

    const Vector_<Cell_> exerciseDates = {Cell_(Date_(2027, 3, 27)), Cell_(Date_(2027, 9, 27))};
    const Vector_<String_> exerciseEvents = {"a = FIX(EQ[A])\nb = FIX(EQ[B])\nEXERCISE MAX(a - b + 60, 0)", "EXERCISE MAX(b - a + 60, 0)"};
    Script::ScriptProductSettings_ exerciseSettings;
    exerciseSettings.regressionFeatures_ = {"VAR[a]"};
    const Script::ScriptProductData_ oneState("one-state-bermudan", exerciseDates, exerciseEvents, exerciseSettings);
    exerciseSettings.regressionFeatures_ = {"VAR[a]", "VAR[b]"};
    const Script::ScriptProductData_ bermudan("two-state-bermudan", exerciseDates, exerciseEvents, exerciseSettings);
    simulation.lsmcTrainingPaths_ = 8192;
    simulation.lsmcValidationPaths_ = 2048;
    auto hardSimulation = simulation;
    hardSimulation.enableAad_ = false;
    constexpr size_t EXERCISE_PATHS = 32768;
    const double oneStatePv = Script::MCSimulation<double>(oneState, model, EXERCISE_PATHS, {}, hardSimulation).aggregated_ / EXERCISE_PATHS;
    const auto exercise = Script::MCSimulation<AAD::Number_>(bermudan, model, EXERCISE_PATHS, {}, simulation);
    const double time = (Date_(2027, 3, 27) - Date_(2026, 9, 27)) / 365.0;
    const double width = std::sqrt(0.20 * 0.20 + 0.30 * 0.30 - 2.0 * 0.35 * 0.20 * 0.30) * std::sqrt(time);
    const double d1 = (std::log(120.0 / 100.0) + 0.5 * width * width) / width;
    const auto cdf = [](double value) { return 0.5 * std::erfc(-value / std::sqrt(2.0)); };
    const double reference = 60.0 + 100.0 - 120.0 + 2.0 * (120.0 * cdf(d1) - 100.0 * cdf(d1 - width));
    std::cout << "Bermudan reference=" << reference << ", one-state PV=" << oneStatePv << ", two-state PV=" << exercise.aggregated_ / EXERCISE_PATHS
              << ", d_spot:EQ[A]=" << exercise["spot:EQ[A]"] << ", d_spot:EQ[B]=" << exercise["spot:EQ[B]"] << '\n';
}

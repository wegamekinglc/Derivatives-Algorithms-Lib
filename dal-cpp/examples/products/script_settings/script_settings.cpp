//
// Created by Codex on 2026/9/15.
//

#include <iomanip>
#include <iostream>

#include <dal/platform/platform.hpp>

#include <dal/model/blackscholes.hpp>
#include <dal/model/factory.hpp>
#include <dal/platform/initall.hpp>
#include <dal/script/diagnostics.hpp>
#include <dal/script/event.hpp>
#include <dal/script/preparation.hpp>
#include <dal/script/settings.hpp>
#include <dal/script/simulation.hpp>

int main() {
    using namespace Dal;
    RegisterAll_::Init(1);
    Script::ScriptProductSettings_ contract;
    contract.defaultIndex_ = "EQ[DAL196_TEST]";
    const Script::ScriptProductData_ product("historical-observation", {Cell_("SCALE"), Cell_(Date_(2026, 9, 11)), Cell_(Date_(2026, 9, 22))},
                                             {"2.0", "x = SCALE * FIX(EQ[DAL196_TEST])", "pay PAYS x"}, contract);
    Script::ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = Date_(2026, 9, 12);
    valuation.fixings_ =
        Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_({{"EQ[DAL196_TEST]", {{DateTime_(Date_(2026, 9, 11), 0.0), 80.0}}}}));
    const Handle_<ModelData_> model(new BSModelData_("fixture", 100.0, 0.0, 0.0, 0.0));
    Script::MonteCarloSettings_ simulation;
    simulation.enableAad_ = true;
    simulation.compiled_ = true;
    constexpr size_t PATHS = 4096;
    const auto result = Script::MCSimulation<AAD::Number_>(product, model, PATHS, valuation, simulation);
    const double pv = result.aggregated_ / PATHS;
    REQUIRE(pv == 160.0 && result["SCALE"] == 80.0, "historical price/risk example failed");
    auto diagnosticModel = CreateModel<double>(model);
    const auto prepared = Script::PrepareScript(product, diagnosticModel.get(), valuation, {});
    std::cout << std::fixed << std::setprecision(4) << "PV=" << pv << ", d_SCALE=" << result["SCALE"] << '\n'
              << Script::DescribeScriptProductData(product) << '\n'
              << Script::ExplainPreparedScript(prepared) << '\n';
}

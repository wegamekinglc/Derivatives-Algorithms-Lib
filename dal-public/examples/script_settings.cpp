//
// Created by Codex on 2026/9/15.
//

#include <iomanip>
#include <iostream>

#include <dal-public/src/global.hpp>
#include <dal-public/src/models.hpp>
#include <dal-public/src/script.hpp>
#include <dal-public/src/value.hpp>

int main() {
    using namespace Dal;
    InitGlobalData(1);
    ScriptProductSettings_ contract;
    contract.defaultIndex_ = "EQ[DAL196_TEST]";
    const auto product = NewScriptProduct("historical-observation", {Cell_("SCALE"), Cell_(Date_(2026, 9, 11)), Cell_(Date_(2026, 9, 22))},
                                          {"2.0", "x = SCALE * FIX(EQ[DAL196_TEST])", "pay PAYS x"}, contract);
    ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = Date_(2026, 9, 12);
    valuation.fixings_ =
        Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_({{"EQ[DAL196_TEST]", {{DateTime_(Date_(2026, 9, 11), 0.0), 80.0}}}}));
    const auto model = NewBSModelData("fixture", 100.0, 0.0, 0.0, 0.0);
    MonteCarloSettings_ simulation;
    simulation.enableAad_ = true;
    simulation.compiled_ = true;
    const auto result = ValueByMonteCarlo(product, model, 4096, valuation, simulation);
    REQUIRE(result.at("PV") == 160.0 && result.at("d_SCALE") == 80.0, "historical price/risk example failed");
    std::cout << std::fixed << std::setprecision(4) << "PV=" << result.at("PV") << ", d_SCALE=" << result.at("d_SCALE") << '\n'
              << DescribeScriptProduct(product) << '\n'
              << ExplainScriptValuation(product, model, valuation) << '\n';
}

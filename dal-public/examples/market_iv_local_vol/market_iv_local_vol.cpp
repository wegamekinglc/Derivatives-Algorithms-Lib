#include <dal/platform/platform.hpp>

#include <dal-public/src/global.hpp>
#include <dal-public/src/models.hpp>
#include <dal-public/src/script.hpp>
#include <dal-public/src/value.hpp>

#include "../../../dal-cpp/examples/models/market_iv_local_vol/marketivexample.hpp"

int main() {
    using namespace Dal;
    InitGlobalData(1);
    const auto fit = MarketIvExample::Fit();
    REQUIRE(fit.IsSuccess(), fit.Failure());
    const auto grid = MarketIvExample::Grid(fit.Snapshot());
    REQUIRE(grid.IsSuccess(), grid.Failure());
    const auto model = NewMarketIvLocalVolModelData("market", grid, MarketIvExample::Settings());
    ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = MarketIvExample::Today();
    MonteCarloSettings_ simulation;
    simulation.rsg_ = "mrg32";
    MarketIvExample::Run(
        fit, model,
        [&](const auto& product) {
            return ValueByMonteCarlo(NewScriptProduct(product.Name(), product.Dates(), product.EventTexts()), Handle_<ModelData_>(model), 16384,
                                     valuation, simulation)
                .at("PV");
        },
        [&](const auto& product) { return InspectMarketIvLocalVolExecution(product, *model, valuation, simulation); });
}

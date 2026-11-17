#include <dal/platform/platform.hpp>

#include <dal/platform/initall.hpp>
#include <dal/script/marketivlocalvol.hpp>
#include <dal/script/simulation.hpp>

#include "marketivexample.hpp"

int main() {
    using namespace Dal;
    RegisterAll_::Init(1);
    const auto fit = MarketIvExample::Fit();
    REQUIRE(fit.IsSuccess(), fit.Failure());
    const auto grid = MarketIvExample::Grid(fit.Snapshot());
    REQUIRE(grid.IsSuccess(), grid.Failure());
    const auto model = BuildMarketIvLocalVolModelData("market", grid, MarketIvExample::Settings());
    Script::ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = MarketIvExample::Today();
    Script::MonteCarloSettings_ simulation;
    simulation.rsg_ = "mrg32";
    MarketIvExample::Run(
        fit, model,
        [&](const auto& product) {
            return Script::MCSimulation<double>(product, Handle_<ModelData_>(model), 16384, valuation, simulation).aggregated_ / 16384.;
        },
        [&](const auto& product) { return Script::InspectMarketIvLocalVolExecution(product, *model, valuation, simulation); });
}

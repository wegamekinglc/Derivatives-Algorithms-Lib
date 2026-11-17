#include <dal/platform/platform.hpp>
#include <dal/script/marketivlocalvol.hpp>

#include <dal-public/src/marketivlocalvol.hpp>

namespace Dal {
    Handle_<MarketIvLocalVolModelData_>
    NewMarketIvLocalVolModelData(const String_& name, const MarketIvDupireResult_& grid, const MarketIvLocalVolModelSettings_& settings) {
        return BuildMarketIvLocalVolModelData(name, grid, settings);
    }
    MarketIvLocalVolExecution_ InspectMarketIvLocalVolExecution(const Script::ScriptProductData_& product,
                                                                const MarketIvLocalVolModelData_& model,
                                                                const Script::ScriptValuationSettings_& valuation,
                                                                const Script::MonteCarloSettings_& simulation) {
        return Script::InspectMarketIvLocalVolExecution(product, model, valuation, simulation);
    }
} // namespace Dal

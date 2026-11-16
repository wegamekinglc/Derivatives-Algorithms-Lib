#pragma once

#include <dal/model/marketivlocalvol.hpp>
#include <dal/script/preparation.hpp>

namespace Dal::Script {
    MarketIvLocalVolExecution_ InspectMarketIvLocalVolExecution(const ScriptProductData_& product,
                                                                const MarketIvLocalVolModelData_& model,
                                                                const ScriptValuationSettings_& valuation = {},
                                                                const MonteCarloSettings_& simulation = {});
} // namespace Dal::Script

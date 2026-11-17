#include <cmath>

#include <dal-public/src/global.hpp>
#include <dal-public/src/marketivlocalvol.hpp>
#include <dal-public/src/script.hpp>
#include <dal-public/src/value.hpp>

int main() {
    using namespace Dal;
    InitGlobalData(1);
    const Date_ today(2026, 11, 16), expiry = today.AddDays(365);
    const MarketIvMarket_ market(today, 100., {{expiry, 102., .98}});
    const MarketIvFitRequest_ request{market, {{"atm", expiry, 102., OptionType_("CALL"), MarketIvQuoteType_("DECIMAL_IV"), .2}}, {}};
    const auto grid = CalibrateMarketIvDupire(FitMarketIvSurface(request).Snapshot(), {{80., 100., 120.}, {.1, 1.}, {}});
    MarketIvLocalVolModelSettings_ settings;
    settings.index_ = "EQ[A]";
    settings.currency_ = "USD";
    settings.factor_ = "W_EQ";
    settings.horizon_ = 1.25;
    const auto model = NewMarketIvLocalVolModelData("installed", grid, settings);
    const auto product = NewScriptProduct("df", {Cell_(expiry)}, {"pay PAYS 1"});
    ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = today;
    const auto report = InspectMarketIvLocalVolExecution(*product, *model, valuation);
    const double pv = ValueByMonteCarlo(product, Handle_<ModelData_>(model), 16, valuation).at("PV");
    return std::abs(pv - .98) <= 2e-12 && report.modelIdentity_ == model->ModelIdentity() && report.perPathMarketQueries_ == 0 ? 0 : 1;
}

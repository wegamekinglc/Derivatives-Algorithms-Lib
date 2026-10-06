//
// Created by Codex on 2026/10/6.
//

#include <array>
#include <typeindex>

#include <dal/indice/detail/snapshoterror.hpp>
#include <dal/model/factory.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/portfoliopreparation.hpp>

namespace Dal::Script::Detail {
    namespace {
        bool HasFactorySharingProof(const ModelData_& model) {
            // These factory families generate paths independently of private script state.
            static const std::array<std::type_index, 6> supported{typeid(BSModelData_),  typeid(CorrelatedBSModelData_),   typeid(HybridModelData_),
                                                                  typeid(GSRModelData_), typeid(MultiFactorGSRModelData_), typeid(GSRSLVModelData_)};
            return std::find(supported.begin(), supported.end(), std::type_index(typeid(model))) != supported.end();
        }

        PlannedScript_ PlanTrade(const ScriptPortfolioData_& portfolio,
                                 size_t trade,
                                 const ScriptValuationSettings_& valuation,
                                 const MonteCarloSettings_& simulation) {
            try {
                return PlanScript(*portfolio.Products()[trade], CreateModel<double>(portfolio.Models()[portfolio.ModelOwners()[trade]]), valuation,
                                  simulation);
            } catch (const std::exception& error) {
                THROW2("InvalidPortfolioPreparation: trade=" + portfolio.TradeIds()[trade] + "; cause=" + String_(error.what()), ScriptError_);
            }
        }

        Vector_<FixingRequest_> HistoricalDependencies(const Vector_<PlannedScript_>& plans) {
            Vector_<FixingRequest_> requests;
            for (const auto& plan : plans)
                for (const auto& request : plan.View().Plan().Requests())
                    if (request.historical_)
                        requests.push_back({request.key_.canonicalIndex_, request.key_.fixingTime_});
            return requests;
        }

        String_ SnapshotTradeContext(const std::exception& error, const ScriptPortfolioData_& portfolio, const Vector_<PlannedScript_>& plans) {
            const auto* failure = dynamic_cast<const Dal::Detail::SnapshotFixingError_*>(&error);
            if (!failure)
                return {};
            for (size_t trade = 0; trade < plans.size(); ++trade) {
                for (const auto& request : plans[trade].View().Plan().Requests()) {
                    if (!request.historical_)
                        continue;
                    for (const auto& dependency : HistoricalFixingDependencies({{request.key_.canonicalIndex_, request.key_.fixingTime_}}))
                        if (SameFixingRequest(dependency, failure->Request()))
                            return "; trade=" + portfolio.TradeIds()[trade];
                }
            }
            return {};
        }

        Handle_<MarketFixingSnapshot_>
        CaptureHistory(const ScriptPortfolioData_& portfolio, const Vector_<PlannedScript_>& plans, const ScriptValuationSettings_& valuation) {
            if (valuation.fixings_)
                return valuation.fixings_;
            try {
                return SnapshotGlobalFixings(HistoricalDependencies(plans));
            } catch (const std::exception& error) {
                THROW2("InvalidPortfolioFixingSnapshot: source=GlobalSnapshot" + SnapshotTradeContext(error, portfolio, plans) +
                           "; cause=" + String_(error.what()),
                       ScriptError_);
            }
        }

        PreparedScript_ CompleteTrade(PlannedScript_&& plan, const Handle_<MarketFixingSnapshot_>& snapshot, const String_& tradeId) {
            try {
                return CompleteScriptPreparation(std::move(plan), snapshot);
            } catch (const std::exception& error) {
                THROW2("InvalidPortfolioPreparation: trade=" + tradeId + "; cause=" + String_(error.what()), ScriptError_);
            }
        }
    } // namespace

    Vector_<PortfolioScenarioGroup_> PlanPortfolioScenarioGroups(const ScriptPortfolioData_& portfolio, const Vector_<PlannedScript_>& plans) {
        REQUIRE2(plans.size() == portfolio.TradeIds().size(), "InvalidPortfolioPlanning: trade extent changed", ScriptError_);
        Vector_<PreparedPortfolioTradeView_> views;
        views.reserve(plans.size());
        for (size_t trade = 0; trade < plans.size(); ++trade) {
            const size_t owner = static_cast<size_t>(portfolio.ModelOwners()[trade]);
            const auto& model = plans[trade].Model();
            views.push_back({&plans[trade].View(), owner, model.SimDim(), model.NumFactors(), model.NumeraireIsDeterministic(),
                             HasFactorySharingProof(*portfolio.Models()[owner])});
        }
        return GroupPlannedPortfolio(views);
    }

    PreparedPortfolio_ PrepareScriptPortfolio(const Handle_<ScriptPortfolioData_>& portfolio,
                                              int numPaths,
                                              const ScriptValuationSettings_& valuation,
                                              const MonteCarloSettings_& simulation,
                                              const PortfolioPreparationAdmission_& beforeHistory) {
        const auto sealed = portfolio;
        REQUIRE2(sealed, "InvalidScriptPortfolio: portfolio must not be null; field=portfolio", ScriptError_);
        REQUIRE2(numPaths > 0, "InvalidPortfolioPaths: path count must be positive; field=numPaths", ScriptError_);
        const auto settings = ResolveValuationSettings(valuation);
        const auto execution = simulation;
        const auto admission = beforeHistory;
        Vector_<PlannedScript_> plans;
        Vector_<PreparedPortfolioTradeView_> views;
        plans.reserve(sealed->TradeIds().size());
        views.reserve(sealed->TradeIds().size());
        for (size_t trade = 0; trade < sealed->TradeIds().size(); ++trade) {
            plans.push_back(PlanTrade(*sealed, trade, settings, execution));
            const size_t owner = static_cast<size_t>(sealed->ModelOwners()[trade]);
            const auto& model = plans.back().Model();
            views.push_back({nullptr, owner, model.SimDim(), model.NumFactors(), model.NumeraireIsDeterministic(),
                             HasFactorySharingProof(*sealed->Models()[owner])});
        }
        if (admission)
            admission(plans);
        const auto snapshot = CaptureHistory(*sealed, plans, settings);
        Vector_<PreparedScript_> trades;
        trades.reserve(plans.size());
        for (size_t trade = 0; trade < plans.size(); ++trade) {
            trades.push_back(CompleteTrade(std::move(plans[trade]), snapshot, sealed->TradeIds()[trade]));
            views[trade].prepared_ = &trades.back();
        }
        auto groups = GroupPreparedPortfolio(views);
        return PreparedPortfolio_(sealed, numPaths, settings, snapshot, std::move(trades), std::move(groups));
    }
} // namespace Dal::Script::Detail

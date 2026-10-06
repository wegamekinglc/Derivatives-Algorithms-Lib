//
// Created by Codex on 2026/10/6.
//

#pragma once

#include <functional>

#include <dal/script/plannedpreparation.hpp>
#include <dal/script/portfolio.hpp>
#include <dal/script/portfoliogroups.hpp>

namespace Dal::Script::Detail {
    class PreparedPortfolio_;
    using PortfolioPreparationAdmission_ = std::function<void(const Vector_<PlannedScript_>&)>;

    [[nodiscard]] PreparedPortfolio_ PrepareScriptPortfolio(const Handle_<ScriptPortfolioData_>& portfolio,
                                                            int numPaths,
                                                            const ScriptValuationSettings_& valuation = {},
                                                            const MonteCarloSettings_& simulation = {},
                                                            const PortfolioPreparationAdmission_& beforeHistory = {});

    class PreparedPortfolio_ {
        Handle_<ScriptPortfolioData_> portfolio_;
        int pathCount_;
        ScriptValuationSettings_ valuation_;
        Handle_<MarketFixingSnapshot_> fixings_;
        Vector_<PreparedScript_> trades_;
        Vector_<PortfolioScenarioGroup_> groups_;

        PreparedPortfolio_(Handle_<ScriptPortfolioData_> portfolio,
                           int numPaths,
                           ScriptValuationSettings_ valuation,
                           Handle_<MarketFixingSnapshot_> fixings,
                           Vector_<PreparedScript_> trades,
                           Vector_<PortfolioScenarioGroup_> groups)
            : portfolio_(std::move(portfolio)), pathCount_(numPaths), valuation_(std::move(valuation)), fixings_(std::move(fixings)),
              trades_(std::move(trades)), groups_(std::move(groups)) {}
        friend PreparedPortfolio_ PrepareScriptPortfolio(const Handle_<ScriptPortfolioData_>&,
                                                         int,
                                                         const ScriptValuationSettings_&,
                                                         const MonteCarloSettings_&,
                                                         const PortfolioPreparationAdmission_&);

    public:
        [[nodiscard]] const Handle_<ScriptPortfolioData_>& Portfolio() const { return portfolio_; }
        [[nodiscard]] int PathCount() const { return pathCount_; }
        [[nodiscard]] const ScriptValuationSettings_& Valuation() const { return valuation_; }
        [[nodiscard]] const Handle_<MarketFixingSnapshot_>& Fixings() const { return fixings_; }
        [[nodiscard]] const Vector_<PreparedScript_>& Trades() const { return trades_; }
        [[nodiscard]] const Vector_<PortfolioScenarioGroup_>& Groups() const { return groups_; }
    };
} // namespace Dal::Script::Detail

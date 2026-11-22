#pragma once

#include <functional>

#include <dal/model/marketivlocalvol.hpp>
#include <dal/script/preparation.hpp>

namespace Dal::Script {
    struct MarketIvScriptRefitBudget_ {
        size_t maxSelectedQuotes_ = 32, maxScenarios_ = 65, maxTotalPaths_ = 1U << 26;
        size_t maxModelRowEvaluations_ = 320000000, workspaceBudgetBytes_ = 1U << 30;
    };

    struct MarketIvScriptRefitRequest_ {
        MarketIvCalibrationSnapshot_ snapshot_;
        MarketIvDupireRequest_ grid_;
        MarketIvLocalVolModelSettings_ model_;
        Handle_<ScriptProductData_> product_;
        ScriptValuationSettings_ valuation_;
        MonteCarloSettings_ simulation_ = [] {
            MonteCarloSettings_ settings;
            settings.rsg_ = "mrg32";
            settings.normalPrecision_ = "Precise";
            return settings;
        }();
        size_t blocks_ = 16, pathsPerBlock_ = 1024;
        Vector_<String_> quoteIds_;
        Vector_<> steps_;
        MarketIvScriptRefitBudget_ budget_;
    };

    struct MarketIvScriptRefitRow_ {
        size_t quoteIndex_;
        String_ id_;
        MarketIvQuoteType_ quoteType_;
        String_ inputUnit_;
        double step_, forward_, discount_;
    };

    struct MarketIvScriptRefitWork_ {
        // Counts cover the base and every selected +/- scenario. The base snapshot is already fitted.
        size_t refits_, grids_, scenarios_, pathsPerScenario_, totalPaths_, modelRowEvaluationLimit_, reservedWorkspaceBytes_;
    };

    struct MarketIvScriptRefitPlan_ {
        const MarketIvScriptRefitRequest_ request_;
        const Vector_<MarketIvScriptRefitRow_> rows_;
        const MarketIvScriptRefitWork_ work_;
        const Handle_<MarketIvLocalVolModelData_> baseModel_;
        const std::shared_ptr<const PreparedScript_> prepared_;
    };

    [[nodiscard]] MarketIvScriptRefitPlan_ PlanMarketIvScriptRefit(const MarketIvScriptRefitRequest_& input);

    struct MarketIvScriptModelPair_ {
        Handle_<MarketIvLocalVolModelData_> plus_, minus_;
    };

    struct MarketIvScriptBlockPvs_ {
        Vector_<> blockMeans_;
        double mean_ = 0.;
    };

    struct MarketIvScriptPairedPvs_ {
        MarketIvScriptBlockPvs_ plus_, minus_;
        double slope_ = 0., standardError_ = 0.;
    };

    struct MarketIvScriptSamplingResult_ {
        MarketIvScriptBlockPvs_ base_;
        Vector_<MarketIvScriptPairedPvs_> rows_; // Same order and quote units as the plan.
        Vector_<> simulationTimes_;              // Includes the initial time. Each interval consumes one normal.
        String_ factor_;
        bool useBb_ = false;
        size_t pathsPerBlock_ = 0;
        // Counts cover only the fixed-model sampling call.
        size_t refits_ = 0, grids_ = 0, scenarios_ = 0, totalPaths_ = 0, peakWorkspaceBytes_ = 0;
    };

    [[nodiscard]] MarketIvScriptSamplingResult_
    SampleMarketIvScriptModelPairs(const MarketIvScriptRefitPlan_& plan,
                                   const std::function<MarketIvScriptModelPair_(const MarketIvScriptRefitRow_&)>& models);

    MarketIvLocalVolExecution_ InspectMarketIvLocalVolExecution(const ScriptProductData_& product,
                                                                const MarketIvLocalVolModelData_& model,
                                                                const ScriptValuationSettings_& valuation = {},
                                                                const MonteCarloSettings_& simulation = {});
} // namespace Dal::Script

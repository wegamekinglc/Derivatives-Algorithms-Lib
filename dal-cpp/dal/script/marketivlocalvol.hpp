#pragma once

#include <functional>

#include <dal/script/marketivscriptrefit.hpp>

namespace Dal::Script {
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

    private:
        friend MarketIvScriptRefitPlan_ PlanMarketIvScriptRefit(const MarketIvScriptRefitRequest_&);
        MarketIvScriptRefitPlan_(MarketIvScriptRefitRequest_ request,
                                 Vector_<MarketIvScriptRefitRow_> rows,
                                 MarketIvScriptRefitWork_ work,
                                 Handle_<MarketIvLocalVolModelData_> baseModel,
                                 std::shared_ptr<const PreparedScript_> prepared)
            : request_(std::move(request)), rows_(std::move(rows)), work_(work), baseModel_(std::move(baseModel)), prepared_(std::move(prepared)) {}
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

    struct MarketIvScriptRefitSide_ {
        Handle_<MarketIvLocalVolModelData_> model_;
        String_ fitStatus_, failureStage_, failure_;
        int selectedStart_ = -1;
        bool regimeChanged_ = false;
    };

    struct MarketIvScriptRefitPair_ {
        MarketIvScriptRefitRow_ row_;
        MarketIvScriptRefitSide_ plus_, minus_;
        std::optional<double> symmetricBumpSlope_;
        [[nodiscard]] String_ Status() const;
    };

    [[nodiscard]] MarketIvScriptRefitPair_ BuildMarketIvScriptRefitPair(const MarketIvScriptRefitPlan_& plan, size_t rowIndex);

    MarketIvLocalVolExecution_ InspectMarketIvLocalVolExecution(const ScriptProductData_& product,
                                                                const MarketIvLocalVolModelData_& model,
                                                                const ScriptValuationSettings_& valuation = {},
                                                                const MonteCarloSettings_& simulation = {});
} // namespace Dal::Script

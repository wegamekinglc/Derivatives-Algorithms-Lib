#pragma once

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
    };

    [[nodiscard]] MarketIvScriptRefitPlan_ PlanMarketIvScriptRefit(const MarketIvScriptRefitRequest_& input);

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

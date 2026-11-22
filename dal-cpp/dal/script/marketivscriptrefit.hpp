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

    struct MarketIvScriptRiskSide_ {
        std::optional<double> pv_;
        std::optional<int> selectedStart_;
        std::optional<bool> regimeChanged_;
        String_ fitStatus_, failureStage_, failure_, modelIdentity_;
    };

    struct MarketIvScriptRiskRow_ {
        MarketIvScriptRefitRow_ quote_;
        String_ currency_, status_;
        double basePv_ = 0.;
        MarketIvScriptRiskSide_ plus_, minus_;
        std::optional<double> slope_, standardError_;
    };

    struct MarketIvScriptRefitExecution_ {
        size_t blocks_, pathsPerBlock_, refits_, grids_, scenarios_, totalPaths_, samplingPeakWorkspaceBytes_;
    };

    struct MarketIvScriptRefitRiskResult_ {
        Vector_<MarketIvScriptRiskRow_> rows_;
        Vector_<> baseBlockMeans_;
        MarketIvScriptRefitExecution_ execution_{};
    };

    [[nodiscard]] MarketIvScriptRefitRiskResult_ EvaluateMarketIvScriptRefitRisk(const MarketIvScriptRefitRequest_& request);
} // namespace Dal::Script

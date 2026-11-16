#pragma once

#include <limits>
#include <optional>

#include <dal/model/surface/marketivsurface.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal {
    struct MarketIvLocalVolModelSettings_ {
        String_ index_, currency_, factor_;
        double horizon_ = std::numeric_limits<double>::quiet_NaN();
        double maxStep_ = 1. / 52.;
        size_t maxInternalSteps_ = 100000, maxSimulationDimension_ = 100000;
        size_t numericPayloadBudgetBytes_ = 67108864, workspaceBudgetBytes_ = 268435456;
    };

    class MarketIvLocalVolError_ : public ScriptError_ {
    public:
        const String_ code_, stage_, field_, expected_, actual_;
        MarketIvLocalVolError_(const String_& code, const String_& stage, const String_& field, const String_& expected, const String_& actual);
    };

    struct MarketIvLocalVolCarryTerm_ {
        size_t ordinal_ = 0;
        std::optional<Date_> date_;
        double time_ = 0., forward_ = 0., discount_ = 0., logForwardOverSpot_ = 0., logDiscount_ = 0.;
        String_ origin_;
    };

    struct MarketIvLocalVolWarning_ {
        String_ code_, field_, message_;
    };

    struct MarketIvLocalVolConstructionExecution_ {
        String_ schema_ = "dal.market-iv-local-vol-construction/1";
        size_t plannedNumericPayloadBytes_ = 0, numericPayloadBytes_ = 0, plannedWorkspaceBytes_ = 0, peakWorkspaceBytes_ = 0;
    };

    void ValidateMarketIvLocalVolModelSettings(const MarketIvLocalVolModelSettings_& settings);
} // namespace Dal

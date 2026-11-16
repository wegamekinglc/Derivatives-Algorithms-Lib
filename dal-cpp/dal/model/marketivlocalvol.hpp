#pragma once

#include <limits>
#include <optional>

#include <dal/model/base.hpp>
#include <dal/model/surface/marketivsurface.hpp>
#include <dal/utilities/exceptions.hpp>
#include <dal/utilities/numerics.hpp>

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

    struct MarketIvLocalVolCapabilities_ {
        String_ schema_ = "dal.market-iv-local-vol-capabilities/1";
        bool passivePrice_ = true, discountFactors_ = true, brownianBridge_ = true, compiled_ = true, tree_ = true;
        bool aad_ = false, nativeRisk_ = false, quoteRisk_ = false, curvature_ = false, segmented_ = false, exercise_ = false, archive_ = false;
    };

    struct MarketIvLocalVolModelSummary_ {
        String_ schema_ = "dal.market-iv-local-vol-model-summary/1", sourceIdentity_, modelIdentity_;
        std::optional<Date_> evaluationDate_;
        double spot_ = 0.;
        String_ carryMode_, index_, currency_, factor_;
        double horizon_ = 0., maxStep_ = 0.;
        size_t gridRows_ = 0, gridColumns_ = 0;
        double gridFirstTime_ = 0., gridLastTime_ = 0.;
        String_ fitStatus_;
        bool underidentified_ = false, nearTie_ = false;
        String_ timeBasis_ = "ACT/365", stepScheme_ = "LOG_EULER_LEFT_VOL/1", localVolScheme_ = "VOL_LINEAR_LOG_SPOT_TIME_FLAT/1",
                carryScheme_ = "LOG_F_LOG_D_LINEAR_LAST_SLOPE/1", timelineScheme_ = "UNION_CARRY_GRID_EVENTS/1";
    };

    struct MarketIvLocalVolExecution_ {
        String_ schema_ = "dal.market-iv-local-vol-execution/1", sourceIdentity_, modelIdentity_, requestIdentity_;
        std::optional<Date_> evaluationDate_;
        double requestedMaxStep_ = 0., resolvedMaxStep_ = 0., requiredTime_ = 0.;
        size_t carryKnots_ = 0, gridTimeKnots_ = 0, anchorCount_ = 0, internalSteps_ = 0, simulationDimension_ = 0, workers_ = 1;
        size_t sharedNumericBytes_ = 0, perWorkerNumericBytes_ = 0;
        size_t plannedNumericPayloadBytes_ = 0, numericPayloadBytes_ = 0, plannedWorkspaceBytes_ = 0, peakWorkspaceBytes_ = 0;
        size_t preparedColumns_ = 0, preparedMarketQueries_ = 0, perPathMarketQueries_ = 0;
    };

    namespace MarketIvLocalVolDetail {
        class Source_;
    }
    namespace AAD {
        class MarketIvLocalVolModel_;
    }

    class MarketIvLocalVolModelData_ final : public ModelData_ {
        std::shared_ptr<const MarketIvLocalVolDetail::Source_> source_;
        MarketIvLocalVolModelData_(const String_& name, std::shared_ptr<const MarketIvLocalVolDetail::Source_> source);
        [[nodiscard]] std::unique_ptr<ModelData_> MutantModel(const String_* newName, const Slide_* slide) const override;
        friend Handle_<MarketIvLocalVolModelData_>
        BuildMarketIvLocalVolModelData(const String_&, const MarketIvDupireResult_&, const MarketIvLocalVolModelSettings_&);
        friend class AAD::MarketIvLocalVolModel_;

    public:
        [[nodiscard]] String_ SourceIdentity() const;
        [[nodiscard]] String_ ModelIdentity() const;
        [[nodiscard]] MarketIvLocalVolModelSettings_ Configuration() const;
        [[nodiscard]] MarketIvLocalVolModelSummary_ Summary() const;
        [[nodiscard]] Vector_<MarketIvLocalVolCarryTerm_> CarryTerms() const;
        [[nodiscard]] MarketIvLocalVolCapabilities_ Capabilities() const { return {}; }
        [[nodiscard]] Vector_<MarketIvLocalVolWarning_> Warnings() const;
        [[nodiscard]] MarketIvLocalVolConstructionExecution_ ConstructionExecution() const;
        void Write(Archive::Store_&) const override;
        using ModelData_::MutantModel;
    };

    Handle_<MarketIvLocalVolModelData_>
    BuildMarketIvLocalVolModelData(const String_& name, const MarketIvDupireResult_& grid, const MarketIvLocalVolModelSettings_& settings);
    void RequireMarketIvLocalVolPassive(const ModelData_& model, const String_& field = "/risk/model");
    void ValidateMarketIvLocalVolModelSettings(const MarketIvLocalVolModelSettings_& settings);
} // namespace Dal

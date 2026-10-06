//
// Created by Codex on 2026/10/4.
//

#pragma once

#include <map>
#include <optional>

#include <dal/math/cell.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/script/settings.hpp>
#include <dal/time/date.hpp>
#include <dal/time/datetime.hpp>

namespace Dal::Script {
    struct SimResults_;

    struct RiskCoordinate_ {
        String_ id_;
        String_ label_;
        String_ family_;
        size_t ordinal_ = 0;
        double value_ = 0.0;
        String_ nativeUnit_;
        std::optional<String_> physicalUnit_;
        double reportScale_ = 1.0;
    };

    struct RiskRequest_ {
        std::optional<Vector_<String_>> inputs_;
        std::optional<Vector_<String_>> outputs_;
        std::optional<Vector_<>> reportFactors_;
        std::optional<size_t> numericPayloadBudgetBytes_;
    };

    struct RiskObservationSnapshot_ {
        String_ index_;
        DateTime_ fixingTime_;
        bool historical_ = false;
        std::optional<double> value_;
    };

    struct RiskExecutionSnapshot_ {
        int pathsPerReplicate_ = 0;
        int pricingReplicates_ = 1;
        bool allExpired_ = false;
        MonteCarloSettings_ simulation_;
        ScriptProductSettings_ productSettings_;
        Vector_<Cell_> productDates_;
        Vector_<String_> productEvents_;
        String_ todayFixingPolicy_;
        String_ fixingSource_;
        String_ modelSnapshotJson_;
        Vector_<RiskObservationSnapshot_> observations_;
    };

    struct RiskResultProvenance_ {
        String_ method_;
        String_ engine_ = "native";
        String_ normalization_ = "mean";
        String_ calibration_ = "fixed";
        String_ modelType_;
        std::optional<Date_> evaluationDate_;
        std::optional<RiskExecutionSnapshot_> execution_;
    };

    class RiskResult_ {
        Vector_<String_> outputIds_{"payoff"};
        Vector_<> values_;
        Matrix_<> jacobian_;
        Vector_<RiskCoordinate_> inputAxis_;
        Vector_<RiskCoordinate_> completeInputAxis_;
        RiskResultProvenance_ provenance_;

        RiskResult_(double value,
                    Matrix_<>&& jacobian,
                    Vector_<RiskCoordinate_>&& inputAxis,
                    const Vector_<RiskCoordinate_>& completeAxis,
                    const RiskResultProvenance_& provenance);
        friend RiskResult_
        ProjectMonteCarloRiskResult(const SimResults_&, int, const Vector_<RiskCoordinate_>&, const RiskRequest_&, const RiskResultProvenance_&);
        friend RiskResult_ ProjectMonteCarloObjectiveRiskResult(
            const SimResults_&, int, const Vector_<RiskCoordinate_>&, const RiskRequest_&, const RiskResultProvenance_&, const char*);

    public:
        [[nodiscard]] const Vector_<String_>& OutputIds() const { return outputIds_; }
        [[nodiscard]] const Vector_<>& Values() const { return values_; }
        [[nodiscard]] const Matrix_<>& Jacobian() const { return jacobian_; }
        [[nodiscard]] const Vector_<RiskCoordinate_>& InputAxis() const { return inputAxis_; }
        [[nodiscard]] const Vector_<RiskCoordinate_>& CompleteInputAxis() const { return completeInputAxis_; }
        [[nodiscard]] const RiskResultProvenance_& Provenance() const { return provenance_; }
        [[nodiscard]] Matrix_<> ReportedJacobian() const;
        [[nodiscard]] std::map<String_, double> LegacyValues() const;
    };

    [[nodiscard]] size_t RiskResultPayloadBytes(size_t outputs, size_t inputs);
    [[nodiscard]] RiskRequest_ PlanScalarRiskRequest(const Vector_<RiskCoordinate_>& completeAxis, const RiskRequest_& request, bool enableAad);
    [[nodiscard]] RiskResult_ ProjectMonteCarloRiskResult(const SimResults_& source,
                                                          int paths,
                                                          const Vector_<RiskCoordinate_>& completeAxis,
                                                          const RiskRequest_& request,
                                                          const RiskResultProvenance_& provenance);
    [[nodiscard]] RiskResult_ ProjectMonteCarloObjectiveRiskResult(const SimResults_& source,
                                                                   int paths,
                                                                   const Vector_<RiskCoordinate_>& completeAxis,
                                                                   const RiskRequest_& request,
                                                                   const RiskResultProvenance_& provenance,
                                                                   const char* objective);
} // namespace Dal::Script

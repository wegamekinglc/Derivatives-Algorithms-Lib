//
// Created by Codex on 2026-10-06.
//

#pragma once

#include <dal/platform/platform.hpp>

#include <dal/script/event.hpp>
#include <dal/script/riskresults.hpp>

namespace Dal::Script {
    struct RiskOutputCoordinate_ {
        String_ id_;
        String_ label_;
        size_t slot_ = 0;
    };

    struct WeightedRiskRequest_ {
        RiskRequest_ selection_;
        std::optional<Vector_<double>> weights_;
    };

    class WeightedRiskPlan_ {
        Vector_<RiskOutputCoordinate_> outputAxis_;
        Vector_<RiskOutputCoordinate_> completeOutputAxis_;
        Vector_<RiskCoordinate_> completeInputAxis_;
        Vector_<double> weights_;
        RiskRequest_ inputRequest_;
        Date_ evaluationDate_;
        bool enableAad_ = true;
        size_t numericPayloadBytes_ = 0;

        WeightedRiskPlan_() = default;
        friend WeightedRiskPlan_
        PlanWeightedRiskRequest(const ScriptProduct_&, const Vector_<RiskCoordinate_>&, const Date_&, const WeightedRiskRequest_&, bool);

    public:
        [[nodiscard]] const Vector_<RiskOutputCoordinate_>& OutputAxis() const { return outputAxis_; }
        [[nodiscard]] const Vector_<RiskOutputCoordinate_>& CompleteOutputAxis() const { return completeOutputAxis_; }
        [[nodiscard]] const Vector_<RiskCoordinate_>& CompleteInputAxis() const { return completeInputAxis_; }
        [[nodiscard]] const Vector_<double>& Weights() const { return weights_; }
        [[nodiscard]] const RiskRequest_& InputRequest() const { return inputRequest_; }
        [[nodiscard]] const Date_& EvaluationDate() const { return evaluationDate_; }
        [[nodiscard]] bool EnableAad() const { return enableAad_; }
        [[nodiscard]] size_t NumericPayloadBytes() const { return numericPayloadBytes_; }
    };

    [[nodiscard]] Vector_<RiskOutputCoordinate_> ScriptRiskOutputAxis(const ScriptProduct_& indexedProduct);
    [[nodiscard]] size_t WeightedRiskResultPayloadBytes(size_t components, size_t inputs);
    [[nodiscard]] WeightedRiskPlan_ PlanWeightedRiskRequest(const ScriptProduct_& indexedProduct,
                                                            const Vector_<RiskCoordinate_>& completeInputAxis,
                                                            const Date_& evaluationDate,
                                                            const WeightedRiskRequest_& request,
                                                            bool enableAad);
    void ValidateWeightedRiskPreparedAxes(const WeightedRiskPlan_& plan,
                                          const ScriptProduct_& preparedProduct,
                                          const Vector_<RiskCoordinate_>& preparedInputAxis);
} // namespace Dal::Script

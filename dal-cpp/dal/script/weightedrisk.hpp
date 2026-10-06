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

    class WeightedRiskResult_ {
        RiskResult_ objective_;
        Vector_<RiskOutputCoordinate_> outputAxis_;
        Vector_<double> weights_;
        Vector_<double> componentMeans_;

        WeightedRiskResult_(RiskResult_&& objective, const WeightedRiskPlan_& plan, Vector_<double>&& componentMeans);
        friend WeightedRiskResult_
        ProjectWeightedMonteCarloRiskResult(const SimResults_&, const Vector_<double>&, int, const WeightedRiskPlan_&, const RiskResultProvenance_&);

    public:
        [[nodiscard]] double WeightedValue() const { return objective_.Values()[0]; }
        [[nodiscard]] const Vector_<double>& ComponentMeans() const { return componentMeans_; }
        [[nodiscard]] const Vector_<double>& Weights() const { return weights_; }
        [[nodiscard]] const Vector_<RiskOutputCoordinate_>& OutputAxis() const { return outputAxis_; }
        [[nodiscard]] const Matrix_<>& Jacobian() const { return objective_.Jacobian(); }
        [[nodiscard]] Matrix_<> ReportedJacobian() const { return objective_.ReportedJacobian(); }
        [[nodiscard]] const Vector_<RiskCoordinate_>& InputAxis() const { return objective_.InputAxis(); }
        [[nodiscard]] const Vector_<RiskCoordinate_>& CompleteInputAxis() const { return objective_.CompleteInputAxis(); }
        [[nodiscard]] const RiskResultProvenance_& Provenance() const { return objective_.Provenance(); }
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
    [[nodiscard]] WeightedRiskResult_ ProjectWeightedMonteCarloRiskResult(const SimResults_& source,
                                                                          const Vector_<double>& componentSums,
                                                                          int paths,
                                                                          const WeightedRiskPlan_& plan,
                                                                          const RiskResultProvenance_& provenance);
} // namespace Dal::Script

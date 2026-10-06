//
// Created by Codex on 2026/10/06.
//

#pragma once

#include <dal/script/weightedrisk.hpp>

namespace Dal::Script {
    namespace Detail {
        struct AADBlockReplayResult_;
    } // namespace Detail

    struct JacobianRiskRequest_ {
        RiskRequest_ selection_;
        size_t maxBlockWidth_ = 1;
        std::optional<size_t> recordingCapacityBudgetBytes_;
        std::optional<size_t> scratchCapacityBudgetBytes_;
    };

    struct JacobianRiskExecution_ {
        Vector_<size_t> actualWidths_;
        size_t replayAttempts_ = 0;
        size_t executedPaths_ = 0;
        size_t peakRecordingBytes_ = 0;
        size_t peakScratchBytes_ = 0;
    };

    class JacobianRiskPlan_ {
        Vector_<RiskOutputCoordinate_> outputAxis_;
        Vector_<RiskOutputCoordinate_> completeOutputAxis_;
        Vector_<RiskCoordinate_> inputAxis_;
        Vector_<RiskCoordinate_> completeInputAxis_;
        Vector_<size_t> inputPositions_;
        JacobianRiskRequest_ request_;
        Date_ evaluationDate_;
        bool enableAad_ = true;
        size_t numericPayloadBytes_ = 0;

        JacobianRiskPlan_() = default;
        friend JacobianRiskPlan_
        PlanJacobianRiskRequest(const ScriptProduct_&, const Vector_<RiskCoordinate_>&, const Date_&, const JacobianRiskRequest_&, bool);

    public:
        [[nodiscard]] const Vector_<RiskOutputCoordinate_>& OutputAxis() const { return outputAxis_; }
        [[nodiscard]] const Vector_<RiskOutputCoordinate_>& CompleteOutputAxis() const { return completeOutputAxis_; }
        [[nodiscard]] const Vector_<RiskCoordinate_>& InputAxis() const { return inputAxis_; }
        [[nodiscard]] const Vector_<RiskCoordinate_>& CompleteInputAxis() const { return completeInputAxis_; }
        [[nodiscard]] const Vector_<size_t>& InputPositions() const { return inputPositions_; }
        [[nodiscard]] const JacobianRiskRequest_& Request() const { return request_; }
        [[nodiscard]] const Date_& EvaluationDate() const { return evaluationDate_; }
        [[nodiscard]] bool EnableAad() const { return enableAad_; }
        [[nodiscard]] size_t NumericPayloadBytes() const { return numericPayloadBytes_; }
    };

    class JacobianRiskResult_ {
        Vector_<> values_;
        Matrix_<> jacobian_;
        Vector_<RiskOutputCoordinate_> outputAxis_;
        Vector_<RiskOutputCoordinate_> completeOutputAxis_;
        Vector_<RiskCoordinate_> inputAxis_;
        Vector_<RiskCoordinate_> completeInputAxis_;
        RiskResultProvenance_ provenance_;
        JacobianRiskExecution_ execution_;

        JacobianRiskResult_(Detail::AADBlockReplayResult_&& source, const JacobianRiskPlan_& plan, const RiskResultProvenance_& provenance);
        friend JacobianRiskResult_
        ProjectJacobianRiskResult(Detail::AADBlockReplayResult_&&, int, const JacobianRiskPlan_&, const RiskResultProvenance_&);

    public:
        [[nodiscard]] const Vector_<>& Values() const { return values_; }
        [[nodiscard]] const Matrix_<>& Jacobian() const { return jacobian_; }
        [[nodiscard]] Matrix_<> ReportedJacobian() const;
        [[nodiscard]] const Vector_<RiskOutputCoordinate_>& OutputAxis() const { return outputAxis_; }
        [[nodiscard]] const Vector_<RiskOutputCoordinate_>& CompleteOutputAxis() const { return completeOutputAxis_; }
        [[nodiscard]] const Vector_<RiskCoordinate_>& InputAxis() const { return inputAxis_; }
        [[nodiscard]] const Vector_<RiskCoordinate_>& CompleteInputAxis() const { return completeInputAxis_; }
        [[nodiscard]] const RiskResultProvenance_& Provenance() const { return provenance_; }
        [[nodiscard]] const JacobianRiskExecution_& Execution() const { return execution_; }
    };

    [[nodiscard]] JacobianRiskPlan_ PlanJacobianRiskRequest(const ScriptProduct_& indexedProduct,
                                                            const Vector_<RiskCoordinate_>& completeInputAxis,
                                                            const Date_& evaluationDate,
                                                            const JacobianRiskRequest_& request,
                                                            bool enableAad);
    void ValidateJacobianRiskPreparedAxes(const JacobianRiskPlan_& plan,
                                          const ScriptProduct_& preparedProduct,
                                          const Vector_<RiskCoordinate_>& preparedInputAxis);
    [[nodiscard]] JacobianRiskResult_ ProjectJacobianRiskResult(Detail::AADBlockReplayResult_&& source,
                                                                int paths,
                                                                const JacobianRiskPlan_& plan,
                                                                const RiskResultProvenance_& provenance);
} // namespace Dal::Script

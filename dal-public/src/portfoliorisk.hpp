//
// Created by Codex on 2026/10/6.
//

#pragma once

#include <utility>

#include <dal/platform/platform.hpp>

#include <dal/math/aad/sample.hpp>
#include <dal/script/portfolio.hpp>
#include <dal/script/weightedrisk.hpp>

namespace Dal {
    namespace Detail {
        class PortfolioRiskPlan_;
        class PortfolioWeightedPlan_;
        class PortfolioJacobianPlan_;
        struct PortfolioWeightedReplayResult_;
        struct PortfolioJacobianReplayResult_;
    } // namespace Detail

    struct PortfolioWeightedRiskRequest_ {
        Script::RiskRequest_ selection_;
        std::optional<Vector_<double>> weights_;
        std::optional<size_t> recordingCapacityBudgetBytes_;
        std::optional<size_t> scratchCapacityBudgetBytes_;
    };

    struct PortfolioJacobianRiskRequest_ {
        Script::RiskRequest_ selection_;
        size_t maxBlockWidth_ = 1;
        std::optional<size_t> recordingCapacityBudgetBytes_;
        std::optional<size_t> scratchCapacityBudgetBytes_;
    };

    struct PortfolioRiskProvenance_ {
        String_ method_;
        String_ engine_;
        String_ normalization_ = "mean";
        String_ calibration_ = "fixed";
        Date_ evaluationDate_;
        Vector_<String_> tradeIds_;
        Vector_<int> modelOwners_;
        Vector_<Script::RiskResultProvenance_> trades_;
    };

    struct PortfolioGroupExecution_ {
        size_t modelOwner_ = 0;
        Vector_<size_t> tradePositions_;
        size_t randomDimension_ = 0;
        size_t factors_ = 0;
        bool deterministicNumeraire_ = false;
        Vector_<Date_> sampleDates_;
        Vector_<double> timeLine_;
        Vector_<AAD::SampleDef_> sampleDefinitions_;
        Script::MonteCarloSettings_ simulation_;
        size_t generatedScenarios_ = 0;
        size_t evaluatorCalls_ = 0;
        size_t suffixReversals_ = 0;
        size_t prefixReversals_ = 0;
        Vector_<size_t> actualWidths_;
        size_t replayAttempts_ = 0;
    };

    struct PortfolioRiskExecution_ {
        Vector_<PortfolioGroupExecution_> groups_;
        size_t peakRecordingBytes_ = 0;
        size_t peakScratchBytes_ = 0;
        size_t requestedMaxBlockWidth_ = 0;
    };

    class PortfolioRiskResultMetadata_ {
        Vector_<Script::RiskOutputCoordinate_> outputAxis_;
        Vector_<Script::RiskOutputCoordinate_> completeOutputAxis_;
        Vector_<Script::RiskCoordinate_> inputAxis_;
        Vector_<Script::RiskCoordinate_> completeInputAxis_;
        PortfolioRiskProvenance_ provenance_;
        PortfolioRiskExecution_ execution_;

    protected:
        PortfolioRiskResultMetadata_(const Detail::PortfolioRiskPlan_& plan, PortfolioRiskProvenance_ provenance, PortfolioRiskExecution_ execution);
        ~PortfolioRiskResultMetadata_() = default;

    public:
        [[nodiscard]] const Vector_<Script::RiskOutputCoordinate_>& OutputAxis() const { return outputAxis_; }
        [[nodiscard]] const Vector_<Script::RiskOutputCoordinate_>& CompleteOutputAxis() const { return completeOutputAxis_; }
        [[nodiscard]] const Vector_<Script::RiskCoordinate_>& InputAxis() const { return inputAxis_; }
        [[nodiscard]] const Vector_<Script::RiskCoordinate_>& CompleteInputAxis() const { return completeInputAxis_; }
        [[nodiscard]] const PortfolioRiskProvenance_& Provenance() const { return provenance_; }
        [[nodiscard]] const PortfolioRiskExecution_& Execution() const { return execution_; }
    };

    class PortfolioWeightedRiskResult_ : public PortfolioRiskResultMetadata_ {
        double weightedValue_;
        Vector_<double> componentMeans_;
        Vector_<double> gradient_;
        Vector_<double> weights_;

        PortfolioWeightedRiskResult_(const Detail::PortfolioWeightedPlan_& plan,
                                     Detail::PortfolioWeightedReplayResult_&& source,
                                     PortfolioRiskProvenance_ provenance,
                                     PortfolioRiskExecution_ execution);
        friend PortfolioWeightedRiskResult_ ValuePortfolioByMonteCarloWithWeightedRisk(const Handle_<Script::ScriptPortfolioData_>&,
                                                                                       int,
                                                                                       const PortfolioWeightedRiskRequest_&,
                                                                                       const Script::ScriptValuationSettings_&,
                                                                                       const Script::MonteCarloSettings_&);

    public:
        [[nodiscard]] double WeightedValue() const { return weightedValue_; }
        [[nodiscard]] const Vector_<double>& ComponentMeans() const { return componentMeans_; }
        [[nodiscard]] const Vector_<double>& Weights() const { return weights_; }
        [[nodiscard]] Matrix_<> Jacobian() const;
        [[nodiscard]] Matrix_<> ReportedJacobian() const;
    };

    class PortfolioJacobianRiskResult_ : public PortfolioRiskResultMetadata_ {
        Vector_<double> values_;
        Matrix_<double> jacobian_;

        PortfolioJacobianRiskResult_(const Detail::PortfolioJacobianPlan_& plan,
                                     Detail::PortfolioJacobianReplayResult_&& source,
                                     PortfolioRiskProvenance_ provenance,
                                     PortfolioRiskExecution_ execution);
        friend PortfolioJacobianRiskResult_ ValuePortfolioByMonteCarloWithJacobianRisk(const Handle_<Script::ScriptPortfolioData_>&,
                                                                                       int,
                                                                                       const PortfolioJacobianRiskRequest_&,
                                                                                       const Script::ScriptValuationSettings_&,
                                                                                       const Script::MonteCarloSettings_&);

    public:
        [[nodiscard]] const Vector_<double>& Values() const { return values_; }
        [[nodiscard]] Matrix_<double> Jacobian() const { return jacobian_; }
        [[nodiscard]] Matrix_<double> ReportedJacobian() const;
    };

    class PortfolioRiskAxes_ {
        Vector_<Script::RiskCoordinate_> inputAxis_;
        Vector_<Script::RiskOutputCoordinate_> outputAxis_;
        Vector_<Vector_<size_t>> tradeInputPositions_;
        Vector_<size_t> outputTrades_;

    public:
        PortfolioRiskAxes_(Vector_<Script::RiskCoordinate_> inputs,
                           Vector_<Script::RiskOutputCoordinate_> outputs,
                           Vector_<Vector_<size_t>> inputPositions,
                           Vector_<size_t> outputTrades)
            : inputAxis_(std::move(inputs)), outputAxis_(std::move(outputs)), tradeInputPositions_(std::move(inputPositions)),
              outputTrades_(std::move(outputTrades)) {}
        [[nodiscard]] const Vector_<Script::RiskCoordinate_>& InputAxis() const { return inputAxis_; }
        [[nodiscard]] const Vector_<Script::RiskOutputCoordinate_>& OutputAxis() const { return outputAxis_; }
        [[nodiscard]] const Vector_<Vector_<size_t>>& TradeInputPositions() const { return tradeInputPositions_; }
        [[nodiscard]] const Vector_<size_t>& OutputTrades() const { return outputTrades_; }
    };

    [[nodiscard]] PortfolioRiskAxes_ ScriptPortfolioRiskAxes(const Handle_<Script::ScriptPortfolioData_>& portfolio);
} // namespace Dal

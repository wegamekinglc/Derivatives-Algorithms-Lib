//
// Created by Codex on 2026/10/6.
//

#pragma once

#include <utility>

#include <dal-public/src/portfoliorisk.hpp>
#include <dal/script/portfoliobatch.hpp>

namespace Dal::Detail {
    class PortfolioRiskPlan_ {
        Handle_<Script::ScriptPortfolioData_> portfolio_;
        PortfolioRiskAxes_ axes_;
        Vector_<Script::Detail::PortfolioBatchOutput_> outputs_;
        Vector_<size_t> inputPositions_;
        Vector_<Script::RiskCoordinate_> inputAxis_;
        Script::RiskRequest_ inputRequest_;
        size_t numericPayloadBytes_;
        bool enableAad_;

    public:
        PortfolioRiskPlan_(Handle_<Script::ScriptPortfolioData_> portfolio,
                           PortfolioRiskAxes_ axes,
                           Vector_<Script::Detail::PortfolioBatchOutput_> outputs,
                           Vector_<size_t> positions,
                           Vector_<Script::RiskCoordinate_> inputs,
                           Script::RiskRequest_ inputRequest,
                           size_t numericPayload,
                           bool enableAad)
            : portfolio_(std::move(portfolio)), axes_(std::move(axes)), outputs_(std::move(outputs)), inputPositions_(std::move(positions)),
              inputAxis_(std::move(inputs)), inputRequest_(std::move(inputRequest)), numericPayloadBytes_(numericPayload), enableAad_(enableAad) {}
        [[nodiscard]] const Handle_<Script::ScriptPortfolioData_>& Portfolio() const { return portfolio_; }
        [[nodiscard]] const PortfolioRiskAxes_& Axes() const { return axes_; }
        [[nodiscard]] const Vector_<Script::Detail::PortfolioBatchOutput_>& Outputs() const { return outputs_; }
        [[nodiscard]] const Vector_<size_t>& InputPositions() const { return inputPositions_; }
        [[nodiscard]] const Vector_<Script::RiskCoordinate_>& InputAxis() const { return inputAxis_; }
        [[nodiscard]] const Script::RiskRequest_& InputRequest() const { return inputRequest_; }
        [[nodiscard]] size_t NumericPayloadBytes() const { return numericPayloadBytes_; }
        [[nodiscard]] bool EnableAad() const { return enableAad_; }
    };

    class PortfolioWeightedPlan_ : public PortfolioRiskPlan_ {
    public:
        explicit PortfolioWeightedPlan_(PortfolioRiskPlan_ plan) : PortfolioRiskPlan_(std::move(plan)) {}
    };

    class PortfolioJacobianPlan_ : public PortfolioRiskPlan_ {
        size_t maxBlockWidth_;

    public:
        PortfolioJacobianPlan_(PortfolioRiskPlan_ plan, size_t width) : PortfolioRiskPlan_(std::move(plan)), maxBlockWidth_(width) {}
        [[nodiscard]] size_t MaxBlockWidth() const { return maxBlockWidth_; }
    };

    [[nodiscard]] PortfolioWeightedPlan_
    PlanPortfolioWeightedRequest(const Handle_<Script::ScriptPortfolioData_>& portfolio, const Script::WeightedRiskRequest_& request, bool enableAad);

    [[nodiscard]] PortfolioJacobianPlan_ PlanPortfolioJacobianRequest(const Handle_<Script::ScriptPortfolioData_>& portfolio,
                                                                      const PortfolioJacobianRiskRequest_& request,
                                                                      bool enableAad);
} // namespace Dal::Detail

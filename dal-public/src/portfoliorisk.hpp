//
// Created by Codex on 2026/10/6.
//

#pragma once

#include <utility>

#include <dal/script/portfolio.hpp>
#include <dal/script/weightedrisk.hpp>

namespace Dal {
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

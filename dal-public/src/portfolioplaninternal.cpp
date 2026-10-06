//
// Created by Codex on 2026/10/6.
//

#include <cmath>
#include <limits>
#include <map>

#include <dal-public/src/portfolioplaninternal.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/riskaxisinternal.hpp>

namespace Dal::Detail {
    namespace {
        Vector_<String_> DefaultOutputs(const Script::ScriptPortfolioData_& portfolio) {
            Vector_<String_> outputs;
            outputs.reserve(portfolio.TradeIds().size());
            for (size_t trade = 0; trade < portfolio.TradeIds().size(); ++trade)
                outputs.push_back("trade:" + String_(std::to_string(trade)) + ":payoff");
            return outputs;
        }

        Vector_<Script::Detail::PortfolioBatchOutput_>
        SelectOutputs(const Script::ScriptPortfolioData_& portfolio, const PortfolioRiskAxes_& axes, const Script::WeightedRiskRequest_& request) {
            const auto ids = request.selection_.outputs_ ? *request.selection_.outputs_ : DefaultOutputs(portfolio);
            const auto selected = Script::Detail::SelectRiskOutputs(axes.OutputAxis(), ids, "InvalidPortfolioWeightedRequest");
            REQUIRE2(!request.weights_ || request.weights_->size() == selected.size(),
                     "InvalidPortfolioWeightedRequest: weight count must match selected outputs; field=weights", ScriptError_);
            std::map<String_, size_t> trades;
            for (size_t output = 0; output < axes.OutputAxis().size(); ++output)
                trades.emplace(axes.OutputAxis()[output].id_, axes.OutputTrades()[output]);
            Vector_<Script::Detail::PortfolioBatchOutput_> outputs;
            outputs.reserve(selected.size());
            for (size_t output = 0; output < selected.size(); ++output) {
                const auto weight = request.weights_ ? (*request.weights_)[output] : 1.0;
                REQUIRE2(std::isfinite(weight), "InvalidPortfolioWeightedRequest: weight must be finite; output=" + selected[output].id_,
                         ScriptError_);
                outputs.push_back({trades.at(selected[output].id_), selected[output], weight});
            }
            return outputs;
        }

        Vector_<Script::RiskCoordinate_>
        SelectInputs(const PortfolioRiskAxes_& axes, const Vector_<size_t>& positions, const Script::RiskRequest_& request) {
            Vector_<Script::RiskCoordinate_> inputs;
            inputs.reserve(positions.size());
            for (size_t column = 0; column < positions.size(); ++column) {
                auto input = axes.InputAxis()[positions[column]];
                input.reportScale_ = request.reportFactors_ ? (*request.reportFactors_)[column] : 1.0;
                inputs.push_back(std::move(input));
            }
            return inputs;
        }
    } // namespace

    PortfolioWeightedPlan_ PlanPortfolioWeightedRequest(const Handle_<Script::ScriptPortfolioData_>& portfolio,
                                                        const Script::WeightedRiskRequest_& requested,
                                                        bool enableAad) {
        const auto sealed = portfolio;
        const auto request = requested;
        auto axes = ScriptPortfolioRiskAxes(sealed);
        auto outputs = SelectOutputs(*sealed, axes, request);
        auto inputs = request.selection_;
        inputs.outputs_.reset();
        REQUIRE2(enableAad || !inputs.inputs_ || inputs.inputs_->empty(),
                 "InvalidPortfolioWeightedRequest: passive execution cannot select risk inputs; field=inputs", ScriptError_);
        if (!enableAad)
            inputs.inputs_ = Vector_<String_>();
        auto positions = Script::Detail::RiskInputPositions(axes.InputAxis(), inputs);
        Script::Detail::ValidateRiskInputRequest(inputs, positions.size());
        REQUIRE2(positions.size() <= static_cast<size_t>(std::numeric_limits<int>::max()),
                 "InvalidPortfolioWeightedRequest: selected input extent exceeds matrix columns", ScriptError_);
        const auto payload = Script::WeightedRiskResultPayloadBytes(outputs.size(), positions.size());
        REQUIRE2(!inputs.numericPayloadBudgetBytes_ || payload <= *inputs.numericPayloadBudgetBytes_,
                 "Portfolio numeric result payload budget exceeded [required=" + String_(std::to_string(payload)) +
                     ", limit=" + String_(std::to_string(inputs.numericPayloadBudgetBytes_.value_or(0))) + "]",
                 ScriptError_);
        auto selected = SelectInputs(axes, positions, inputs);
        return {sealed, std::move(axes), std::move(outputs), std::move(positions), std::move(selected), std::move(inputs), payload, enableAad};
    }
} // namespace Dal::Detail

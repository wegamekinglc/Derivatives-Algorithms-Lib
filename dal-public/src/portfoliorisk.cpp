//
// Created by Codex on 2026/10/6.
//

#include <cmath>

#include <dal-public/src/portfoliorisk.hpp>
#include <dal-public/src/riskvalueinternal.hpp>
#include <dal/model/factory.hpp>
#include <dal/platform/platform.hpp>

namespace Dal {
    namespace {
        String_ PortfolioTradePrefix(size_t position) { return "trade:" + String_(std::to_string(position)) + ":"; }

        Vector_<Script::RiskCoordinate_> IndexTradeAxes(const Script::ScriptPortfolioData_& portfolio,
                                                        size_t trade,
                                                        const AAD::Model_<double>& model,
                                                        Vector_<Script::RiskOutputCoordinate_>* outputs,
                                                        Vector_<size_t>* outputTrades) {
            try {
                auto product = portfolio.Products()[trade]->Product();
                REQUIRE2(product.HasPayoff(), "product must contain a PAYS or EXERCISE payoff", ScriptError_);
                product.IndexVariables();
                auto inputs = Detail::ScriptRiskInputAxis(model, product);
                for (const auto& input : inputs)
                    REQUIRE2(std::isfinite(input.value_), "input coordinate must be finite; input=" + input.id_, ScriptError_);
                for (auto output : Script::ScriptRiskOutputAxis(product)) {
                    output.id_ = PortfolioTradePrefix(trade) + output.id_;
                    outputs->push_back(std::move(output));
                    outputTrades->push_back(trade);
                }
                return inputs;
            } catch (const std::exception& error) {
                THROW2("InvalidScriptPortfolio: trade axes cannot be indexed; field=products; trade=" + portfolio.TradeIds()[trade] +
                           "; cause=" + String_(error.what()),
                       ScriptError_);
            }
        }
    } // namespace

    PortfolioRiskAxes_ ScriptPortfolioRiskAxes(const Handle_<Script::ScriptPortfolioData_>& portfolio) {
        REQUIRE2(portfolio, "InvalidScriptPortfolio: portfolio must not be null; field=portfolio", ScriptError_);
        Vector_<std::unique_ptr<AAD::Model_<double>>> models;
        for (const auto& data : portfolio->Models())
            models.push_back(CreateModel<double>(data));
        Vector_<Vector_<Script::RiskCoordinate_>> localInputs;
        Vector_<Script::RiskOutputCoordinate_> outputs;
        Vector_<size_t> outputTrades;
        for (size_t trade = 0; trade < portfolio->TradeIds().size(); ++trade)
            localInputs.push_back(IndexTradeAxes(*portfolio, trade, *models[portfolio->ModelOwners()[trade]], &outputs, &outputTrades));
        Vector_<Script::RiskCoordinate_> inputs;
        Vector_<size_t> modelOffsets;
        for (size_t owner = 0; owner < models.size(); ++owner) {
            modelOffsets.push_back(inputs.size());
            const auto first = std::find(portfolio->ModelOwners().begin(), portfolio->ModelOwners().end(), static_cast<int>(owner));
            const size_t trade = static_cast<size_t>(first - portfolio->ModelOwners().begin());
            for (size_t ordinal = 0; ordinal < models[owner]->NumParams(); ++ordinal) {
                auto input = localInputs[trade][ordinal];
                input.id_ = "model:" + String_(std::to_string(owner)) + ":parameter:" + String_(std::to_string(ordinal));
                inputs.push_back(std::move(input));
            }
        }
        Vector_<Vector_<size_t>> inputPositions;
        for (size_t trade = 0; trade < localInputs.size(); ++trade) {
            const size_t owner = static_cast<size_t>(portfolio->ModelOwners()[trade]);
            const size_t parameters = models[owner]->NumParams();
            Vector_<size_t> mapping;
            mapping.reserve(localInputs[trade].size());
            for (size_t ordinal = 0; ordinal < parameters; ++ordinal)
                mapping.push_back(modelOffsets[owner] + ordinal);
            for (size_t ordinal = parameters; ordinal < localInputs[trade].size(); ++ordinal) {
                auto input = localInputs[trade][ordinal];
                input.id_ = PortfolioTradePrefix(trade) + "constant:" + String_(std::to_string(input.ordinal_));
                mapping.push_back(inputs.size());
                inputs.push_back(std::move(input));
            }
            inputPositions.push_back(std::move(mapping));
        }
        return {std::move(inputs), std::move(outputs), std::move(inputPositions), std::move(outputTrades)};
    }
} // namespace Dal

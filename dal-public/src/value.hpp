//
// Created by wegam on 2022/11/20.
//

#pragma once

#include <optional>

#include <dal-public/src/portfoliorisk.hpp>
#include <dal/script/jacobianrisk.hpp>
#include <dal/script/riskresults.hpp>
#include <dal/script/simulation.hpp>

namespace Dal {

    using AAD::Model_;
    using Script::MonteCarloSettings_;
    using Script::ScriptProductData_;
    using Script::ScriptValuationSettings_;

    [[nodiscard]] MonteCarloSettings_ DefaultRiskMonteCarloSettings();
    [[nodiscard]] PortfolioWeightedRiskResult_
    ValuePortfolioByMonteCarloWithWeightedRisk(const Handle_<Script::ScriptPortfolioData_>& portfolio,
                                               int numPath,
                                               const PortfolioWeightedRiskRequest_& request = {},
                                               const ScriptValuationSettings_& valuation = {},
                                               const MonteCarloSettings_& simulation = DefaultRiskMonteCarloSettings());
    [[nodiscard]] PortfolioJacobianRiskResult_
    ValuePortfolioByMonteCarloWithJacobianRisk(const Handle_<Script::ScriptPortfolioData_>& portfolio,
                                               int numPath,
                                               const PortfolioJacobianRiskRequest_& request = {},
                                               const ScriptValuationSettings_& valuation = {},
                                               const MonteCarloSettings_& simulation = DefaultRiskMonteCarloSettings());
    [[nodiscard]] Script::RiskResult_ ValueByMonteCarloWithRisk(const Handle_<ScriptProductData_>& product,
                                                                const Handle_<ModelData_>& modelData,
                                                                int numPath,
                                                                const Script::RiskRequest_& request = {},
                                                                const ScriptValuationSettings_& valuation = {},
                                                                const MonteCarloSettings_& simulation = DefaultRiskMonteCarloSettings());
    [[nodiscard]] Script::WeightedRiskResult_
    ValueByMonteCarloWithWeightedRisk(const Handle_<ScriptProductData_>& product,
                                      const Handle_<ModelData_>& modelData,
                                      int numPath,
                                      const Script::WeightedRiskRequest_& request = {},
                                      const ScriptValuationSettings_& valuation = {},
                                      const MonteCarloSettings_& simulation = DefaultRiskMonteCarloSettings());

    [[nodiscard]] Script::JacobianRiskResult_
    ValueByMonteCarloWithJacobianRisk(const Handle_<ScriptProductData_>& product,
                                      const Handle_<ModelData_>& modelData,
                                      int numPath,
                                      const Script::JacobianRiskRequest_& request = {},
                                      const ScriptValuationSettings_& valuation = {},
                                      const MonteCarloSettings_& simulation = DefaultRiskMonteCarloSettings());

    String_ ExplainScriptValuation(const Handle_<ScriptProductData_>& product,
                                   const Handle_<ModelData_>& modelData,
                                   const ScriptValuationSettings_& valuation = ScriptValuationSettings_());

    //  Simulation diagnostic (dal.script-simulation/1): runs the full valuation with
    //  numPath paths and reports the per-exercise-event regression and exercise rates
    String_ ExplainScriptSimulation(const Handle_<ScriptProductData_>& product,
                                    const Handle_<ModelData_>& modelData,
                                    int numPath,
                                    const ScriptValuationSettings_& valuation = ScriptValuationSettings_(),
                                    const MonteCarloSettings_& simulation = MonteCarloSettings_());

    std::map<String_, double> ValueByMonteCarlo(const Handle_<ScriptProductData_>& product,
                                                const Handle_<ModelData_>& modelData,
                                                int numPath,
                                                const ScriptValuationSettings_& valuation,
                                                const MonteCarloSettings_& simulation = MonteCarloSettings_());

    //  compiled: opt into the script flat-stream evaluator.
    std::map<String_, double> ValueByMonteCarlo(const Handle_<ScriptProductData_>& product,
                                                const Handle_<ModelData_>& modelData,
                                                int numPath,
                                                const String_& rsg = "sobol",
                                                bool useBb = false,
                                                bool enableAad = false,
                                                double smooth = Script::DEFAULT_SMOOTH,
                                                std::optional<bool> compiled = std::nullopt);

} // namespace Dal

//
// Created by wegam on 2022/11/20.
//

#include <cmath>
#include <dal/model/factory.hpp>
#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/script/diagnostics.hpp>
#include <dal/storage/globals.hpp>

#include <dal-public/src/value.hpp>
#include <dal-public/src/valuevalidation.hpp>

namespace Dal {
    using AAD::Model_;
    using Script::SimResults_;

    namespace {
        ScriptValuationSettings_ CheckedValuation(const Handle_<ScriptProductData_>& product,
                                                  const Handle_<ModelData_>& modelData,
                                                  const ScriptValuationSettings_& valuation) {
            Detail::CheckScriptValuationInputs(product, modelData);
            return Script::ResolveValuationSettings(valuation);
        }
    } // namespace

    String_ ExplainScriptValuation(const Handle_<ScriptProductData_>& product,
                                   const Handle_<ModelData_>& modelData,
                                   const ScriptValuationSettings_& valuation) {
        XGLOBAL::ValuationMutationGuard_ valuationGuard;
        const auto& productCopy = product;
        const auto& modelCopy = modelData;
        const auto settings = CheckedValuation(productCopy, modelCopy, valuation);
        auto model = CreateModel<double>(modelCopy);
        const auto prepared = Script::PrepareScript(*productCopy, model.get(), settings, MonteCarloSettings_());
        return Script::ExplainPreparedScript(prepared);
    }

    String_ ExplainScriptSimulation(const Handle_<ScriptProductData_>& product,
                                    const Handle_<ModelData_>& modelData,
                                    int numPath,
                                    const ScriptValuationSettings_& valuation,
                                    const MonteCarloSettings_& simulation) {
        XGLOBAL::ValuationMutationGuard_ valuationGuard;
        //  Validate the signed argument before the size_t conversion: a negative count
        //  would otherwise wrap to a near-maximum allocation request
        REQUIRE2(numPath > 0,
                 "InvalidPathCount: number of Monte Carlo paths must be positive; numPath=" + String_(std::to_string(numPath)) +
                     "; expected a positive integer",
                 ScriptError_);
        const auto& productCopy = product;
        const auto& modelCopy = modelData;
        const auto settings = CheckedValuation(productCopy, modelCopy, valuation);
        return Script::ExplainScriptSimulation(*productCopy, modelCopy, static_cast<size_t>(numPath), settings, simulation);
    }

    std::map<String_, double> ValueByMonteCarlo(const Handle_<ScriptProductData_>& product,
                                                const Handle_<ModelData_>& modelData,
                                                int nPaths,
                                                const String_& rsg,
                                                bool useBb,
                                                bool enableAad,
                                                double smooth,
                                                std::optional<bool> compiled) {
        return ValueByMonteCarlo(product, modelData, nPaths, ScriptValuationSettings_(),
                                 MonteCarloSettings_{rsg, useBb, enableAad, smooth, compiled});
    }

    std::map<String_, double> ValueByMonteCarlo(const Handle_<ScriptProductData_>& product,
                                                const Handle_<ModelData_>& modelData,
                                                int nPaths,
                                                const ScriptValuationSettings_& valuation,
                                                const MonteCarloSettings_& simulation) {
        XGLOBAL::ValuationMutationGuard_ valuationGuard;
        REQUIRE2(nPaths > 0,
                 "InvalidPathCount: number of Monte Carlo paths must be positive; numPath=" + String_(std::to_string(nPaths)) +
                     "; expected a positive integer",
                 ScriptError_);
        const auto& productCopy = product;
        const auto& modelCopy = modelData;
        //  Snapshot: date-capture callbacks inside CheckedValuation can mutate the caller's settings object
        const auto execution = simulation;
        const auto settings = CheckedValuation(productCopy, modelCopy, valuation);
        Script::ValidateSimulationSettings(execution);
        const size_t numPaths = static_cast<size_t>(nPaths);
        const auto results = execution.enableAad_ ? Script::MCSimulation<AAD::Number_>(*productCopy, modelCopy, numPaths, settings, execution)
                                                  : Script::MCSimulation<double>(*productCopy, modelCopy, numPaths, settings, execution);
        const double pv = results.aggregated_ / static_cast<double>(numPaths);
        REQUIRE2(std::isfinite(pv), "InvalidPayoff: non-finite Monte Carlo mean; output=PV", ScriptError_);
        std::map<String_, double> res;
        res["PV"] = pv;
        if (execution.enableAad_)
            for (const auto& n : results.names_) {
                const double risk = results[n];
                REQUIRE2(std::isfinite(risk), "InvalidRisk: non-finite Monte Carlo sensitivity; output=PV; input=" + n, ScriptError_);
                res["d_" + n] = risk;
            }
        return res;
    }
} // namespace Dal

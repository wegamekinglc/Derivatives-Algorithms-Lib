//
// Created by wegam on 2022/11/20.
//

#include <dal/model/factory.hpp>
#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/script/diagnostics.hpp>
#include <dal/storage/globals.hpp>

#include <dal-public/src/value.hpp>

namespace Dal {
    using AAD::Model_;
    using Script::SimResults_;

    namespace {
        const std::set<String_> MODEL_STORE = {"BSModelData_", "DupireModelData_"};

        ScriptValuationSettings_ CheckedValuation(const Handle_<ScriptProductData_>& product,
                                                  const Handle_<ModelData_>& modelData,
                                                  const ScriptValuationSettings_& valuation) {
            REQUIRE2(product, "InvalidSetting: product=null; expected a non-null product", ScriptError_);
            REQUIRE2(modelData, "InvalidSetting: modelData=null; expected a non-null model", ScriptError_);
            const auto modelType = modelData->Type();
            REQUIRE2(MODEL_STORE.find(modelType) != MODEL_STORE.end(),
                     "InvalidSetting: modelData.Type=" + modelType + "; expected BSModelData_ or DupireModelData_", ScriptError_);
            return Script::ResolveValuationSettings(valuation);
        }
    } // namespace

    String_ ExplainScriptValuation(const Handle_<ScriptProductData_>& product,
                                   const Handle_<ModelData_>& modelData,
                                   const ScriptValuationSettings_& valuation) {
        XGLOBAL::ValuationMutationGuard_ valuationGuard;
        const auto productCopy = product;
        const auto modelCopy = modelData;
        const auto settings = CheckedValuation(productCopy, modelCopy, valuation);
        auto model = CreateModel<double>(modelCopy);
        const auto prepared = Script::PrepareScript(*productCopy, model.get(), settings, MonteCarloSettings_());
        return Script::ExplainPreparedScript(prepared);
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
        const auto productCopy = product;
        const auto modelCopy = modelData;
        const auto execution = simulation;
        const auto settings = CheckedValuation(productCopy, modelCopy, valuation);
        Script::ValidateSimulationSettings(execution);
        const size_t numPaths = static_cast<size_t>(nPaths);
        const auto results = execution.enableAad_ ? Script::MCSimulation<AAD::Number_>(*productCopy, modelCopy, numPaths, settings, execution)
                                                  : Script::MCSimulation<double>(*productCopy, modelCopy, numPaths, settings, execution);
        std::map<String_, double> res;
        res["PV"] = results.aggregated_ / static_cast<double>(numPaths);
        if (execution.enableAad_)
            for (const auto& n : results.names_)
                res["d_" + n] = results[n];
        return res;
    }
} // namespace Dal

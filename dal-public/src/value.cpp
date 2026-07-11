//
// Created by wegam on 2022/11/20.
//

#include <dal/model/factory.hpp>
#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

#include <dal-public/src/value.hpp>

#include <mutex>

namespace Dal {
    using AAD::Model_;
    using Script::SimResults_;

    namespace {
        const std::set<String_> MODEL_STORE = {"BSModelData_", "DupireModelData_"};
        std::mutex VALUE_MUTEX;
    } // namespace

    std::map<String_, double> ValueByMonteCarlo(const Handle_<ScriptProductData_>& product,
                                                const Handle_<ModelData_>& modelData,
                                                int nPaths,
                                                const String_& rsg,
                                                bool useBb,
                                                bool enableAad,
                                                double smooth,
                                                std::optional<bool> compiled) {
        REQUIRE(nPaths > 0, "number of Monte Carlo paths must be positive");
        std::lock_guard<std::mutex> lock(VALUE_MUTEX);
        const size_t numPaths = static_cast<size_t>(nPaths);
        const auto modelType = modelData->Type();
        REQUIRE(MODEL_STORE.find(modelType) != MODEL_STORE.end(), "only support Black-Scholes and Dupire model now");
        auto prd = product->Product();
        std::map<String_, double> res;
        if (enableAad) {
            int maxNestedIfs = prd.PreProcess(true, true);
            SimResults_ results = Script::MCSimulation<AAD::Number_>(prd, modelData, numPaths, rsg, useBb, compiled, maxNestedIfs, smooth);
            res["PV"] = results.aggregated_ / static_cast<double>(numPaths);
            for (const auto& n : results.names_)
                res["d_" + n] = results[n];
        } else {
            prd.PreProcess(false, false);
            SimResults_ results = Script::MCSimulation<double>(prd, modelData, numPaths, rsg, useBb, compiled);
            res["PV"] = results.aggregated_ / static_cast<double>(numPaths);
            return res;
        }
        return res;
    }
} // namespace Dal

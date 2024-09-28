//
// Created by wegam on 2022/11/20.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <public/src/value.hpp>
#include <dal/model/factory.hpp>

namespace Dal {
    using AAD::Model_;
    using Script::SimResults_;

    namespace {
        const std::set<String_> MODEL_STORE = {
                "BSModelData_",
                "DupireModelData_"
        };
    }


    std::map<String_, double> ValueByMonteCarlo(const Handle_<ScriptProductData_>& product,
                                                const Handle_<ModelData_>& model_data,
                                                int n_paths,
                                                const String_& rsg,
                                                bool use_bb,
                                                bool enable_aad,
                                                double smooth) {
        const auto modelType = model_data->Type();
        REQUIRE(MODEL_STORE.find(modelType) != MODEL_STORE.end(), "only support black scholes and Dupire model now");
        auto prd = product->Product();
        std::map<String_, double> res;
        if (enable_aad) {
            int max_nested_ifs = prd.PreProcess(true, true);
            SimResults_ results = Script::MCSimulation<AAD::Number_>(prd, model_data, n_paths, rsg, use_bb, false, max_nested_ifs, smooth);
            res["PV"] = results.aggregated_ / static_cast<double>(n_paths);
            for(const auto& n: results.names_)
                res["d_" + n] = results[n];
        } else {
            prd.PreProcess(false, false);
            SimResults_ results = Script::MCSimulation<double>(prd, model_data, n_paths, rsg, use_bb, false);
            res["PV"] = results.aggregated_ / static_cast<double>(n_paths);
            return res;
        }
        return res;
    }
}
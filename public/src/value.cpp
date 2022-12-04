//
// Created by wegam on 2022/11/20.
//


#include <public/src/value.hpp>
#include <dal/platform/strict.hpp>


namespace Dal {
    using AAD::Model_;
    using AAD::BlackScholes_;
    using AAD::BSModelData_;
    using Script::SimResults_;

    std::map<String_, double> ValueByMonteCarlo(const Handle_<ScriptProductData_>& product,
                                                const Handle_<ModelData_>& modelData,
                                                int n_paths,
                                                const String_& rsg,
                                                bool use_bb,
                                                bool enable_aad,
                                                double smooth) {
        REQUIRE(modelData->Type() == "BSModelData", "only support black scholes model now");
        auto model_imp = dynamic_cast<const BSModelData_*>(modelData.get());
        std::unique_ptr<Model_<double>> mdl;
        std::unique_ptr<Model_<AAD::Number_>> aad_model;
        auto& prd = product->Product();
        std::map<String_, double> res;
        if (enable_aad) {
            aad_model = std::make_unique<BlackScholes_<AAD::Number_>>(model_imp->spot_,
                                                                      model_imp->vol_,
                                                                      model_imp->rate_,
                                                                      model_imp->div_);
            int max_nested_ifs = prd.PreProcess(true, true);
            SimResults_<AAD::Number_> results = MCSimulation(prd, *aad_model, n_paths, rsg, use_bb, max_nested_ifs, smooth);
            Vector_<String_> parameters = aad_model->ParameterLabels();
            auto sum = 0.0;
            auto n = results.Rows();
            for (auto row = 0; row < n; ++row)
                sum += results.aggregated_[row];
            res["value"] = sum / static_cast<double>(n);
            for(int i = 0; i < parameters.size(); ++i)
                res["d_" + parameters[i]] = results.risks_[i];
        }
        else {
            mdl = std::make_unique<BlackScholes_<>>(model_imp->spot_,
                                                    model_imp->vol_,
                                                    model_imp->rate_,
                                                    model_imp->div_);
            prd.PreProcess(false, false);
            SimResults_<> results = MCSimulation(prd, *mdl, n_paths, rsg, use_bb);
            auto sum = 0.0;
            auto n = results.Rows();
            for (auto row = 0; row < n; ++row)
                sum += results.aggregated_[row];
            res["value"] = sum / static_cast<double>(n);
            return res;
        }
        return res;
    }
}
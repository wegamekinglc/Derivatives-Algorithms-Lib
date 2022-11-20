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

    std::map<String_, double> ValueByMonteCarlo(const Handle_<ScriptProduct_>& product,
                                                const Handle_<ModelData_>& modelData,
                                                int n_paths,
                                                const String_& rsg,
                                                bool use_bb) {
        REQUIRE(modelData->Type() == "BSModelData", "only support black scholes model now");
        auto model_imp = dynamic_cast<const BSModelData_*>(modelData.get());
        std::unique_ptr<Model_<double>> mdl = std::make_unique<BlackScholes_<>>(model_imp->spot_,
                                                                                model_imp->vol_,
                                                                                model_imp->spotMeasure_,
                                                                                model_imp->rate_,
                                                                                model_imp->div_);

        auto prd = product->Clone();
        int maxNestedIfs = prd->PreProcess(false, false);
        SimResults_<> results = MCSimulation(*prd, *mdl, n_paths, rsg, use_bb);

        std::map<String_, double> res;
        auto sum = 0.0;
        auto n = results.Rows();
        for (auto row = 0; row < n; ++row)
            sum += results.aggregated_[row];
        res["value"] = sum / static_cast<double>(n);
        return res;
    }
}
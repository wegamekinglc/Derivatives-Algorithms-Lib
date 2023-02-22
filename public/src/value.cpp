//
// Created by wegam on 2022/11/20.
//

#include <set>
#include <public/src/value.hpp>
#include <dal/platform/strict.hpp>


namespace Dal {
    using AAD::Model_;
    using AAD::BlackScholes_;
    using AAD::Dupire_;
    using AAD::BSModelData_;
    using AAD::DupireModelData_;
    using Script::SimResults_;

    namespace {
        const std::set<String_> MODEL_STORE = {
                "BSModelData_",
                "DupireModelData_"
        };

        template <class T_>
        auto ToMatrix(const Matrix_<>& src) {
            Matrix_<T_> rtn(src.Rows(), src.Cols());
            for (int i = 0; i < rtn.Rows(); ++i)
                for (int j = 0; j < rtn.Cols(); ++j)
                    rtn(i, j) = T_(src(i, j));
            return rtn;
        }
    }


    std::map<String_, double> ValueByMonteCarlo(const Handle_<ScriptProductData_>& product,
                                                const Handle_<ModelData_>& modelData,
                                                int n_paths,
                                                const String_& rsg,
                                                bool use_bb,
                                                bool enable_aad,
                                                double smooth) {
        const auto model_type = modelData->Type();
        REQUIRE(MODEL_STORE.find(model_type) != MODEL_STORE.end(), "only support black scholes and Dupire model now");
        std::unique_ptr<Model_<double>> model;
        std::unique_ptr<Model_<AAD::Number_>> aad_model;
        auto& prd = product->Product();
        std::map<String_, double> res;
        if (enable_aad) {
            if (model_type == "BSModelData_") {
                auto model_imp = dynamic_cast<const BSModelData_*>(modelData.get());
                aad_model = std::make_unique<BlackScholes_<AAD::Number_>>(model_imp->spot_,
                                                                          model_imp->vol_,
                                                                          model_imp->rate_,
                                                                          model_imp->div_);
            }
            else if (model_type == "DupireModelData_") {
                auto model_imp = dynamic_cast<const DupireModelData_*>(modelData.get());
                aad_model = std::make_unique<Dupire_<AAD::Number_>>(AAD::Number_(model_imp->spot_),
                                                                    AAD::Number_(model_imp->rate_),
                                                                    AAD::Number_(model_imp->repo_),
                                                                    model_imp->spots_,
                                                                    model_imp->times_,
                                                                    ToMatrix<AAD::Number_>(model_imp->vols_));
            }
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
            if (model_type == "BSModelData_") {
                auto model_imp = dynamic_cast<const BSModelData_*>(modelData.get());
                model = std::make_unique<BlackScholes_<>>(model_imp->spot_,
                                                          model_imp->vol_,
                                                          model_imp->rate_,
                                                          model_imp->div_);
            }
            else if (model_type == "DupireModelData_") {
                auto model_imp = dynamic_cast<const DupireModelData_*>(modelData.get());
                model = std::make_unique<Dupire_<>>(model_imp->spot_,
                                                    model_imp->rate_,
                                                    model_imp->repo_,
                                                    model_imp->spots_,
                                                    model_imp->times_,
                                                    model_imp->vols_);
            }
            prd.PreProcess(false, false);
            SimResults_<> results = MCSimulation(prd, *model, n_paths, rsg, use_bb);
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
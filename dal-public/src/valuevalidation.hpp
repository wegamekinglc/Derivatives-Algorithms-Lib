//
// Created by Codex on 2026/10/4.
//

#pragma once

#include <set>

#include <dal/model/base.hpp>
#include <dal/script/event.hpp>

namespace Dal::Detail {
    inline const std::set<String_> SCRIPT_MODEL_STORE = {"BSModelData_", "CorrelatedBSModelData_",  "HybridModelData_",
                                                         "GSRModelData", "MultiFactorGSRModelData", "GSRSLVModelData"};

    inline void CheckScriptValuationInputs(const Handle_<Script::ScriptProductData_>& product, const Handle_<ModelData_>& modelData) {
        REQUIRE2(product, "InvalidSetting: product=null; expected a non-null product", ScriptError_);
        REQUIRE2(modelData, "InvalidSetting: modelData=null; expected a non-null model", ScriptError_);
        const auto modelType = modelData->Type();
        REQUIRE2(SCRIPT_MODEL_STORE.find(modelType) != SCRIPT_MODEL_STORE.end(),
                 "InvalidSetting: modelData.Type=" + modelType +
                     "; expected BSModelData_, CorrelatedBSModelData_, HybridModelData_, GSRModelData, MultiFactorGSRModelData, or GSRSLVModelData",
                 ScriptError_);
    }
} // namespace Dal::Detail

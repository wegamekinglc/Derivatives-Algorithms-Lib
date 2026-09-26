//
// Created by Codex on 2026/9/27.
//

#include <dal/model/correlatedblackscholes.hpp>
#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

namespace Dal {
#include <dal/auto/MG_CorrelatedBSModelData_v1_Read.inc>
#include <dal/auto/MG_CorrelatedBSModelData_v1_Write.inc>

    void CorrelatedBSModelData_::Write(Archive::Store_& dst) const {
        CorrelatedBSModelData_v1::XWrite(dst, name_, indices_, spots_, vols_, divs_, rate_, correlations_);
    }

    std::unique_ptr<ModelData_> CorrelatedBSModelData_::MutantModel(const String_* newName, const Slide_* slide) const {
        REQUIRE(!slide, "slides are not supported for CorrelatedBSModelData");
        return std::make_unique<CorrelatedBSModelData_>(*newName, indices_, spots_, vols_, divs_, rate_, correlations_);
    }
} // namespace Dal

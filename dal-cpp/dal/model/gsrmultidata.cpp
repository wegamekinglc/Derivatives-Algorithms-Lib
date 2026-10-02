//
// Created by Codex on 2026/10/2.
//

#include <dal/platform/platform.hpp>

#include <dal/model/gsrmultidata.hpp>
#include <dal/platform/strict.hpp>

namespace Dal {
#include <dal/auto/MG_MultiFactorGSRModelData_v1_Read.inc>
#include <dal/auto/MG_MultiFactorGSRModelData_v1_Write.inc>
#include <dal/auto/MG_MultiFactorGSRVolData_v1_Read.inc>
#include <dal/auto/MG_MultiFactorGSRVolData_v1_Write.inc>

    void MultiFactorGSRVolData_::Write(Archive::Store_& dst) const {
        MultiFactorGSRVolData_v1::XWrite(dst, name_, factorNames_, gKnotDates_, gValues_, hKnotDates_, hValues_, correlations_);
    }
    void MultiFactorGSRModelData_::Write(Archive::Store_& dst) const { MultiFactorGSRModelData_v1::XWrite(dst, name_, curve_, vol_); }
    std::unique_ptr<ModelData_> MultiFactorGSRModelData_::MutantModel(const String_* newName, const Slide_* slide) const {
        REQUIRE(!slide, "slides are not supported for MultiFactorGSRModelData");
        return std::make_unique<MultiFactorGSRModelData_>(*newName, curve_, vol_);
    }
} // namespace Dal

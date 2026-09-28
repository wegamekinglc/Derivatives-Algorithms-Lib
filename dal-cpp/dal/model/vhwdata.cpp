//
// Created by Codex on 2026/9/28.
//

#include <dal/model/vhwdata.hpp>
#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

namespace Dal {
#include <dal/auto/MG_VHWCurveData_v1_Read.inc>
#include <dal/auto/MG_VHWCurveData_v1_Write.inc>
#include <dal/auto/MG_VHWModelData_v1_Read.inc>
#include <dal/auto/MG_VHWModelData_v1_Write.inc>
#include <dal/auto/MG_VHWVolData_v1_Read.inc>
#include <dal/auto/MG_VHWVolData_v1_Write.inc>

    void VHWCurveData_::Write(Archive::Store_& dst) const {
        VHWCurveData_v1::XWrite(dst, name_, evaluationDate_, currency_, nodeDates_, discountLogDF_, projectionTenors_, projectionLogDF_);
    }

    void VHWVolData_::Write(Archive::Store_& dst) const { VHWVolData_v1::XWrite(dst, name_, gKnotDates_, gValues_, hKnotDates_, hValues_); }

    void VHWModelData_::Write(Archive::Store_& dst) const { VHWModelData_v1::XWrite(dst, name_, curve_, vol_); }

    std::unique_ptr<ModelData_> VHWModelData_::MutantModel(const String_* newName, const Slide_* slide) const {
        REQUIRE(!slide, "slides are not supported for VHWModelData");
        return std::make_unique<VHWModelData_>(*newName, curve_, vol_);
    }
} // namespace Dal

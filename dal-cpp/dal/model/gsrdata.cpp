//
// Created by Codex on 2026/9/28.
//

#include <dal/model/gsrdata.hpp>
#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

namespace Dal {
#include <dal/auto/MG_GSRCurveData_v1_Read.inc>
#include <dal/auto/MG_GSRCurveData_v1_Write.inc>
#include <dal/auto/MG_GSRModelData_v1_Read.inc>
#include <dal/auto/MG_GSRModelData_v1_Write.inc>
#include <dal/auto/MG_GSRVolData_v1_Read.inc>
#include <dal/auto/MG_GSRVolData_v1_Write.inc>

    void GSRCurveData_::Write(Archive::Store_& dst) const {
        GSRCurveData_v1::XWrite(dst, name_, evaluationDate_, currency_, nodeDates_, discountLogDF_, projectionTenors_, projectionLogDF_);
    }

    void GSRVolData_::Write(Archive::Store_& dst) const { GSRVolData_v1::XWrite(dst, name_, gKnotDates_, gValues_, hKnotDates_, hValues_); }

    void GSRModelData_::Write(Archive::Store_& dst) const { GSRModelData_v1::XWrite(dst, name_, curve_, vol_); }

    std::unique_ptr<ModelData_> GSRModelData_::MutantModel(const String_* newName, const Slide_* slide) const {
        REQUIRE(!slide, "slides are not supported for GSRModelData");
        return std::make_unique<GSRModelData_>(*newName, curve_, vol_);
    }
} // namespace Dal

//
// Created by Codex on 2026/9/27.
//

#include <dal/model/hybriddata.hpp>
#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

namespace Dal {
#include <dal/auto/MG_HybridBSEquityData_v1_Read.inc>
#include <dal/auto/MG_HybridBSEquityData_v1_Write.inc>
#include <dal/auto/MG_HybridConstantCorrelationData_v1_Read.inc>
#include <dal/auto/MG_HybridConstantCorrelationData_v1_Write.inc>
#include <dal/auto/MG_HybridDeterministicRateData_v1_Read.inc>
#include <dal/auto/MG_HybridDeterministicRateData_v1_Write.inc>
#include <dal/auto/MG_HybridModelData_v1_Read.inc>
#include <dal/auto/MG_HybridModelData_v1_Write.inc>

    void HybridBSEquityData_::Write(Archive::Store_& dst) const {
        HybridBSEquityData_v1::XWrite(dst, name_, index_, currency_, factor_, spot_, vol_, div_);
    }

    void HybridDeterministicRateData_::Write(Archive::Store_& dst) const { HybridDeterministicRateData_v1::XWrite(dst, name_, currency_, rate_); }

    void HybridConstantCorrelationData_::Write(Archive::Store_& dst) const {
        HybridConstantCorrelationData_v1::XWrite(dst, name_, factorNames_, correlations_);
    }

    void HybridModelData_::Write(Archive::Store_& dst) const { HybridModelData_v1::XWrite(dst, name_, domesticCurrency_, components_, correlation_); }

    std::unique_ptr<ModelData_> HybridModelData_::MutantModel(const String_* newName, const Slide_* slide) const {
        REQUIRE(!slide, "slides are not supported for HybridModelData");
        return std::make_unique<HybridModelData_>(*newName, domesticCurrency_, components_, correlation_);
    }
} // namespace Dal

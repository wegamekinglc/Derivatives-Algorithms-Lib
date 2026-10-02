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
#include <dal/auto/MG_HybridGSRRateData_v2_Read.inc>
#include <dal/auto/MG_HybridGSRRateData_v2_Write.inc>
#include <dal/auto/MG_HybridGSRSLVRateData_v1_Read.inc>
#include <dal/auto/MG_HybridGSRSLVRateData_v1_Write.inc>
#include <dal/auto/MG_HybridLocalVolEquityData_v1_Read.inc>
#include <dal/auto/MG_HybridLocalVolEquityData_v1_Write.inc>
#include <dal/auto/MG_HybridLogDfRateData_v1_Read.inc>
#include <dal/auto/MG_HybridLogDfRateData_v1_Write.inc>
#include <dal/auto/MG_HybridModelData_v1_Read.inc>
#include <dal/auto/MG_HybridModelData_v1_Write.inc>

    void HybridBSEquityData_::Write(Archive::Store_& dst) const {
        HybridBSEquityData_v1::XWrite(dst, name_, index_, currency_, factor_, spot_, vol_, div_);
    }

    void HybridLocalVolEquityData_::Write(Archive::Store_& dst) const {
        HybridLocalVolEquityData_v1::XWrite(dst, name_, index_, currency_, factor_, spot_, div_, surface_, maxStep_);
    }

    void HybridGSRRateData_::Write(Archive::Store_& dst) const { HybridGSRRateData_v2::XWrite(dst, name_, factors_, curve_, vol_, multiVol_); }

    Vector_<String_> HybridGSRSLVRateData_::RiskLabels() const {
        const auto& curve = *model_->gaussian_->curve_;
        const auto& vol = *model_->gaussian_->vol_;
        Vector_<String_> labels;
        for (size_t i = 1; i < curve.nodeDates_.size(); ++i)
            labels.push_back("logdf:OIS:" + Date::ToString(curve.nodeDates_[i]));
        for (size_t row = 0; row < curve.projectionTenors_.size(); ++row)
            for (size_t i = 1; i < curve.nodeDates_.size(); ++i)
                labels.push_back("logdf:" + curve.projectionTenors_[row] + ":" + Date::ToString(curve.nodeDates_[i]));
        for (size_t factor = 0; factor < vol.factorNames_.size(); ++factor)
            for (const auto& date : vol.gKnotDates_)
                labels.push_back("g:" + vol.factorNames_[factor] + ":" + Date::ToString(date));
        for (size_t factor = 0; factor < vol.factorNames_.size(); ++factor)
            for (const auto& date : vol.hKnotDates_)
                labels.push_back("H:" + vol.factorNames_[factor] + ":" + Date::ToString(date));
        labels.push_back("kappa");
        labels.push_back("volOfVol");
        for (int row = 0; row < model_->leverage_->values_.Rows(); ++row)
            for (int col = 0; col < model_->leverage_->values_.Cols(); ++col)
                labels.push_back("leverage:" + String::FromInt(row) + ":" + String::FromInt(col));
        return labels;
    }

    void HybridGSRSLVRateData_::Write(Archive::Store_& dst) const { HybridGSRSLVRateData_v1::XWrite(dst, name_, volFactor_, bridgeFactor_, model_); }

    void HybridDeterministicRateData_::Write(Archive::Store_& dst) const { HybridDeterministicRateData_v1::XWrite(dst, name_, currency_, rate_); }

    void HybridLogDfRateData_::Write(Archive::Store_& dst) const { HybridLogDfRateData_v1::XWrite(dst, name_, currency_, times_, logDF_, scheme_); }

    void HybridConstantCorrelationData_::Write(Archive::Store_& dst) const {
        HybridConstantCorrelationData_v1::XWrite(dst, name_, factorNames_, correlations_);
    }

    void HybridModelData_::Write(Archive::Store_& dst) const { HybridModelData_v1::XWrite(dst, name_, domesticCurrency_, components_, correlation_); }

    std::unique_ptr<ModelData_> HybridModelData_::MutantModel(const String_* newName, const Slide_* slide) const {
        REQUIRE(!slide, "slides are not supported for HybridModelData");
        return std::make_unique<HybridModelData_>(*newName, domesticCurrency_, components_, correlation_);
    }
} // namespace Dal

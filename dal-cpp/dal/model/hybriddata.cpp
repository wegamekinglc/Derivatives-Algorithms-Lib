//
// Created by Codex on 2026/9/27.
//

#include <algorithm>
#include <cmath>

#include <dal/model/hybriddata.hpp>
#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

namespace Dal {
    namespace {
        void AppendCurveLabels(const GSRCurveData_& curve, Vector_<String_>* labels) {
            for (size_t i = 1; i < curve.nodeDates_.size(); ++i)
                labels->push_back("logdf:OIS:" + Date::ToString(curve.nodeDates_[i]));
            for (size_t row = 0; row < curve.projectionTenors_.size(); ++row)
                for (size_t i = 1; i < curve.nodeDates_.size(); ++i)
                    labels->push_back("logdf:" + curve.projectionTenors_[row] + ":" + Date::ToString(curve.nodeDates_[i]));
        }

        void AppendFactorLabels(const Vector_<String_>& factorNames,
                                const Vector_<Date_>& gDates,
                                const Vector_<Date_>& hDates,
                                Vector_<String_>* labels) {
            const auto prefix = [&](size_t factor) { return factorNames.empty() ? String_() : factorNames[factor] + ":"; };
            const size_t factors = factorNames.empty() ? size_t(1) : factorNames.size();
            for (size_t factor = 0; factor < factors; ++factor)
                for (const auto& date : gDates)
                    labels->push_back("g:" + prefix(factor) + Date::ToString(date));
            for (size_t factor = 0; factor < factors; ++factor)
                for (const auto& date : hDates)
                    labels->push_back("H:" + prefix(factor) + Date::ToString(date));
        }
    } // namespace

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
        const auto& gaussian = *model_->gaussian_;
        Vector_<String_> labels;
        AppendCurveLabels(*gaussian.curve_, &labels);
        AppendFactorLabels(gaussian.vol_->factorNames_, gaussian.vol_->gKnotDates_, gaussian.vol_->hKnotDates_, &labels);
        labels.push_back("kappa");
        labels.push_back("volOfVol");
        for (int row = 0; row < model_->leverage_->values_.Rows(); ++row)
            for (int col = 0; col < model_->leverage_->values_.Cols(); ++col)
                labels.push_back("leverage:" + String::FromInt(row) + ":" + String::FromInt(col));
        return labels;
    }

    Vector_<String_> HybridGSRRateData_::RiskLabels() const {
        Vector_<String_> labels;
        AppendCurveLabels(*curve_, &labels);
        if (multiVol_)
            AppendFactorLabels(multiVol_->factorNames_, multiVol_->gKnotDates_, multiVol_->hKnotDates_, &labels);
        else
            AppendFactorLabels(Vector_<String_>(), vol_->gKnotDates_, vol_->hKnotDates_, &labels);
        return labels;
    }

    Matrix_<> HybridGSRSLVRateData_::FactorCorrelations() const { return model_->FactorCorrelations(); }

    namespace {
        struct ComponentFactors_ {
            size_t offset_;
            Matrix_<> block_;
        };

        void RequireBlockDimensions(const Matrix_<>& block, size_t width, const String_& name) {
            REQUIRE(block.Empty() || (block.Rows() == static_cast<int>(width) && block.Cols() == static_cast<int>(width)),
                    "InvalidHybridCorrelation: correlation block must match the factors of " + name);
        }

        Vector_<String_> GatherComponentFactors(const Vector_<Handle_<HybridComponentData_>>& components, Vector_<ComponentFactors_>* blocks) {
            Vector_<String_> names;
            for (const auto& component : components) {
                REQUIRE(component, "InvalidHybridCorrelation: null component");
                const auto factorNames = component->FactorNames();
                RequireBlockDimensions(component->FactorCorrelations(), factorNames.size(), component->Name());
                for (const auto& factor : factorNames) {
                    REQUIRE(!factor.empty() && std::find(names.begin(), names.end(), factor) == names.end(),
                            "InvalidHybridCorrelation: duplicate or empty factor " + factor);
                    names.push_back(factor);
                }
                blocks->push_back({names.size() - factorNames.size(), component->FactorCorrelations()});
            }
            REQUIRE(!names.empty(), "InvalidHybridCorrelation: at least one factor is required");
            return names;
        }

        // Zero is a legitimate correlation, so duplicate and intra-pair detection tracks
        // ownership in a companion mask instead of reading values back from the matrix.
        Matrix_<> FillCorrelationMatrix(const Vector_<String_>& names, const Vector_<ComponentFactors_>& blocks, Matrix_<char>* owned) {
            const int total = static_cast<int>(names.size());
            Matrix_<> correlations(total, total, 0.0);
            *owned = Matrix_<char>(total, total, 0);
            for (int i = 0; i < total; ++i)
                correlations(i, i) = 1.0;
            for (const auto& block : blocks) {
                if (block.block_.Empty())
                    continue;
                const int width = block.block_.Rows();
                for (int i = 0; i < width; ++i)
                    for (int j = 0; j < width; ++j) {
                        correlations(static_cast<int>(block.offset_) + i, static_cast<int>(block.offset_) + j) = block.block_(i, j);
                        (*owned)(static_cast<int>(block.offset_) + i, static_cast<int>(block.offset_) + j) = 1;
                    }
            }
            return correlations;
        }

        void ApplyFactorLinks(const Vector_<String_>& names, const Vector_<HybridFactorLink_>& links, Matrix_<char>* owned, Matrix_<>* correlations) {
            const auto slot = [&](const String_& factor) {
                const auto found = std::find(names.begin(), names.end(), factor);
                REQUIRE(found != names.end(), "InvalidHybridCorrelation: unknown factor " + factor);
                return static_cast<int>(found - names.begin());
            };
            for (const auto& link : links) {
                const int a = slot(link.factorA_), b = slot(link.factorB_);
                REQUIRE(a != b, "InvalidHybridCorrelation: a link must join two distinct factors");
                REQUIRE(std::isfinite(link.correlation_) && std::abs(link.correlation_) <= 1.0,
                        "InvalidHybridCorrelation: link correlations must be finite in [-1, 1]");
                REQUIRE((*owned)(a, b) == 0, "InvalidHybridCorrelation: duplicate correlation for " + link.factorA_ + " and " + link.factorB_);
                (*owned)(a, b) = (*owned)(b, a) = 1;
                (*correlations)(a, b) = (*correlations)(b, a) = link.correlation_;
            }
        }
    } // namespace

    Handle_<HybridCorrelationData_> AssembleHybridCorrelation(const String_& name,
                                                              const Vector_<Handle_<HybridComponentData_>>& components,
                                                              const Vector_<HybridFactorLink_>& links) {
        Vector_<ComponentFactors_> blocks;
        const auto names = GatherComponentFactors(components, &blocks);
        Matrix_<char> owned;
        auto correlations = FillCorrelationMatrix(names, blocks, &owned);
        ApplyFactorLinks(names, links, &owned, &correlations);
        return Handle_<HybridCorrelationData_>(new HybridConstantCorrelationData_(name, names, correlations));
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

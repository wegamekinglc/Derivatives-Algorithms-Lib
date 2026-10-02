//
// Created by Codex on 2026/9/27.
//

#pragma once

#include <algorithm>
#include <cmath>

#include <dal/curve/logdfinterp.hpp>
#include <dal/model/correlatedblackscholes.hpp>
#include <dal/model/gsrdata.hpp>
#include <dal/model/gsrmultidata.hpp>
#include <dal/model/gsrslvdata.hpp>
#include <dal/model/surface/lvmodel.hpp>
#include <dal/storage/archive.hpp>

/*IF--------------------------------------------------------------------------
storable HybridBSEquityData
    Black-Scholes equity component of a hybrid model
version 1
&members
name is ?string
index is string
currency is string
factor is string
spot is number
vol is number
div is number
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
storable HybridLocalVolEquityData
    Equity component driven by a reusable local-volatility surface
version 1
&members
name is ?string
index is string
currency is string
factor is string
spot is number
div is number
surface is handle LocalVolSurfaceData
maxStep is number
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
storable HybridGSRRateData
    Stochastic domestic GSR rate component of a hybrid model
version 1
&members
name is ?string
factors is string[]
curve is handle GSRCurveData
vol is ?handle GSRVolData
multiVol is ?handle MultiFactorGSRVolData
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
storable HybridDeterministicRateData
    Domestic deterministic-rate component of a hybrid model
version 1
&members
name is ?string
currency is string
rate is number
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
storable HybridLogDfRateData
    Domestic deterministic log-discount-factor rate component of a hybrid model
version 1
&members
name is ?string
currency is string
times is number[]
logDF is number[]
scheme is string
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
storable HybridGSRSLVRateData
    Stochastic local volatility domestic rate component of a hybrid model
version 1
&members
name is ?string
volFactor is string
bridgeFactor is string
model is handle GSRSLVModelData
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
storable HybridConstantCorrelationData
    Constant named-factor correlation of a hybrid model
version 1
&members
name is ?string
factorNames is string[]
correlations is number[][]
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
storable HybridModelData
    Composed hybrid model data
version 1
&members
name is ?string
domesticCurrency is string
components is +handle HybridComponentData
correlation is handle HybridCorrelationData
-IF-------------------------------------------------------------------------*/

namespace Dal {
    struct HybridComponentData_ : Storable_ {
        const String_ currency_;

        HybridComponentData_(const char* type, const String_& name, const String_& currency) : Storable_(type, name), currency_(currency) {}
        [[nodiscard]] virtual Vector_<String_> RiskLabels() const = 0;
        [[nodiscard]] virtual Vector_<String_> FactorNames() const = 0;
        [[nodiscard]] virtual Vector_<String_> ObservableNames() const = 0;
    };

    struct HybridBSEquityData_ : HybridComponentData_ {
        String_ index_;
        String_ factor_;
        double spot_;
        double vol_;
        double div_;

        HybridBSEquityData_(
            const String_& name, const String_& index, const String_& currency, const String_& factor, double spot, double vol, double div)
            : HybridComponentData_("HybridBSEquityData_", name, currency), index_(CanonicalCorrelatedBSAssetNames({index})[0]), factor_(factor),
              spot_(spot), vol_(vol), div_(div) {
            REQUIRE(!name_.empty() && !currency_.empty() && !factor_.empty(),
                    "InvalidHybridComponent: equity name, currency, and factor must be nonempty");
        }
        [[nodiscard]] Vector_<String_> RiskLabels() const override { return {"spot:" + index_, "vol:" + index_, "div:" + index_}; }
        [[nodiscard]] Vector_<String_> FactorNames() const override { return {factor_}; }
        [[nodiscard]] Vector_<String_> ObservableNames() const override { return {index_}; }
        void Write(Archive::Store_& dst) const override;
    };

    struct HybridLocalVolEquityData_ : HybridComponentData_ {
        String_ index_;
        String_ factor_;
        double spot_;
        double div_;
        Handle_<LocalVolSurfaceData_> surface_;
        double maxStep_;

        HybridLocalVolEquityData_(const String_& name,
                                  const String_& index,
                                  const String_& currency,
                                  const String_& factor,
                                  double spot,
                                  double div,
                                  const Handle_<LocalVolSurfaceData_>& surface,
                                  double maxStep = 1.0 / 12.0)
            : HybridComponentData_("HybridLocalVolEquityData_", name, currency), index_(CanonicalCorrelatedBSAssetNames({index})[0]), factor_(factor),
              spot_(spot), div_(div), surface_(surface), maxStep_(maxStep) {
            REQUIRE(!name_.empty() && !currency_.empty() && !factor_.empty() && surface_,
                    "InvalidHybridComponent: local-vol equity name, currency, factor, and surface are required");
            REQUIRE(std::isfinite(spot_) && spot_ > 0.0 && std::isfinite(div_),
                    "InvalidHybridComponent: local-vol equity spot must be positive and dividend finite");
            REQUIRE(std::isfinite(maxStep_) && maxStep_ > 0.0, "InvalidHybridComponent: local-vol maximum step must be positive and finite");
        }
        [[nodiscard]] Vector_<String_> RiskLabels() const override {
            Vector_<String_> labels{"spot:" + index_, "div:" + index_};
            for (int i = 0; i < surface_->vols_.Rows(); ++i)
                for (int j = 0; j < surface_->vols_.Cols(); ++j)
                    labels.push_back("lvol:" + index_ + ":" + String::FromInt(i) + ":" + String::FromInt(j));
            return labels;
        }
        [[nodiscard]] Vector_<String_> FactorNames() const override { return {factor_}; }
        [[nodiscard]] Vector_<String_> ObservableNames() const override { return {index_}; }
        void Write(Archive::Store_& dst) const override;
    };

    struct HybridGSRRateData_ : HybridComponentData_ {
        Vector_<String_> factors_;
        Handle_<GSRCurveData_> curve_;
        Handle_<GSRVolData_> vol_;
        Handle_<MultiFactorGSRVolData_> multiVol_;

        HybridGSRRateData_(const String_& name,
                           const Vector_<String_>& factors,
                           const Handle_<GSRCurveData_>& curve,
                           const Handle_<GSRVolData_>& vol,
                           const Handle_<MultiFactorGSRVolData_>& multiVol)
            : HybridComponentData_("HybridGSRRateData_", name, curve ? curve->currency_ : String_()), factors_(factors), curve_(curve), vol_(vol),
              multiVol_(multiVol) {
            REQUIRE(!name_.empty() && curve_, "InvalidHybridComponent: GSR rate name and curve are required");
            REQUIRE((vol_ != nullptr) != (multiVol_ != nullptr), "InvalidHybridComponent: exactly one of vol and multiVol is required");
            if (multiVol_) {
                if (factors_.empty())
                    factors_ = multiVol_->factorNames_;
                REQUIRE(factors_.size() == multiVol_->factorNames_.size() &&
                            std::all_of(factors_.begin(), factors_.end(), [](const String_& factor) { return !factor.empty(); }),
                        "InvalidHybridComponent: one nonempty hybrid factor name per Gaussian factor is required");
                static_cast<void>(MultiFactorGSRModelData_(name, curve_, multiVol_));
            } else {
                REQUIRE(factors_.size() == 1 && !factors_.front().empty(), "InvalidHybridComponent: one nonempty hybrid factor name is required");
                static_cast<void>(GSRModelData_(name, curve_, vol_));
            }
        }
        HybridGSRRateData_(const String_& name, const String_& factor, const Handle_<GSRCurveData_>& curve, const Handle_<GSRVolData_>& vol)
            : HybridGSRRateData_(name, Vector_<String_>{factor}, curve, vol, Handle_<MultiFactorGSRVolData_>()) {}
        HybridGSRRateData_(const String_& name,
                           const Vector_<String_>& factors,
                           const Handle_<GSRCurveData_>& curve,
                           const Handle_<MultiFactorGSRVolData_>& multiVol)
            : HybridGSRRateData_(name, factors, curve, Handle_<GSRVolData_>(), multiVol) {}
        [[nodiscard]] size_t NumFactors() const { return multiVol_ ? multiVol_->factorNames_.size() : size_t(1); }
        [[nodiscard]] Vector_<String_> RiskLabels() const override {
            Vector_<String_> labels;
            for (size_t i = 1; i < curve_->nodeDates_.size(); ++i)
                labels.push_back("logdf:OIS:" + Date::ToString(curve_->nodeDates_[i]));
            for (size_t row = 0; row < curve_->projectionTenors_.size(); ++row)
                for (size_t i = 1; i < curve_->nodeDates_.size(); ++i)
                    labels.push_back("logdf:" + curve_->projectionTenors_[row] + ":" + Date::ToString(curve_->nodeDates_[i]));
            const auto prefix = [&](size_t factor) { return multiVol_ ? multiVol_->factorNames_[factor] + ":" : String_(); };
            for (size_t factor = 0; factor < NumFactors(); ++factor)
                for (const auto& date : multiVol_ ? multiVol_->gKnotDates_ : vol_->gKnotDates_)
                    labels.push_back("g:" + prefix(factor) + Date::ToString(date));
            for (size_t factor = 0; factor < NumFactors(); ++factor)
                for (const auto& date : multiVol_ ? multiVol_->hKnotDates_ : vol_->hKnotDates_)
                    labels.push_back("H:" + prefix(factor) + Date::ToString(date));
            return labels;
        }
        [[nodiscard]] Vector_<String_> FactorNames() const override { return factors_; }
        [[nodiscard]] Vector_<String_> ObservableNames() const override { return {}; }
        void Write(Archive::Store_& dst) const override;
    };

    struct HybridGSRSLVRateData_ : HybridComponentData_ {
        String_ volFactor_, bridgeFactor_;
        Handle_<GSRSLVModelData_> model_;

        HybridGSRSLVRateData_(const String_& name, const String_& volFactor, const String_& bridgeFactor, const Handle_<GSRSLVModelData_>& model)
            : HybridComponentData_("HybridGSRSLVRateData_", name, model ? model->gaussian_->curve_->currency_ : String_()), volFactor_(volFactor),
              bridgeFactor_(bridgeFactor), model_(model) {
            REQUIRE(!name_.empty() && model_, "InvalidHybridComponent: SLV rate name and model are required");
            REQUIRE(!volFactor_.empty() && !bridgeFactor_.empty(), "InvalidHybridComponent: SLV variance and bridge factor names are required");
            const auto names = FactorNames();
            for (size_t i = 0; i < names.size(); ++i)
                for (size_t j = 0; j < i; ++j)
                    REQUIRE(names[i] != names[j], "InvalidHybridComponent: SLV factor names must be unique");
        }
        [[nodiscard]] Vector_<String_> FactorNames() const override {
            auto names = model_->gaussian_->vol_->factorNames_;
            names.push_back(volFactor_);
            names.push_back(bridgeFactor_);
            return names;
        }
        [[nodiscard]] Vector_<String_> ObservableNames() const override { return {}; }
        [[nodiscard]] Vector_<String_> RiskLabels() const override;
        void Write(Archive::Store_& dst) const override;
    };

    struct HybridDeterministicRateData_ : HybridComponentData_ {
        double rate_;

        HybridDeterministicRateData_(const String_& name, const String_& currency, double rate)
            : HybridComponentData_("HybridDeterministicRateData_", name, currency), rate_(rate) {
            REQUIRE(!name_.empty() && !currency_.empty(), "InvalidHybridComponent: rate name and currency must be nonempty");
        }
        [[nodiscard]] Vector_<String_> RiskLabels() const override { return {"rate:" + currency_}; }
        [[nodiscard]] Vector_<String_> FactorNames() const override { return {}; }
        [[nodiscard]] Vector_<String_> ObservableNames() const override { return {}; }
        void Write(Archive::Store_& dst) const override;
    };

    struct HybridLogDfRateData_ : HybridComponentData_ {
        Vector_<> times_;
        Vector_<> logDF_;
        String_ scheme_;

        HybridLogDfRateData_(
            const String_& name, const String_& currency, const Vector_<>& times, const Vector_<>& logDF, const String_& scheme = "LOG_LINEAR")
            : HybridComponentData_("HybridLogDfRateData_", name, currency), times_(times), logDF_(logDF), scheme_(LogDfScheme_(scheme).String()) {
            REQUIRE(!name_.empty() && !currency_.empty(), "InvalidHybridCurve: component name and currency must be nonempty");
            REQUIRE(times_.size() == logDF_.size() && times_.size() >= 2,
                    "InvalidHybridCurve: times and logDF must have equal length of at least two");
            REQUIRE(times_[0] == 0.0 && logDF_[0] == 0.0, "InvalidHybridCurve: first node must be t=0, logDF=0");
            for (size_t i = 0; i < times_.size(); ++i) {
                REQUIRE(std::isfinite(times_[i]) && std::isfinite(logDF_[i]),
                        "InvalidHybridCurve: non-finite time or logDF at node " + String::FromInt(static_cast<int>(i)));
                REQUIRE(i == 0 || times_[i] > times_[i - 1],
                        "InvalidHybridCurve: times must be strictly increasing at node " + String::FromInt(static_cast<int>(i)));
            }
            static_cast<void>(LogDfInterpolation_(times_, LogDfScheme_(scheme_)));
        }
        [[nodiscard]] Vector_<String_> RiskLabels() const override {
            Vector_<String_> labels;
            for (size_t i = 1; i < logDF_.size(); ++i)
                labels.push_back("logdf:" + currency_ + ":" + String::FromInt(static_cast<int>(i)));
            return labels;
        }
        [[nodiscard]] Vector_<String_> FactorNames() const override { return {}; }
        [[nodiscard]] Vector_<String_> ObservableNames() const override { return {}; }
        void Write(Archive::Store_& dst) const override;
    };

    struct HybridCorrelationData_ : Storable_ {
        HybridCorrelationData_(const char* type, const String_& name) : Storable_(type, name) {}
        [[nodiscard]] virtual const Vector_<String_>& FactorNames() const = 0;
        [[nodiscard]] virtual const Matrix_<>& Correlations() const = 0;
    };

    struct HybridConstantCorrelationData_ : HybridCorrelationData_ {
        Vector_<String_> factorNames_;
        Matrix_<> correlations_;

        HybridConstantCorrelationData_(const String_& name, const Vector_<String_>& factorNames, const Matrix_<>& correlations)
            : HybridCorrelationData_("HybridConstantCorrelationData_", name), factorNames_(factorNames), correlations_(correlations) {}
        [[nodiscard]] const Vector_<String_>& FactorNames() const override { return factorNames_; }
        [[nodiscard]] const Matrix_<>& Correlations() const override { return correlations_; }
        void Write(Archive::Store_& dst) const override;
    };

    struct HybridSettings_ {
        String_ domesticCurrency_;
        Vector_<Handle_<HybridComponentData_>> components_;
        Handle_<HybridCorrelationData_> correlation_;
    };

    struct HybridModelData_ : ModelData_ {
        String_ domesticCurrency_;
        Vector_<Handle_<HybridComponentData_>> components_;
        Handle_<HybridCorrelationData_> correlation_;

        HybridModelData_(const String_& name, const HybridSettings_& settings)
            : HybridModelData_(name, settings.domesticCurrency_, settings.components_, settings.correlation_) {}
        HybridModelData_(const String_& name,
                         const String_& domesticCurrency,
                         const Vector_<Handle_<HybridComponentData_>>& components,
                         const Handle_<HybridCorrelationData_>& correlation)
            : ModelData_("HybridModelData_", name), domesticCurrency_(domesticCurrency), components_(components), correlation_(correlation) {
            REQUIRE(!domesticCurrency_.empty() && correlation_, "InvalidHybridModel: domestic currency and correlation provider are required");
            for (const auto& component : components_)
                REQUIRE(component, "InvalidHybridComponent: null component");
            Vector_<Handle_<HybridComponentData_>> ordered = components_;
            std::sort(ordered.begin(), ordered.end(), [](const auto& lhs, const auto& rhs) { return lhs->Name() < rhs->Name(); });
            for (const auto& component : ordered) {
                const auto labels = component->RiskLabels();
                parameterLabels_.Append(labels);
            }
        }
        void Write(Archive::Store_& dst) const override;

    private:
        std::unique_ptr<ModelData_> MutantModel(const String_* newName, const Slide_* slide) const override;
    };

    // Convenience construction still yields HybridModelData_: local volatility
    // remains an equity component, not a separate simulation model.
    inline Handle_<ModelData_> MakeFlatRateLocalVolHybridModelData(const String_& name,
                                                                   const String_& index,
                                                                   double spot,
                                                                   double rate,
                                                                   double div,
                                                                   const Vector_<>& spots,
                                                                   const Vector_<>& times,
                                                                   const Matrix_<>& vols,
                                                                   double maxStep = 1.0) {
        const Handle_<LocalVolSurfaceData_> surface(new LocalVolSurfaceData_(name + "_vol", spots, times, vols));
        const Vector_<Handle_<HybridComponentData_>> components = {
            Handle_<HybridComponentData_>(new HybridLocalVolEquityData_("equity", index, "USD", "W_EQ", spot, div, surface, maxStep)),
            Handle_<HybridComponentData_>(new HybridDeterministicRateData_("rate", "USD", rate))};
        const Handle_<HybridCorrelationData_> correlation(new HybridConstantCorrelationData_("correlation", {"W_EQ"}, Matrix_<>(1, 1, 1.0)));
        return Handle_<ModelData_>(new HybridModelData_(name, "USD", components, correlation));
    }
} // namespace Dal

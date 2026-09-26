//
// Created by Codex on 2026/9/27.
//

#pragma once

#include <algorithm>

#include <dal/model/correlatedblackscholes.hpp>
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
storable HybridDeterministicRateData
    Domestic deterministic-rate component of a hybrid model
version 1
&members
name is ?string
currency is string
rate is number
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
} // namespace Dal

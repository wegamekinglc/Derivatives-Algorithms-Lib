//
// Created by Codex on 2026/10/5.
//

#pragma once

#include <gtest/gtest.h>

#include <iostream>
#include <string>

#include <rapidjson/document.h>

#include <dal/curve/quoteriskaggregation.hpp>

namespace QuoteRiskRecordChecks {
    inline void AssertRangesUnchanged(const Dal::Vector_<Dal::RateQuoteRiskRange_>& original,
                                      const Dal::Vector_<Dal::RateQuoteRiskRange_>& captured) {
        ASSERT_EQ(captured.size(), original.size());
        for (std::size_t i = 0; i < original.size(); ++i) {
            ASSERT_EQ(captured[i].blockKey_, original[i].blockKey_);
            ASSERT_EQ(captured[i].offset_, original[i].offset_);
            ASSERT_EQ(captured[i].size_, original[i].size_);
        }
    }

    inline void AssertAxisUnchanged(const Dal::RateQuoteRiskAxis_& original, const Dal::RateQuoteRiskAxis_& captured) {
        ASSERT_EQ(captured.scheme_, original.scheme_);
        ASSERT_EQ(captured.fingerprint_, original.fingerprint_);
        AssertRangesUnchanged(original.parameterRanges_, captured.parameterRanges_);
        AssertRangesUnchanged(original.residualRanges_, captured.residualRanges_);
        ASSERT_EQ(captured.parameters_.size(), original.parameters_.size());
        ASSERT_EQ(captured.quotes_.size(), original.quotes_.size());
        for (std::size_t i = 0; i < original.parameters_.size(); ++i) {
            const auto& first = original.parameters_[i];
            const auto& second = captured.parameters_[i];
            ASSERT_EQ(second.blockKey_, first.blockKey_);
            ASSERT_EQ(second.blockOrdinal_, first.blockOrdinal_);
            ASSERT_EQ(second.globalOrdinal_, first.globalOrdinal_);
            ASSERT_EQ(second.date_, first.date_);
            ASSERT_EQ(second.component_, first.component_);
        }
        for (std::size_t i = 0; i < original.quotes_.size(); ++i) {
            const auto& first = original.quotes_[i];
            const auto& second = captured.quotes_[i];
            ASSERT_EQ(second.blockKey_, first.blockKey_);
            ASSERT_EQ(second.blockOrdinal_, first.blockOrdinal_);
            ASSERT_EQ(second.globalOrdinal_, first.globalOrdinal_);
            ASSERT_EQ(second.displayName_, first.displayName_);
            ASSERT_EQ(second.unit_, first.unit_);
        }
    }

    inline void AssertMappingUnchanged(const Dal::RateQuoteRiskProvenance_& original, const Dal::RateQuoteRiskProvenance_& captured) {
        ASSERT_TRUE(original.CalibrationRecord().empty());
        ASSERT_FALSE(captured.CalibrationRecord().empty());
        ASSERT_EQ(captured.Available(), original.Available());
        ASSERT_EQ(captured.Reason(), original.Reason());
        ASSERT_EQ(captured.Kind(), original.Kind());
        ASSERT_EQ(captured.CalibrationId(), original.CalibrationId());
        AssertAxisUnchanged(original.Axis(), captured.Axis());
        ASSERT_EQ(captured.State().scheme_, original.State().scheme_);
        ASSERT_EQ(captured.State().fingerprint_, original.State().fingerprint_);
        ASSERT_EQ(captured.State().components_.size(), original.State().components_.size());
        for (std::size_t i = 0; i < original.State().components_.size(); ++i) {
            ASSERT_EQ(captured.State().components_[i].componentKey_, original.State().components_[i].componentKey_);
            ASSERT_EQ(captured.State().components_[i].fingerprint_, original.State().components_[i].fingerprint_);
        }
        ASSERT_EQ(captured.ComponentKeyByParameterBlock(), original.ComponentKeyByParameterBlock());
        ASSERT_EQ(captured.Tolerance(), original.Tolerance());
        ASSERT_EQ(captured.EffectiveInverse().Rows(), original.EffectiveInverse().Rows());
        ASSERT_EQ(captured.EffectiveInverse().Cols(), original.EffectiveInverse().Cols());
        for (int row = 0; row < original.EffectiveInverse().Rows(); ++row)
            for (int column = 0; column < original.EffectiveInverse().Cols(); ++column)
                ASSERT_EQ(captured.EffectiveInverse()(row, column), original.EffectiveInverse()(row, column));
    }

    inline void ReadCapturedRecord(const Dal::RateQuoteRiskProvenance_& captured, rapidjson::Document* record) {
        record->Parse<rapidjson::kParseFullPrecisionFlag>(captured.CalibrationRecord().c_str());
        ASSERT_FALSE(record->HasParseError());
        ASSERT_TRUE(record->IsObject());
        for (const auto* field : {"spec", "options", "result", "market", "kind", "tolerance", "effectiveInverseScaling"})
            ASSERT_TRUE(record->HasMember(field)) << field;
        ASSERT_EQ(std::string((*record)["kind"].GetString()), std::string(captured.Kind().c_str()));
        ASSERT_EQ((*record)["tolerance"].GetDouble(), captured.Tolerance());
        ASSERT_EQ(std::string((*record)["effectiveInverseScaling"].GetString()), "solver_scaled");
        std::cout << "CalibrationRecordOracle," << captured.Kind() << ',' << captured.State().fingerprint_ << ',' << captured.CalibrationRecord()
                  << '\n';
    }

    inline void AssertPortfolioUnchanged(const Dal::Vector_<Dal::RateTradeDefinition_>& trades,
                                         const Dal::RatePricingMarket_& market,
                                         const Dal::RateQuoteRiskProvenance_& original,
                                         const Dal::RateQuoteRiskProvenance_& captured) {
        const auto first = Dal::AggregateRatePortfolioQuoteRisk(trades, market, {original});
        const auto second = Dal::AggregateRatePortfolioQuoteRisk(trades, market, {captured});
        ASSERT_TRUE(first.provenanceFailures_.empty());
        ASSERT_TRUE(second.provenanceFailures_.empty());
        ASSERT_FALSE(first.buckets_.empty());
        ASSERT_EQ(second.policy_, first.policy_);
        ASSERT_EQ(second.pvByActualPvCcy_, first.pvByActualPvCcy_);
        ASSERT_EQ(second.buckets_.size(), first.buckets_.size());
        ASSERT_EQ(second.meta_.size(), first.meta_.size());
        for (std::size_t i = 0; i < first.buckets_.size(); ++i) {
            const auto& expected = first.buckets_[i];
            const auto& actual = second.buckets_[i];
            ASSERT_EQ(actual.calibrationId_, expected.calibrationId_);
            ASSERT_EQ(actual.axisFingerprint_, expected.axisFingerprint_);
            ASSERT_EQ(actual.quoteKey_, expected.quoteKey_);
            ASSERT_EQ(actual.quoteName_, expected.quoteName_);
            ASSERT_EQ(actual.residualBlock_, expected.residualBlock_);
            ASSERT_EQ(actual.quoteOrdinal_, expected.quoteOrdinal_);
            ASSERT_EQ(actual.actualPvCcy_, expected.actualPvCcy_);
            ASSERT_EQ(actual.dPvDDecimalQuote_, expected.dPvDDecimalQuote_);
            ASSERT_EQ(actual.dv01_, expected.dv01_);
        }
        for (std::size_t i = 0; i < first.meta_.size(); ++i) {
            const auto& expected = first.meta_[i];
            const auto& actual = second.meta_[i];
            ASSERT_EQ(actual.instrumentId_, expected.instrumentId_);
            ASSERT_EQ(actual.calibrationId_, expected.calibrationId_);
            ASSERT_EQ(actual.eligible_, expected.eligible_);
            ASSERT_EQ(actual.structuralZero_, expected.structuralZero_);
            ASSERT_EQ(actual.reason_, expected.reason_);
            ASSERT_EQ(actual.failingComponentKey_, expected.failingComponentKey_);
            ASSERT_EQ(actual.originalNodeRiskReason_, expected.originalNodeRiskReason_);
            ASSERT_EQ(actual.actualPvCcy_, expected.actualPvCcy_);
            ASSERT_EQ(actual.pv_, expected.pv_);
        }
    }
} // namespace QuoteRiskRecordChecks

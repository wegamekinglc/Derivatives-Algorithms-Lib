//
// Created by dal-implementer on 2026/8/31.
//

#include <dal/curve/quoteriskprovenance.hpp>

#include <gtest/gtest.h>

#include <type_traits>

namespace {
    using SingleFactory_ = Dal::RateQuoteRiskProvenance_ (*)(const Dal::CurveCalibrationSpec_&,
                                                             const Dal::CurveCalibrationResult_&,
                                                             const Dal::CurveCalibrationOptions_&,
                                                             const Dal::RatePricingMarket_&,
                                                             const Dal::RateQuoteRiskProvenanceConfig_&);
    using JointFactory_ = Dal::RateQuoteRiskProvenance_ (*)(const Dal::JointXccyCalibrationSpec_&,
                                                            const Dal::JointXccyCalibrationResult_&,
                                                            const Dal::JointXccyCalibrationOptions_&,
                                                            const Dal::RatePricingMarket_&,
                                                            const Dal::RateQuoteRiskProvenanceConfig_&);
    using StagedFactory_ = Dal::RateQuoteRiskProvenance_ (*)(const Dal::CrossCurrencyCalibrationSpec_&,
                                                             const Dal::CrossCurrencyCalibrationResult_&,
                                                             const Dal::CrossCurrencyCalibrationOptions_&,
                                                             const Dal::RatePricingMarket_&,
                                                             const Dal::RateQuoteRiskProvenanceConfig_&);

    static_assert(std::is_same_v<decltype(&Dal::BuildSingleCurveQuoteRiskProvenance), SingleFactory_>);
    static_assert(std::is_same_v<decltype(&Dal::BuildJointXccyQuoteRiskProvenance), JointFactory_>);
    static_assert(std::is_same_v<decltype(&Dal::BuildStagedXccyBasisQuoteRiskProvenance), StagedFactory_>);
} // namespace

TEST(QuoteRiskProvenanceHeaderTest, TestCoreHeaderIsSelfContained) {
    ASSERT_EQ(Dal::RateQuoteRiskAxisFingerprintScheme(), "dal.quote-risk-axis/1+jcs+sha256");
    ASSERT_EQ(Dal::RateQuoteRiskStateFingerprintScheme(), "dal.quote-risk-state/1+jcs+sha256");
}

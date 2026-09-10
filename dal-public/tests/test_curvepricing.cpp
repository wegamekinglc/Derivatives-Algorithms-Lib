//
// Created by dal-implementer on 2026/7/28.
//

#include <gtest/gtest.h>

#include <type_traits>

#include "jointquoteriskfixture.hpp"
#include <dal-public/src/curvepricing.hpp>
#include <tests/curve/jointxccyquoteriskfixtures.hpp>

TEST(CurvePricingPublicTest, TestJointUnregisteredXccyBaseRisk) {
    using namespace JointXccyQuoteRiskFixtures;
    const auto spec = JointQuoteRiskFixtures::Spec();
    const auto calibrated = Dal::CalibrateJointMultiCurveBundle(spec, Options());
    const auto market = Market(spec, calibrated);
    const auto provenance = Dal::BuildJointMultiCurveQuoteRiskProvenance(spec, calibrated, Options(), market, JointQuoteRiskFixtures::Config(2));
    const auto risk = Dal::AggregateRatePortfolioQuoteRisk({Trade(spec)}, market, {provenance});
    ASSERT_EQ(risk.buckets_.size(), 5);
    ASSERT_TRUE(risk.meta_[0].eligible_);
    const double derivative = (Reprice(spec, Options(), 0, 1, 1.0e-6) - Reprice(spec, Options(), 0, 1, -1.0e-6)) / 2.0e-6;
    ASSERT_NEAR(risk.buckets_[1].dPvDDecimalQuote_, derivative, 1.0e-3);
    ASSERT_EQ(risk.buckets_[1].actualPvCcy_, Dal::Ccy_("EUR"));
}

TEST(CurvePricingPublicTest, TestGenericJointCrossLanguageReference) {
    const auto spec = JointQuoteRiskPublicFixture::Spec();
    Dal::JointMultiCurveCalibrationOptions_ options;
    options.computeEffJacobianInverse_ = true;
    const auto result = Dal::CalibrateJointMultiCurveBundle(spec, options);
    const auto market = JointQuoteRiskPublicFixture::Market(result);
    const auto provenance = Dal::BuildJointMultiCurveQuoteRiskProvenance(spec, result, options, market, JointQuoteRiskPublicFixture::Config());
    const auto risk = Dal::AggregateRatePortfolioQuoteRisk({JointQuoteRiskPublicFixture::Trade()}, market, {provenance});
    const auto reference = JointQuoteRiskPublicFixture::ParityRows();
    ASSERT_EQ(risk.buckets_.size(), reference.size());
    for (int row = 0; row < static_cast<int>(reference.size()); ++row) {
        const auto& bucket = risk.buckets_[row];
        ASSERT_EQ(bucket.axisFingerprint_, Dal::String_(reference[row][0]));
        ASSERT_EQ(bucket.quoteKey_, Dal::String_(reference[row][1]));
        ASSERT_STREQ(bucket.actualPvCcy_.String(), reference[row][2].c_str());
        ASSERT_NEAR(bucket.dPvDDecimalQuote_, std::stod(reference[row][3]), 1.0e-5);
        ASSERT_NEAR(bucket.dv01_, std::stod(reference[row][4]), 1.0e-9);
    }
}

TEST(CurvePricingPublicTest, TestGenericJointFacadeAndLegacyAggregateInitialization) {
    const Dal::JointMultiCurveCalibrationOptions_ oldOptions{Dal::CurveJacobianMode_::Value_::BUMPED, false};
    ASSERT_FALSE(oldOptions.computeEffJacobianInverse_);
    using calibrate_t =
        Dal::JointMultiCurveCalibrationResult_ (*)(const Dal::JointMultiCurveCalibrationSpec_&, const Dal::JointMultiCurveCalibrationOptions_&);
    const calibrate_t calibrate = &Dal::CalibrateJointMultiCurveBundle;
    using provenance_t = Dal::RateQuoteRiskProvenance_ (*)(const Dal::JointMultiCurveCalibrationSpec_&, const Dal::JointMultiCurveCalibrationResult_&,
                                                           const Dal::JointMultiCurveCalibrationOptions_&, const Dal::RatePricingMarket_&,
                                                           const Dal::RateQuoteRiskProvenanceConfig_&);
    const provenance_t provenance = &Dal::BuildJointMultiCurveQuoteRiskProvenance;
    ASSERT_NE(calibrate, nullptr);
    ASSERT_NE(provenance, nullptr);
    ASSERT_THROW(calibrate(Dal::JointMultiCurveCalibrationSpec_(), oldOptions), Dal::Exception_);
}

TEST(CurvePricingPublicTest, TestClosedRegistryAndStructuredTerms) {
    const auto families = Dal::RateInstrumentTypeListAll();
    ASSERT_EQ(families.size(), 7);
    ASSERT_STREQ(families[0].String(), "DEPOSIT");
    ASSERT_STREQ(families[6].String(), "XCCY");

    Dal::DepositTradeTerms_ deposit;
    deposit.notional_ = 1'000'000.0;
    deposit.contractRate_ = 0.04;
    deposit.lend_ = true;
    deposit.discountComponentKey_ = "clab/v1/local/discount/USD/OIS";
    const Dal::RateTradeDefinition_ trade{
        "deposit-1", Dal::RateInstrumentType_("DEPOSIT"), Dal::Date_(2026, 1, 15), Dal::Date_(2026, 1, 15), Dal::Date_(2026, 4, 15), Dal::Ccy_("USD"),
        deposit,
    };

    ASSERT_TRUE(std::holds_alternative<Dal::DepositTradeTerms_>(trade.terms_));
    ASSERT_DOUBLE_EQ(std::get<Dal::DepositTradeTerms_>(trade.terms_).contractRate_, 0.04);
}

TEST(CurvePricingPublicTest, TestNodeSensitivityPublicShapeRemainsAggregateAndCallable) {
    static_assert(std::is_aggregate_v<Dal::RateTradeNodeSensitivityResult_>);
    static_assert(std::is_same_v<decltype(Dal::RateTradeNodeSensitivityResult_::eligible_), bool>);
    static_assert(std::is_same_v<decltype(Dal::RateTradeNodeSensitivityResult_::pv_), double>);
    static_assert(std::is_same_v<decltype(Dal::RateTradeNodeSensitivityResult_::gradient_), Dal::Vector_<>>);
    static_assert(std::is_same_v<decltype(Dal::RateTradeNodeSensitivityResult_::reason_), Dal::String_>);
    using entry_t = Dal::RateTradeNodeSensitivityResult_ (*)(const Dal::RateTradeDefinition_&, const Dal::RatePricingMarket_&, const Dal::String_&);
    const entry_t entry = &Dal::RateTradeNodeSensitivities;

    const Dal::RateTradeNodeSensitivityResult_ defaults;
    ASSERT_NE(entry, nullptr);
    ASSERT_FALSE(defaults.eligible_);
    ASSERT_DOUBLE_EQ(defaults.pv_, 0.0);
    ASSERT_TRUE(defaults.gradient_.empty());
    ASSERT_TRUE(defaults.reason_.empty());
}

TEST(CurvePricingPublicTest, TestNodeSensitivityReasonConsumerAcceptsAdditiveValidationFailure) {
    const auto consume = [](const Dal::String_& reason) {
        if (reason.empty())
            return 0;
        if (reason == "TRADE_FAMILY_NOT_AAD_ENABLED")
            return 1;
        if (reason == "TRADE_DOES_NOT_DEPEND_ON_COMPONENT")
            return 2;
        if (reason == "CURVE_COMPONENT_UNAVAILABLE")
            return 3;
        if (reason == "CURVE_REPRESENTATION_NOT_AAD_ENABLED")
            return 4;
        if (reason == "TRADE_VALIDATION_FAILED")
            return 5;
        if (reason == "AAD_EVALUATION_FAILED")
            return 6;
        return -1;
    };

    ASSERT_EQ(consume("TRADE_VALIDATION_FAILED"), 5);
    ASSERT_EQ(consume("UNKNOWN_REASON"), -1);
}

TEST(CurvePricingPublicTest, TestBatchAndAggregationEntryPointsRemainCallableThroughPublicHeader) {
    static_assert(std::is_aggregate_v<Dal::RateTradeNodeSensitivityCell_>);
    static_assert(std::is_aggregate_v<Dal::RatePortfolioNodeRiskMetaEntry_>);
    static_assert(std::is_aggregate_v<Dal::RatePortfolioNodeRiskComponent_>);
    static_assert(std::is_aggregate_v<Dal::RatePortfolioNodeRisk_>);
    using batch_t = Dal::Vector_<Dal::RateTradeNodeSensitivityCell_> (*)(const Dal::Vector_<Dal::RateTradeDefinition_>&,
                                                                         const Dal::RatePricingMarket_&, const Dal::Vector_<Dal::String_>&);
    using aggregate_t = Dal::RatePortfolioNodeRisk_ (*)(const Dal::Vector_<Dal::RateTradeDefinition_>&, const Dal::RatePricingMarket_&,
                                                        const Dal::Vector_<Dal::String_>&);
    const batch_t batch = &Dal::RateTradeNodeSensitivitiesBatch;
    const aggregate_t aggregate = &Dal::AggregateRatePortfolioNodeRisk;
    ASSERT_NE(batch, nullptr);
    ASSERT_NE(aggregate, nullptr);

    const Dal::RateTradeNodeSensitivityCell_ cellDefaults;
    ASSERT_TRUE(cellDefaults.instrumentId_.empty());
    ASSERT_TRUE(cellDefaults.componentKey_.empty());
    ASSERT_FALSE(cellDefaults.result_.eligible_);

    const Dal::RatePortfolioNodeRisk_ aggregateDefaults;
    ASSERT_EQ(aggregateDefaults.policy_, "UnconvertedByActualPvCcy");
    ASSERT_TRUE(aggregateDefaults.components_.empty());
    ASSERT_TRUE(aggregateDefaults.pvByActualPvCcy_.empty());
    ASSERT_TRUE(aggregateDefaults.meta_.empty());
}

TEST(CurvePricingPublicTest, TestQuoteRiskEntryPointsRemainCallableThroughPublicHeader) {
    using CoreSingleFactory_ = Dal::RateQuoteRiskProvenance_ (*)(const Dal::CurveCalibrationSpec_&, const Dal::CurveCalibrationResult_&,
                                                                 const Dal::CurveCalibrationOptions_&, const Dal::RatePricingMarket_&,
                                                                 const Dal::RateQuoteRiskProvenanceConfig_&);
    using PublicSingleFactory_ =
        Dal::RateQuoteRiskProvenance_ (*)(const Dal::CurveCalibrationSpec_&, const Dal::CalibrationResult_&, const Dal::CurveCalibrationOptions_&,
                                          const Dal::RatePricingMarket_&, const Dal::RateQuoteRiskProvenanceConfig_&);
    using JointFactory_ = Dal::RateQuoteRiskProvenance_ (*)(const Dal::JointXccyCalibrationSpec_&, const Dal::JointXccyCalibrationResult_&,
                                                            const Dal::JointXccyCalibrationOptions_&, const Dal::RatePricingMarket_&,
                                                            const Dal::RateQuoteRiskProvenanceConfig_&);
    using StagedFactory_ = Dal::RateQuoteRiskProvenance_ (*)(const Dal::CrossCurrencyCalibrationSpec_&, const Dal::CrossCurrencyCalibrationResult_&,
                                                             const Dal::CrossCurrencyCalibrationOptions_&, const Dal::RatePricingMarket_&,
                                                             const Dal::RateQuoteRiskProvenanceConfig_&);
    using Aggregate_ = Dal::RatePortfolioQuoteRisk_ (*)(const Dal::Vector_<Dal::RateTradeDefinition_>&, const Dal::RatePricingMarket_&,
                                                        const Dal::Vector_<Dal::RateQuoteRiskProvenance_>&);

    const CoreSingleFactory_ coreSingle = static_cast<CoreSingleFactory_>(&Dal::BuildSingleCurveQuoteRiskProvenance);
    const PublicSingleFactory_ publicSingle = static_cast<PublicSingleFactory_>(&Dal::BuildSingleCurveQuoteRiskProvenance);
    const JointFactory_ joint = &Dal::BuildJointXccyQuoteRiskProvenance;
    const StagedFactory_ staged = &Dal::BuildStagedXccyBasisQuoteRiskProvenance;
    const Aggregate_ aggregate = &Dal::AggregateRatePortfolioQuoteRisk;

    ASSERT_NE(coreSingle, nullptr);
    ASSERT_NE(publicSingle, nullptr);
    ASSERT_NE(joint, nullptr);
    ASSERT_NE(staged, nullptr);
    ASSERT_NE(aggregate, nullptr);
}

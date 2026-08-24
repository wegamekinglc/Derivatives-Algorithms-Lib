//
// Created by dal-implementer on 2026/7/28.
//

#include <gtest/gtest.h>

#include <type_traits>

#include <dal-public/src/curvepricing.hpp>

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
                                                                        const Dal::RatePricingMarket_&,
                                                                        const Dal::Vector_<Dal::String_>&);
    using aggregate_t = Dal::RatePortfolioNodeRisk_ (*)(const Dal::Vector_<Dal::RateTradeDefinition_>&, const Dal::RatePricingMarket_&, const Dal::Vector_<Dal::String_>&);
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

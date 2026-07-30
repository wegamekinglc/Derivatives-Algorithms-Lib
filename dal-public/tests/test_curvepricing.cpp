//
// Created by dal-implementer on 2026/7/28.
//

#include <gtest/gtest.h>

#include <dal-public/src/curvepricing.hpp>

TEST(CurvePricingPublicTest, TestClosedRegistryAndStructuredTerms) {
    const auto families = Dal::RateInstrumentTypeListAll();
    ASSERT_EQ(families.size(), 7);
    ASSERT_EQ(families[0].String(), "DEPOSIT");
    ASSERT_EQ(families[6].String(), "XCCY");

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

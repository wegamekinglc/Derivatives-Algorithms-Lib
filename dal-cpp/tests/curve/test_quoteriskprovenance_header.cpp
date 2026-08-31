//
// Created by dal-implementer on 2026/8/31.
//

#include <gtest/gtest.h>

#include <dal/curve/quoteriskprovenance.hpp>

TEST(QuoteRiskProvenanceHeaderTest, TestFingerprintSchemes) {
    ASSERT_EQ(Dal::RateQuoteRiskAxisFingerprintScheme(), "dal.quote-risk-axis/1+jcs+sha256");
    ASSERT_EQ(Dal::RateQuoteRiskStateFingerprintScheme(), "dal.quote-risk-state/1+jcs+sha256");
}

//
// Created by dal-implementer on 2026/9/11.
//

#include <gtest/gtest.h>

#include <dal/curve/ratecashflowpricing_internal.hpp>

#include "jointxccyquoteriskfixtures.hpp"

using namespace Dal;
using namespace JointXccyQuoteRiskFixtures;

namespace {
    CurveCalibrationSpec_ EuroSingleSpec(const Date_& today) {
        CurveCalibrationSpec_ result;
        result.today_ = today;
        result.ccy_ = "EUR";
        result.curveName_ = "eur-single";
        result.initialGuess_ = 0.02;
        for (int ordinal = 0; ordinal < 3; ++ordinal) {
            const auto maturity = Date::AddMonths(today, 12 * (ordinal + 1));
            result.knotDates_.push_back(maturity);
            result.instruments_.push_back(Handle_<YCInstrument_>(
                new Deposit_(today, today, maturity, 0.017 + ordinal * 0.001, JointQuoteRiskFixtures::Index(0))));
        }
        return result;
    }

    RatePricingMarket_ WithEuroDiscount(const RatePricingMarket_& market, const Handle_<DiscountCurve_>& discount) {
        auto result = market;
        result.curveComponents_["eur-discount"] = discount;
        const auto& old = *market.xccyMarket_;
        const Handle_<CurveBlock_> domestic(new CurveBlock_("EUR", "EUR", {{CollateralType_("OIS"), discount}},
                                                           old.DomesticBlock().ForwardCurves(), DayBasis::Act365F()));
        const Handle_<CurveBlock_> foreign(new CurveBlock_("USD", "USD", old.ForeignBlock().DiscountCurves(),
                                                          old.ForeignBlock().ForwardCurves(), DayBasis::Act365F()));
        auto xccy = std::make_shared<CrossCurrencyMarket_>(domestic, foreign, old.FxSpot(), result.valuationTime_, Ccy_("EUR"), result.fixings_);
        xccy->SetBasisCurve(result.curveComponents_.at("basis"));
        result.xccyMarket_ = xccy;
        return result;
    }

    void AssertMetaEqual(const RatePortfolioQuoteRiskMetaEntry_& actual, const RatePortfolioQuoteRiskMetaEntry_& expected) {
        ASSERT_EQ(actual.instrumentId_, expected.instrumentId_);
        ASSERT_EQ(actual.calibrationId_, expected.calibrationId_);
        ASSERT_EQ(actual.eligible_, expected.eligible_) << actual.originalNodeRiskReason_;
        ASSERT_EQ(actual.structuralZero_, expected.structuralZero_);
        ASSERT_EQ(actual.reason_, expected.reason_);
        ASSERT_EQ(actual.failingComponentKey_, expected.failingComponentKey_);
        ASSERT_EQ(actual.originalNodeRiskReason_, expected.originalNodeRiskReason_);
        ASSERT_EQ(actual.actualPvCcy_, expected.actualPvCcy_);
        ASSERT_DOUBLE_EQ(actual.pv_, expected.pv_);
    }

    void AssertBucketEqual(const RateQuoteRiskBucket_& actual, const RateQuoteRiskBucket_& expected) {
        ASSERT_EQ(actual.calibrationId_, expected.calibrationId_);
        ASSERT_EQ(actual.axisFingerprint_, expected.axisFingerprint_);
        ASSERT_EQ(actual.quoteKey_, expected.quoteKey_);
        ASSERT_EQ(actual.quoteName_, expected.quoteName_);
        ASSERT_EQ(actual.residualBlock_, expected.residualBlock_);
        ASSERT_EQ(actual.quoteOrdinal_, expected.quoteOrdinal_);
        ASSERT_EQ(actual.actualPvCcy_, expected.actualPvCcy_);
        ASSERT_DOUBLE_EQ(actual.dPvDDecimalQuote_, expected.dPvDDecimalQuote_);
        ASSERT_DOUBLE_EQ(actual.dv01_, expected.dv01_);
    }

    void AssertSourceEqual(const RatePortfolioQuoteRisk_& actual, const RatePortfolioQuoteRisk_& expected) {
        ASSERT_TRUE(actual.provenanceFailures_.empty());
        ASSERT_EQ(actual.policy_, expected.policy_);
        ASSERT_EQ(expected.meta_.size(), 1);
        const auto meta = std::find_if(actual.meta_.begin(), actual.meta_.end(), [&](const auto& entry) {
            return entry.calibrationId_ == expected.meta_[0].calibrationId_;
        });
        ASSERT_NE(meta, actual.meta_.end());
        ASSERT_NO_FATAL_FAILURE(AssertMetaEqual(*meta, expected.meta_[0]));
        for (const auto& bucket : expected.buckets_) {
            const auto found = std::find_if(actual.buckets_.begin(), actual.buckets_.end(), [&](const auto& entry) {
                return entry.calibrationId_ == bucket.calibrationId_ && entry.quoteKey_ == bucket.quoteKey_;
            });
            ASSERT_NE(found, actual.buckets_.end());
            ASSERT_NO_FATAL_FAILURE(AssertBucketEqual(*found, bucket));
        }
    }
} // namespace

TEST(JointQuoteRiskTest, TestXccyProvenanceOrderPreservesStandaloneAndJointResults) {
    const auto spec = JointQuoteRiskFixtures::Spec();
    const auto calibrated = CalibrateJointMultiCurve(spec, Options());
    const auto singleSpec = EuroSingleSpec(spec.today_);
    CurveCalibrationOptions_ singleOptions;
    singleOptions.computeEffJacobianInverse_ = true;
    const auto singleResult = CalibrateYieldCurve(singleSpec, singleOptions);
    const Handle_<DiscountCurve_> euro(std::shared_ptr<const DiscountCurve_>(std::shared_ptr<void>(), singleResult.curve_.get()));
    const auto market = WithEuroDiscount(Market(spec, calibrated), euro);
    const auto joint = BuildJointMultiCurveQuoteRiskProvenance(spec, calibrated, Options(), market, JointQuoteRiskFixtures::Config(2));
    const auto single = BuildSingleCurveQuoteRiskProvenance(singleSpec, singleResult, singleOptions, market,
                                                           {"eur-single", {{"eur-single", "eur-discount"}}});
    ASSERT_TRUE(joint.Available());
    ASSERT_TRUE(single.Available());
    const Vector_<RateTradeDefinition_> trades{Trade(spec)};
    const auto jointOnly = AggregateRatePortfolioQuoteRisk(trades, market, {joint});
    const auto singleOnly = AggregateRatePortfolioQuoteRisk(trades, market, {single});
    ASSERT_EQ(jointOnly.buckets_.size(), 5);
    ASSERT_EQ(singleOnly.buckets_.size(), 3);
    ASSERT_TRUE(jointOnly.meta_[0].eligible_);
    ASSERT_TRUE(singleOnly.meta_[0].eligible_);
    ASSERT_EQ(jointOnly.pvByActualPvCcy_, singleOnly.pvByActualPvCcy_);
    for (bool jointFirst : {false, true}) {
        SCOPED_TRACE(jointFirst ? "joint, single" : "single, joint");
        const Vector_<RateQuoteRiskProvenance_> sources = jointFirst ? Vector_<RateQuoteRiskProvenance_>{joint, single}
                                                                  : Vector_<RateQuoteRiskProvenance_>{single, joint};
        RateCashflowPricingInternal::g_nodeSensitivityPassivePriceCount = 0;
        const auto actual = AggregateRatePortfolioQuoteRisk(trades, market, sources);
        ASSERT_EQ(actual.meta_.size(), 2);
        ASSERT_NO_FATAL_FAILURE(AssertSourceEqual(actual, jointOnly));
        ASSERT_NO_FATAL_FAILURE(AssertSourceEqual(actual, singleOnly));
        ASSERT_EQ(actual.buckets_.size(), 8);
        ASSERT_EQ(actual.pvByActualPvCcy_, jointOnly.pvByActualPvCcy_);
        ASSERT_EQ(RateCashflowPricingInternal::g_nodeSensitivityPassivePriceCount, 1);
    }
}

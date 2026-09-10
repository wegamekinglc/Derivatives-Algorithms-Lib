//
// Created by dal-implementer on 2026/9/11.
//

#include <gtest/gtest.h>

#include <array>

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

    Handle_<DiscountCurve_> HistoricalExtraChain(const JointMultiCurveCalibrationSpec_& spec, const Handle_<DiscountCurve_>& base) {
        const auto anchor = Date::AddMonths(spec.today_, -12);
        const Vector_<Date_> knots{Date::AddMonths(spec.today_, -6), Date::AddMonths(spec.today_, 18)};
        const Handle_<DiscountCurve_> pwc(NewDiscountPWC("pwc-middle", "USD", PiecewiseConstant_(knots, {0.003, 0.005}), base));
        const Handle_<DiscountCurve_> log(NewDiscountLogDF("log-middle", "USD", {anchor, knots[0], knots[1]}, {0.0, -0.001, -0.008},
                                                         DayBasis::Act365F(), LogDfScheme_("LOG_LINEAR"), pwc));
        const Handle_<DiscountCurve_> zero(NewDiscountZeroRate("zero-middle", "USD", anchor, knots, {0.004, 0.006}, DayBasis::Act365F(),
                                                              LogDfScheme_("LOG_LINEAR"), log));
        return Handle_<DiscountCurve_>(new Tape::DiscountPWLF_<double>("usd-6m", "USD", knots, {0.009, 0.011}, {0.010, 0.012}, zero));
    }

    RatePricingMarket_ NativeChainMarket(const JointMultiCurveCalibrationSpec_& spec,
                                         const JointMultiCurveCalibrationResult_& calibrated,
                                         bool throughForward = true) {
        return Market(spec, calibrated, false, [&](const auto&) {
            return HistoricalExtraChain(spec, throughForward ? calibrated.forwardCurves_.at(PeriodLength_("3M"))
                                                            : calibrated.discountCurves_.at(CollateralType_("OIS")));
        });
    }

    Vector_<> NativeForwardParameters(const Handle_<DiscountCurve_>& curve) {
        if (const auto* pwc = dynamic_cast<const Tape::DiscountPWC_<double>*>(curve.get()))
            return pwc->FRight();
        const auto& pwlf = dynamic_cast<const Tape::DiscountPWLF_<double>&>(*curve);
        const auto left = pwlf.FLeft(), right = pwlf.FRight();
        Vector_<> result;
        for (int i = 0; i < static_cast<int>(left.size()); ++i) {
            result.push_back(left[i]);
            result.push_back(right[i]);
        }
        return result;
    }

    Handle_<DiscountCurve_> CloneNativeShift(const Handle_<DiscountCurve_>& curve,
                                            const YCComponent_::substitutions_t& bases,
                                            int coordinate,
                                            double shift) {
        auto clone = curve->Clone(curve->Name(), bases);
        if (coordinate >= 0) {
            auto& fit = dynamic_cast<FittableCurve_&>(*clone);
            Vector_<> dx(fit.NX(), 0.0);
            dx[coordinate] = shift;
            fit.ApplyDX(dx.begin(), 1.0);
        }
        return handle_cast<DiscountCurve_>(Handle_<YCComponent_>(std::move(clone)));
    }

    JointMultiCurveCalibrationResult_ NativeBump(const JointMultiCurveCalibrationResult_& calibrated, int block, int coordinate, double shift) {
        auto result = calibrated;
        const auto& discount = calibrated.discountCurves_.at(CollateralType_("OIS"));
        const auto& forward = calibrated.forwardCurves_.at(PeriodLength_("3M"));
        YCComponent_::substitutions_t bases;
        if (block == 0) {
            const auto bumped = CloneNativeShift(discount, {}, coordinate, shift);
            result.discountCurves_[CollateralType_("OIS")] = bumped;
            bases.emplace(discount.get(), handle_cast<YCComponent_>(bumped));
        }
        result.forwardCurves_[PeriodLength_("3M")] = CloneNativeShift(forward, bases, block == 1 ? coordinate : -1, shift);
        return result;
    }

    void AssertNativeBumpState(const RatePricingMarket_& actual, const RatePricingMarket_& original, int block, int coordinate, double shift) {
        for (int index = 0; index < 2; ++index) {
            const auto key = JointQuoteRiskFixtures::BlockKey(index);
            const auto parameters = NativeForwardParameters(actual.curveComponents_.at(key));
            auto expected = NativeForwardParameters(original.curveComponents_.at(key));
            if (index == block)
                expected[coordinate] += shift;
            ASSERT_EQ(parameters, expected);
        }
        if (block == 1) {
            ASSERT_EQ(actual.curveComponents_.at("curve:0"), original.curveComponents_.at("curve:0"));
            ASSERT_EQ(RateCashflowPricingInternal::NodeSensitivityBase(*actual.curveComponents_.at("curve:1")),
                      RateCashflowPricingInternal::NodeSensitivityBase(*original.curveComponents_.at("curve:1")));
        }
    }

    void AssertNativeSlice(const JointMultiCurveCalibrationSpec_& spec,
                           const JointMultiCurveCalibrationResult_& calibrated,
                           const RatePricingMarket_& market,
                           int block,
                           const RateTradeNodeSensitivityCell_& cell,
                           bool throughForward = true) {
        const auto key = JointQuoteRiskFixtures::BlockKey(block);
        const auto passive = PriceRateTrade(Trade(spec), market);
        ASSERT_TRUE(passive.succeeded_);
        ASSERT_EQ(cell.componentKey_, key);
        ASSERT_TRUE(cell.result_.eligible_) << cell.result_.reason_;
        ASSERT_TRUE(cell.result_.reason_.empty());
        ASSERT_NEAR(cell.result_.pv_, passive.pv_, 1.0e-8);
        ASSERT_EQ(cell.result_.gradient_.size(), NativeForwardParameters(market.curveComponents_.at(key)).size());
        constexpr double bump = 1.0e-6;
        for (int coordinate = 0; coordinate < static_cast<int>(cell.result_.gradient_.size()); ++coordinate) {
            SCOPED_TRACE(coordinate);
            std::array<double, 2> pv;
            for (int side = 0; side < 2; ++side) {
                const double shift = side == 0 ? -bump : bump;
                const auto shifted = NativeChainMarket(spec, NativeBump(calibrated, block, coordinate, shift), throughForward);
                ASSERT_NO_FATAL_FAILURE(AssertNativeBumpState(shifted, market, block, coordinate, shift));
                const auto price = PriceRateTrade(Trade(spec), shifted);
                ASSERT_TRUE(price.succeeded_) << price.error_;
                pv[side] = price.pv_;
            }
            ASSERT_NEAR(cell.result_.gradient_[coordinate], (pv[1] - pv[0]) / (2.0 * bump), 1.0e-3);
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

TEST(JointQuoteRiskTest, TestHistoricalMixedChainMatchesEveryNativeCoordinate) {
    for (bool layered : {false, true})
        for (auto layout : {CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD, CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD}) {
            SCOPED_TRACE(layered);
            SCOPED_TRACE(CurveParameterization_(layout).String());
            const auto spec = JointQuoteRiskFixtures::Spec(5, 2, layout, layered);
            const auto calibrated = CalibrateJointMultiCurve(spec, Options());
            const auto market = NativeChainMarket(spec, calibrated);
            const auto cells = RateCashflowPricingInternal::JointNodeSensitivitiesBatch({Trade(spec)}, market, {"curve:0", "curve:1"});
            ASSERT_EQ(cells.size(), 2);
            ASSERT_NO_FATAL_FAILURE(AssertNativeSlice(spec, calibrated, market, 0, cells[0]));
            ASSERT_NO_FATAL_FAILURE(AssertNativeSlice(spec, calibrated, market, 1, cells[1]));
        }
}

TEST(JointQuoteRiskTest, TestHistoricalMixedChainConsumesOnlyNativeDiscount) {
    for (bool layered : {false, true})
        for (auto layout : {CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD, CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD}) {
            SCOPED_TRACE(layered);
            SCOPED_TRACE(CurveParameterization_(layout).String());
            const auto spec = JointQuoteRiskFixtures::Spec(5, 2, layout, layered);
            const auto calibrated = CalibrateJointMultiCurve(spec, Options());
            const auto market = NativeChainMarket(spec, calibrated, false);
            const auto cells = RateCashflowPricingInternal::JointNodeSensitivitiesBatch({Trade(spec)}, market, {"curve:0", "curve:1"});
            ASSERT_EQ(cells.size(), 2);
            ASSERT_NO_FATAL_FAILURE(AssertNativeSlice(spec, calibrated, market, 0, cells[0], false));
            ASSERT_FALSE(cells[1].result_.eligible_);
            ASSERT_EQ(cells[1].result_.reason_, "TRADE_DOES_NOT_DEPEND_ON_COMPONENT");
            ASSERT_TRUE(cells[1].result_.gradient_.empty());
            const auto provenance = BuildJointMultiCurveQuoteRiskProvenance(spec, calibrated, Options(), market, JointQuoteRiskFixtures::Config(2));
            const auto risk = AggregateRatePortfolioQuoteRisk({Trade(spec)}, market, {provenance});
            ASSERT_TRUE(risk.meta_[0].eligible_);
            ASSERT_FALSE(risk.meta_[0].structuralZero_);
            ASSERT_EQ(risk.buckets_.size(), 5);
        }
}

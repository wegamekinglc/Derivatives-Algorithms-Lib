//
// Created by dal-implementer on 2026/9/11.
//

#include <gtest/gtest.h>

#include <dal/curve/ratecashflowpricing_internal.hpp>
#include <dal/storage/archive.hpp>

#include "jointxccyquoteriskfixtures.hpp"

using namespace Dal;
using namespace JointXccyQuoteRiskFixtures;

namespace {
    class OpaqueCurve_ : public DiscountCurve_ {
        Handle_<DiscountCurve_> inner_;

    public:
        explicit OpaqueCurve_(const Handle_<DiscountCurve_>& inner = {}) : DiscountCurve_("opaque", "USD"), inner_(inner) {}
        double operator()(const Date_& from, const Date_& to) const override { return inner_ ? (*inner_)(from, to) : 1.0; }
        void Poll(Vector_<const YCComponent_*>* all) const override {
            all->push_back(this);
            if (inner_)
                inner_->Poll(all);
        }
        void Poll(std::map<const YCComponent_*, Handle_<YCComponent_>>* all) const override {
            if (inner_)
                inner_->Poll(all);
        }
        [[nodiscard]] std::unique_ptr<YCComponent_> Clone(const String_&, const YCComponent_::substitutions_t& changes) const override {
            const auto found = changes.find(inner_.get());
            return std::make_unique<OpaqueCurve_>(found == changes.end() ? inner_ : handle_cast<DiscountCurve_>(found->second));
        }
        void Write(Archive::Store_& dst) const override {
            dst.SetType("OpaqueCurve_JointQuoteRiskTestOnly");
            if (inner_)
                Archive::Utils::Set(dst, "inner", inner_);
            dst.Done();
        }
    };

    class TaggedPwc_ : public Tape::DiscountPWC_<double> {
    public:
        explicit TaggedPwc_(const Tape::DiscountPWC_<double>& curve)
            : Tape::DiscountPWC_<double>(curve.Name(), curve.ccy_.String(), curve.KnotDates(), curve.FRight(), curve.Base()) {}
    };

    RatePortfolioQuoteRisk_ Risk(const JointMultiCurveCalibrationSpec_& spec,
                                 const JointMultiCurveCalibrationResult_& calibrated,
                                 const RatePricingMarket_& market,
                                 const Vector_<RateTradeDefinition_>& trades) {
        const auto provenance = BuildJointMultiCurveQuoteRiskProvenance(spec, calibrated, Options(), market, JointQuoteRiskFixtures::Config(2));
        REQUIRE(provenance.Available(), provenance.Reason());
        return AggregateRatePortfolioQuoteRisk(trades, market, {provenance});
    }

    RateTradeDefinition_ Future(const JointMultiCurveCalibrationSpec_& spec, const String_& key) {
        FutureTradeTerms_ terms;
        terms.contractCount_ = 1.0;
        terms.referencePrice_ = 100.0;
        terms.contractValuePerPricePoint_ = 1.0;
        terms.long_ = true;
        terms.index_ = JointQuoteRiskFixtures::Index(1);
        terms.fixingIdentity_ = {"USD-3M", 11, 0};
        terms.forecastComponentKey_ = key;
        return {"future", RateInstrumentType_::Value_::FUTURE, spec.today_, spec.today_, Date::AddMonths(spec.today_, 3), Ccy_("USD"), terms};
    }

    void AssertIncomplete(const RatePortfolioQuoteRisk_& risk,
                          const RateTradeDefinition_& trade,
                          const RatePricingMarket_& market,
                          const String_& reason = "AAD_EVALUATION_FAILED",
                          const String_& key = "curve:0") {
        ASSERT_TRUE(risk.provenanceFailures_.empty());
        ASSERT_TRUE(risk.buckets_.empty());
        ASSERT_EQ(risk.meta_.size(), 1);
        const auto& meta = risk.meta_[0];
        ASSERT_FALSE(meta.eligible_);
        ASSERT_FALSE(meta.structuralZero_);
        ASSERT_EQ(meta.reason_, "QUOTE_RISK_TRADE_PROVENANCE_INCOMPLETE");
        ASSERT_EQ(meta.originalNodeRiskReason_, reason);
        ASSERT_EQ(meta.failingComponentKey_, key);
        ASSERT_EQ(meta.instrumentId_, trade.instrumentId_);
        ASSERT_EQ(meta.calibrationId_, "generic-joint");
        const auto passive = PriceRateTrade(trade, market);
        ASSERT_DOUBLE_EQ(meta.pv_, passive.succeeded_ ? passive.pv_ : 0.0);
    }
} // namespace

TEST(JointQuoteRiskTest, TestUnregisteredXccyBaseMatchesFullRecalibration) {
    const auto spec = JointQuoteRiskFixtures::Spec();
    const auto calibrated = CalibrateJointMultiCurve(spec, Options());
    const auto market = Market(spec, calibrated);
    const auto risk = Risk(spec, calibrated, market, {Trade(spec)});
    ASSERT_EQ(risk.meta_.size(), 1);
    ASSERT_TRUE(risk.meta_[0].eligible_);
    ASSERT_EQ(risk.buckets_.size(), 5);
    const double derivative = (Reprice(spec, Options(), 0, 1, 1.0e-6) - Reprice(spec, Options(), 0, 1, -1.0e-6)) / 2.0e-6;
    ASSERT_NEAR(risk.buckets_[1].dPvDDecimalQuote_, derivative, 1.0e-3);
}

TEST(JointQuoteRiskTest, TestOpaqueLeafFailsClosed) {
    const auto spec = JointQuoteRiskFixtures::Spec();
    const auto calibrated = CalibrateJointMultiCurve(spec, Options());
    const auto market = Market(spec, calibrated, true, {}, [&](const auto&) {
        return Flat(spec, "eur-3m", "EUR", 0.019, Handle_<DiscountCurve_>(new OpaqueCurve_()));
    });
    const auto risk = Risk(spec, calibrated, market, {Trade(spec)});
    AssertIncomplete(risk, Trade(spec), market);
}

TEST(JointQuoteRiskTest, TestOpaqueForwardingFailsClosed) {
    const auto spec = JointQuoteRiskFixtures::Spec();
    const auto calibrated = CalibrateJointMultiCurve(spec, Options());
    const auto market = Market(spec, calibrated, true, [&](const auto& base) {
        return Flat(spec, "usd-6m", "USD", 0.015, Handle_<DiscountCurve_>(new OpaqueCurve_(base)));
    });
    const auto risk = Risk(spec, calibrated, market, {Trade(spec)});
    AssertIncomplete(risk, Trade(spec), market);
}

TEST(JointQuoteRiskTest, TestDerivedBuiltinFailsClosed) {
    const auto spec = JointQuoteRiskFixtures::Spec();
    const auto calibrated = CalibrateJointMultiCurve(spec, Options());
    const auto market = Market(spec, calibrated, true, {}, [](const auto& root) {
        return Handle_<DiscountCurve_>(new TaggedPwc_(dynamic_cast<const Tape::DiscountPWC_<double>&>(*root)));
    });
    const auto risk = Risk(spec, calibrated, market, {Trade(spec)});
    AssertIncomplete(risk, Trade(spec), market);
}

TEST(JointQuoteRiskTest, TestOpaqueOnlyRootKeepsRepresentationGate) {
    const auto spec = JointQuoteRiskFixtures::Spec();
    const auto calibrated = CalibrateJointMultiCurve(spec, Options());
    auto market = JointQuoteRiskFixtures::Market(spec, calibrated);
    market.curveComponents_["opaque-forecast"] = Handle_<DiscountCurve_>(new OpaqueCurve_(market.curveComponents_.at("curve:0")));
    const auto risk = Risk(spec, calibrated, market, {Future(spec, "opaque-forecast")});
    AssertIncomplete(risk, Future(spec, "opaque-forecast"), market, "CURVE_REPRESENTATION_NOT_AAD_ENABLED");
}

//
// Created by dal-implementer on 2026/9/11.
//

#include <gtest/gtest.h>

#include <array>
#include <future>
#include <limits>

#include <dal/curve/ratecashflowpricing_internal.hpp>
#include <dal/storage/archive.hpp>

#include "jointquoteriskopaque.hpp"
#include "jointxccyquoteriskfixtures.hpp"

using namespace Dal;
using namespace JointXccyQuoteRiskFixtures;

namespace {

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

    void AssertXccyOracleBuckets(const JointMultiCurveCalibrationSpec_& spec,
                                 const JointMultiCurveCalibrationOptions_& options,
                                 const Vector_<RatePortfolioQuoteRisk_>& risks) {
        int global = 0;
        for (int block = 0; block < 2; ++block)
            for (int ordinal = 0; ordinal < static_cast<int>(spec.curves_[block].instruments_.size()); ++ordinal, ++global) {
                const std::array<double, 5> prices{
                    risks[0].meta_[0].pv_, Reprice(spec, options, block, ordinal, 1.0e-6), Reprice(spec, options, block, ordinal, -1.0e-6),
                    Reprice(spec, options, block, ordinal, 1.0e-4), Reprice(spec, options, block, ordinal, -1.0e-4)};
                double scale = 1.0;
                for (double pv : prices)
                    scale = std::max(scale, std::abs(pv));
                const double derivative = (prices[1] - prices[2]) / 2.0e-6;
                const double dv01 = (prices[3] - prices[4]) / 2.0;
                for (const auto& risk : risks) {
                    const auto& bucket = risk.buckets_[global];
                    ASSERT_LE(std::abs(bucket.dPvDDecimalQuote_ - derivative), 5.0e-6 * std::max(scale, std::abs(derivative)));
                    ASSERT_LE(std::abs(bucket.dv01_ - dv01), 5.0e-6 * std::max(scale * 1.0e-4, std::abs(dv01)));
                    ASSERT_LE(std::abs(bucket.dv01_ - 1.0e-4 * bucket.dPvDDecimalQuote_),
                              64.0 * std::numeric_limits<double>::epsilon() * std::max(scale * 1.0e-4, std::abs(bucket.dv01_)));
                }
                ASSERT_DOUBLE_EQ(risks[0].buckets_[global].dPvDDecimalQuote_, risks[1].buckets_[global].dPvDDecimalQuote_);
            }
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
    const auto market = Market(spec, calibrated, true, {},
                               [&](const auto&) { return Flat(spec, "eur-3m", "EUR", 0.019, Handle_<DiscountCurve_>(new OpaqueCurve_())); });
    const auto risk = Risk(spec, calibrated, market, {Trade(spec)});
    AssertIncomplete(risk, Trade(spec), market);
}

TEST(JointQuoteRiskTest, TestOpaqueForwardingFailsClosed) {
    const auto spec = JointQuoteRiskFixtures::Spec();
    const auto calibrated = CalibrateJointMultiCurve(spec, Options());
    const auto market = Market(spec, calibrated, true,
                               [&](const auto& base) { return Flat(spec, "usd-6m", "USD", 0.015, Handle_<DiscountCurve_>(new OpaqueCurve_(base))); });
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

TEST(JointQuoteRiskTest, TestUnregisteredXccyOracleAcrossModesLayoutsLayersAndAliases) {
    for (bool layered : {false, true})
        for (auto layout : {CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD, CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD})
            for (auto mode : {CurveJacobianMode_::Value_::ANALYTIC, CurveJacobianMode_::Value_::BUMPED}) {
                const auto spec = JointQuoteRiskFixtures::Spec(5, 2, layout, layered);
                const auto options = Options(mode);
                const auto calibrated = CalibrateJointMultiCurve(spec, options);
                const auto market = Market(spec, calibrated);
                auto aliased = market;
                const auto extra = market.xccyMarket_->ForeignBlock().ForwardCurves().at(PeriodLength_("6M"));
                aliased.curveComponents_["extra"] = extra;
                aliased.curveComponents_["extra-alias"] = extra;
                Vector_<RatePortfolioQuoteRisk_> risks;
                int preparations = 0, sweeps = 0;
                for (const auto* routed : {&market, static_cast<const RatePricingMarket_*>(&aliased)}) {
                    const auto provenance =
                        BuildJointMultiCurveQuoteRiskProvenance(spec, calibrated, options, *routed, JointQuoteRiskFixtures::Config(2));
                    RateCashflowPricingInternal::g_nodeSensitivityPreparationCount = 0;
                    RateCashflowPricingInternal::g_nodeSensitivitySweepCount = 0;
                    RateCashflowPricingInternal::g_nodeSensitivityPassivePriceCount = 0;
                    RateCashflowPricingInternal::g_quoteRiskProvenancePreparationCount = 0;
                    risks.push_back(AggregateRatePortfolioQuoteRisk({Trade(spec)}, *routed, {provenance}));
                    ASSERT_EQ(RateCashflowPricingInternal::g_nodeSensitivityPassivePriceCount, 1);
                    ASSERT_EQ(RateCashflowPricingInternal::g_quoteRiskProvenancePreparationCount, 1);
                    ASSERT_EQ(risks.back().buckets_.size(), 5);
                    ASSERT_TRUE(risks.back().meta_[0].eligible_);
                    ASSERT_EQ(risks.back().meta_[0].actualPvCcy_, Ccy_("EUR"));
                    if (risks.size() == 1) {
                        preparations = RateCashflowPricingInternal::g_nodeSensitivityPreparationCount;
                        sweeps = RateCashflowPricingInternal::g_nodeSensitivitySweepCount;
                    } else {
                        ASSERT_EQ(RateCashflowPricingInternal::g_nodeSensitivityPreparationCount, preparations);
                        ASSERT_EQ(RateCashflowPricingInternal::g_nodeSensitivitySweepCount, sweeps);
                    }
                }
                ASSERT_NO_FATAL_FAILURE(AssertXccyOracleBuckets(spec, options, risks));
            }
}

TEST(JointQuoteRiskTest, TestMixedUnregisteredLayersPreserveHistoricalKnots) {
    const auto spec = JointQuoteRiskFixtures::Spec();
    const auto calibrated = CalibrateJointMultiCurve(spec, Options());
    for (bool historical : {false, true}) {
        const Vector_<Date_> knots{Date::AddMonths(spec.today_, historical ? -6 : 6), Date::AddMonths(spec.today_, 18)};
        for (bool mixed : {false, true}) {
            const CurveTransform_ chain = [&](const auto& base) {
                Handle_<DiscountCurve_> middle;
                if (mixed) {
                    const auto anchor = Date::AddMonths(spec.today_, -12);
                    const auto log =
                        Handle_<DiscountCurve_>(NewDiscountLogDF("log-middle", "USD", {anchor, knots[0], knots[1]}, {0.0, -0.001, -0.008},
                                                                 DayBasis::Act365F(), LogDfScheme_("LOG_LINEAR"), base));
                    middle = Handle_<DiscountCurve_>(NewDiscountZeroRate("zero-middle", "USD", anchor, knots, {0.004, 0.006}, DayBasis::Act365F(),
                                                                         LogDfScheme_("LOG_LINEAR"), log));
                } else
                    middle = Handle_<DiscountCurve_>(NewDiscountPWC("pwc-middle", "USD", PiecewiseConstant_(knots, {0.003, 0.005}), base));
                return Handle_<DiscountCurve_>(new Tape::DiscountPWLF_<double>("usd-6m", "USD", knots, {0.009, 0.011}, {0.010, 0.012}, middle));
            };
            const auto market = Market(spec, calibrated, false, chain);
            const auto risk = Risk(spec, calibrated, market, {Trade(spec)});
            ASSERT_EQ(risk.buckets_.size(), 5);
            ASSERT_TRUE(risk.meta_[0].eligible_);
            const double derivative = (Reprice(spec, Options(), 0, 1, 1.0e-6, chain) - Reprice(spec, Options(), 0, 1, -1.0e-6, chain)) / 2.0e-6;
            ASSERT_NEAR(risk.buckets_[1].dPvDDecimalQuote_, derivative, 1.0e-2);
        }
    }
}

TEST(JointQuoteRiskTest, TestClosuresArePerTradeAndIgnoreUnusedOpaqueDescendants) {
    const auto spec = JointQuoteRiskFixtures::Spec();
    const auto calibrated = CalibrateJointMultiCurve(spec, Options());
    auto market = Market(spec, calibrated);
    market.curveComponents_["opaque"] = Handle_<DiscountCurve_>(new OpaqueCurve_(market.curveComponents_.at("curve:0")));
    market.curveComponents_["unused-descendant"] = Flat(spec, "unused", "USD", 0.01, market.curveComponents_.at("opaque"));
    const auto future = Future(spec, "curve:0");
    const auto opaque = Future(spec, "opaque");
    const auto xccy = Trade(spec);
    const auto expected = Risk(spec, calibrated, market, {future, xccy});
    for (bool reverse : {false, true}) {
        Vector_<RateTradeDefinition_> trades{future, xccy, opaque};
        if (reverse)
            std::reverse(trades.begin(), trades.end());
        const auto actual = Risk(spec, calibrated, market, trades);
        ASSERT_EQ(actual.meta_.size(), 3);
        ASSERT_EQ(actual.buckets_.size(), expected.buckets_.size());
        ASSERT_EQ(actual.meta_[reverse ? 0 : 2].originalNodeRiskReason_, "CURVE_REPRESENTATION_NOT_AAD_ENABLED");
        for (int i = 0; i < static_cast<int>(actual.buckets_.size()); ++i)
            ASSERT_DOUBLE_EQ(actual.buckets_[i].dPvDDecimalQuote_, expected.buckets_[i].dPvDDecimalQuote_);
    }
    const auto clean = Market(spec, calibrated);
    RateCashflowPricingInternal::g_nodeSensitivityPreparationCount = 0;
    Risk(spec, calibrated, clean, {future, xccy});
    const int preparations = RateCashflowPricingInternal::g_nodeSensitivityPreparationCount;
    RateCashflowPricingInternal::g_nodeSensitivityPreparationCount = 0;
    Risk(spec, calibrated, market, {future, xccy});
    ASSERT_EQ(RateCashflowPricingInternal::g_nodeSensitivityPreparationCount, preparations);
}

TEST(JointQuoteRiskTest, TestSingleCurrencyUnregisteredChainAndStandaloneFixedBase) {
    const auto spec = JointQuoteRiskFixtures::Spec();
    const auto calibrated = CalibrateJointMultiCurve(spec, Options());
    auto market = JointQuoteRiskFixtures::Market(spec, calibrated);
    market.curveComponents_["spread"] = Flat(spec, "spread", "USD", 0.015, Flat(spec, "middle", "USD", 0.004, market.curveComponents_.at("curve:0")));
    const auto trade = Future(spec, "spread");
    const auto joint = Risk(spec, calibrated, market, {trade});
    ASSERT_TRUE(joint.meta_[0].eligible_);
    ASSERT_FALSE(joint.meta_[0].structuralZero_);
    const auto standalone = RateTradeNodeSensitivities(trade, market, "curve:0");
    ASSERT_FALSE(standalone.eligible_);
    ASSERT_EQ(standalone.reason_, "TRADE_DOES_NOT_DEPEND_ON_COMPONENT");
    const auto active = RateCashflowPricingInternal::JointNodeSensitivitiesBatch({trade}, market, {"curve:0"});
    ASSERT_EQ(active.size(), 1);
    ASSERT_TRUE(active[0].result_.eligible_);
    ASSERT_GT(std::abs(joint.buckets_[0].dPvDDecimalQuote_), 1.0);
    market.curveComponents_["spread"] = Flat(spec, "spread", "USD", 0.015);
    RateCashflowPricingInternal::g_nodeSensitivitySweepCount = 0;
    const auto zero = Risk(spec, calibrated, market, {trade});
    ASSERT_TRUE(zero.meta_[0].structuralZero_);
    ASSERT_EQ(RateCashflowPricingInternal::g_nodeSensitivitySweepCount, 0);
}

TEST(JointQuoteRiskTest, TestInvalidSourceGuardsUnregisteredXccyPathBeforePricing) {
    struct CyclicPwc_ : TaggedPwc_ {
        int* calls_;
        CyclicPwc_(const Tape::DiscountPWC_<double>& curve, int* calls) : TaggedPwc_(curve), calls_(calls) {}
        void SetBase(const Handle_<DiscountCurve_>& base) { base_ = base; }
        double operator()(const Date_&, const Date_&) const override {
            ++*calls_;
            THROW("Unsafe cyclic path was evaluated");
        }
    };
    const auto spec = JointQuoteRiskFixtures::Spec();
    const auto calibrated = CalibrateJointMultiCurve(spec, Options());
    auto market = Market(spec, calibrated);
    auto trade = Trade(spec);
    std::get<XccyTradeTerms_>(trade.terms_).config_.convention_.foreignIndex_.collateral_ = CollateralType_("GC");
    const auto rebuild = [&](const Handle_<DiscountCurve_>& base) {
        auto discounts = market.xccyMarket_->ForeignBlock().DiscountCurves();
        discounts[CollateralType_("GC")] = Flat(spec, "independent-discount", "USD", 0.02);
        auto forwards = market.xccyMarket_->ForeignBlock().ForwardCurves();
        forwards[PeriodLength_("6M")] = Flat(spec, "usd-6m", "USD", 0.015, base);
        const Handle_<CurveBlock_> foreign(new CurveBlock_("USD", "USD", discounts, forwards, spec.liborBasis_));
        const auto& domestic = market.xccyMarket_->DomesticBlock();
        const Handle_<CurveBlock_> domesticHandle(
            new CurveBlock_("EUR", "EUR", domestic.DiscountCurves(), domestic.ForwardCurves(), DayBasis::Act365F()));
        auto xccy = std::make_shared<CrossCurrencyMarket_>(domesticHandle, foreign, 0.9, market.valuationTime_, Ccy_("EUR"), market.fixings_);
        xccy->SetBasisCurve(market.curveComponents_.at("basis"));
        market.xccyMarket_ = xccy;
    };
    rebuild(market.curveComponents_.at("curve:0"));
    const auto provenance = BuildJointMultiCurveQuoteRiskProvenance(spec, calibrated, Options(), market, JointQuoteRiskFixtures::Config(2));
    int calls = 0;
    const auto cycle = std::make_shared<CyclicPwc_>(dynamic_cast<const Tape::DiscountPWC_<double>&>(*market.curveComponents_.at("curve:0")), &calls);
    const Handle_<DiscountCurve_> handle(cycle);
    cycle->SetBase(handle);
    struct ClearCycle_ {
        CyclicPwc_* curve_;
        ~ClearCycle_() { curve_->SetBase({}); }
    } clear{cycle.get()};
    market.curveComponents_["curve:0"] = handle;
    rebuild(handle);
    RateCashflowPricingInternal::g_nodeSensitivitySweepCount = 0;
    const auto risk = AggregateRatePortfolioQuoteRisk({trade}, market, {provenance});
    ASSERT_EQ(risk.provenanceFailures_.size(), 1);
    ASSERT_EQ(risk.provenanceFailures_[0].actualStateFingerprint_, "INVALID");
    ASSERT_EQ(calls, 0);
    ASSERT_TRUE(risk.meta_.empty());
    ASSERT_TRUE(risk.buckets_.empty());
    ASSERT_EQ(RateCashflowPricingInternal::g_nodeSensitivitySweepCount, 0);
}

namespace {
    thread_local int recordedFaultKind = 0;
    thread_local int completedSweeps = 0;
    thread_local const DiscountCurve_* liveFaultTarget = nullptr;
    thread_local int liveFaultNodes = 0;

    void FailWithLiveTape(const DiscountCurve_* target) {
        if (target != liveFaultTarget)
            return;
#if DAL_RATE_RISK_NATIVE_AAD
        liveFaultNodes = AAD::Tape()->nodes_.Size();
#endif
        THROW("Joint test failure with recorded nodes still live");
    }

    void FailRecordedSweep(const String_& key, RateTradeNodeSensitivityResult_* cell) {
        ++completedSweeps;
        if (key != "curve:1")
            return;
        REQUIRE(cell->eligible_ && !cell->gradient_.empty(), "Fault must follow a completed AAD sweep");
        if (recordedFaultKind == 0)
            THROW("QUOTE_RISK_CYCLIC_CURVE_GRAPH");
        if (recordedFaultKind == 1)
            cell->gradient_[0] = std::numeric_limits<double>::quiet_NaN();
        else
            cell->gradient_.push_back(0.0);
    }
} // namespace

TEST(JointQuoteRiskTest, TestPostRecordingFailuresDiscardEarlierSlicesAndRestoreTape) {
    const auto spec = JointQuoteRiskFixtures::Spec(5, 2, CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD, true);
    const auto calibrated = CalibrateJointMultiCurve(spec, Options());
    const auto market = JointQuoteRiskFixtures::Market(spec, calibrated);
    const auto trade = JointQuoteRiskFixtures::Irs(spec);
    const auto healthy = Future(spec, "curve:0");
    const auto expected = Risk(spec, calibrated, market, {healthy});
    for (int kind = 0; kind < 3; ++kind) {
        recordedFaultKind = kind;
        completedSweeps = 0;
        struct FaultScope_ {
            FaultScope_() { RateCashflowPricingInternal::g_quoteRiskRecordedSweepHook = FailRecordedSweep; }
            ~FaultScope_() { RateCashflowPricingInternal::g_quoteRiskRecordedSweepHook = nullptr; }
        } fault;
#if DAL_RATE_RISK_NATIVE_AAD
        const auto initialSize = AAD::Tape()->nodes_.Size();
        std::atomic<int> recordedSize{0};
        RateCashflowPricingInternal::NodeSensitivityTapeSizeObservation_ observation(recordedSize);
#endif
        const auto failed = Risk(spec, calibrated, market, {trade});
        AssertIncomplete(failed, trade, market, "AAD_EVALUATION_FAILED", "curve:1");
        ASSERT_EQ(completedSweeps, 2);
        const auto mixed = Risk(spec, calibrated, market, {trade, healthy});
        ASSERT_EQ(mixed.buckets_.size(), expected.buckets_.size());
        for (int i = 0; i < static_cast<int>(mixed.buckets_.size()); ++i)
            ASSERT_DOUBLE_EQ(mixed.buckets_[i].dPvDDecimalQuote_, expected.buckets_[i].dPvDDecimalQuote_);
        ASSERT_DOUBLE_EQ(mixed.pvByActualPvCcy_.at("USD"), PriceRateTrade(trade, market).pv_ + PriceRateTrade(healthy, market).pv_);
#if DAL_RATE_RISK_NATIVE_AAD
        ASSERT_GT(recordedSize.load(), initialSize + 5);
        ASSERT_EQ(AAD::Tape()->nodes_.Size(), initialSize);
#endif
    }
    const auto expectedHealthy = Risk(spec, calibrated, market, {trade});
    auto first = std::async(std::launch::async, [&]() { return Risk(spec, calibrated, market, {trade}); });
    auto second = std::async(std::launch::async, [&]() { return Risk(spec, calibrated, market, {trade}); });
    for (const auto& result : {first.get(), second.get()}) {
        ASSERT_TRUE(result.meta_[0].eligible_);
        for (int i = 0; i < 5; ++i)
            ASSERT_DOUBLE_EQ(result.buckets_[i].dPvDDecimalQuote_, expectedHealthy.buckets_[i].dPvDDecimalQuote_);
    }
}

TEST(JointQuoteRiskTest, TestLiveRecordingExceptionRewindsBeforeUnregisteredXccyReuse) {
    const auto spec = JointQuoteRiskFixtures::Spec(5, 2, CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD, true);
    const auto calibrated = CalibrateJointMultiCurve(spec, Options());
    const auto market = Market(spec, calibrated);
    const auto provenance = BuildJointMultiCurveQuoteRiskProvenance(spec, calibrated, Options(), market, JointQuoteRiskFixtures::Config(2));
    const auto expected = AggregateRatePortfolioQuoteRisk({Trade(spec)}, market, {provenance});
#if DAL_RATE_RISK_NATIVE_AAD
    const auto initialSize = AAD::Tape()->nodes_.Size();
#endif
    {
        liveFaultTarget = market.curveComponents_.at("curve:1").get();
        liveFaultNodes = 0;
        struct FaultScope_ {
            FaultScope_() { RateCashflowPricingInternal::g_jointNodeSensitivityRecordedPvHook = FailWithLiveTape; }
            ~FaultScope_() { RateCashflowPricingInternal::g_jointNodeSensitivityRecordedPvHook = nullptr; }
        } fault;
        const auto trade = JointQuoteRiskFixtures::Irs(spec);
        const auto failed = AggregateRatePortfolioQuoteRisk({trade}, market, {provenance});
        AssertIncomplete(failed, trade, market, "AAD_EVALUATION_FAILED", "curve:1");
#if DAL_RATE_RISK_NATIVE_AAD
        ASSERT_GT(liveFaultNodes, initialSize + 5);
        ASSERT_EQ(AAD::Tape()->nodes_.Size(), initialSize);
#endif
    }
    const auto recovered = AggregateRatePortfolioQuoteRisk({Trade(spec)}, market, {provenance});
    ASSERT_TRUE(recovered.meta_[0].eligible_);
    for (int i = 0; i < 5; ++i)
        ASSERT_DOUBLE_EQ(recovered.buckets_[i].dPvDDecimalQuote_, expected.buckets_[i].dPvDDecimalQuote_);
}

TEST(JointQuoteRiskTest, TestUnusedLayeredBlockIsNeverPrepared) {
    const auto spec = JointQuoteRiskFixtures::Spec(5, 3, CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD, true);
    const auto calibrated = CalibrateJointMultiCurve(spec, Options());
    const auto market = JointQuoteRiskFixtures::Market(spec, calibrated);
    const auto provenance = BuildJointMultiCurveQuoteRiskProvenance(spec, calibrated, Options(), market, JointQuoteRiskFixtures::Config(3));
    RateCashflowPricingInternal::g_nodeSensitivityPreparationCount = 0;
    RateCashflowPricingInternal::g_nodeSensitivitySweepCount = 0;
    const auto risk = AggregateRatePortfolioQuoteRisk({JointQuoteRiskFixtures::Irs(spec)}, market, {provenance});
    ASSERT_TRUE(risk.meta_[0].eligible_);
    ASSERT_EQ(risk.buckets_.size(), 5);
    ASSERT_EQ(RateCashflowPricingInternal::g_nodeSensitivityPreparationCount, 2);
    ASSERT_EQ(RateCashflowPricingInternal::g_nodeSensitivitySweepCount, 2);
}

TEST(JointQuoteRiskTest, TestRootValidationGraphAndExpiryGateOrder) {
    const auto spec = JointQuoteRiskFixtures::Spec();
    const auto calibrated = CalibrateJointMultiCurve(spec, Options());
    for (bool opaqueRoot : {false, true}) {
        auto market = Market(spec, calibrated, false, {}, [&](const auto&) {
            const Handle_<DiscountCurve_> opaque(new OpaqueCurve_({}, "EUR"));
            return opaqueRoot ? opaque : Flat(spec, "eur-3m", "EUR", 0.019, opaque);
        });
        auto trade = Trade(spec);
        auto badFamily = trade;
        badFamily.instrumentType_ = RateInstrumentType_::Value_::DEPOSIT;
        AssertIncomplete(Risk(spec, calibrated, market, {badFamily}), badFamily, market, "TRADE_FAMILY_NOT_AAD_ENABLED");
        market.valuationTime_ = DateTime_(spec.today_, 12, 0);
        AssertIncomplete(Risk(spec, calibrated, market, {trade}), trade, market, opaqueRoot ? "AAD_EVALUATION_FAILED" : "TRADE_VALIDATION_FAILED");
        market.valuationTime_ = DateTime_(spec.today_);
        AssertIncomplete(Risk(spec, calibrated, market, {trade}), trade, market);
        trade.startDate_ = Date::AddMonths(spec.today_, -24);
        trade.maturityDate_ = spec.today_.AddDays(-1);
        const auto expired = Risk(spec, calibrated, market, {trade});
        if (opaqueRoot)
            AssertIncomplete(expired, trade, market);
        else {
            ASSERT_TRUE(expired.meta_[0].eligible_);
            ASSERT_DOUBLE_EQ(expired.meta_[0].pv_, 0.0);
        }
        market.xccyMarket_.reset();
        const auto unresolved = Risk(spec, calibrated, market, {trade});
        ASSERT_TRUE(unresolved.meta_[0].eligible_);
        ASSERT_TRUE(unresolved.meta_[0].structuralZero_);
        ASSERT_EQ(RateTradeNodeSensitivities(trade, market, "curve:0").reason_, "TRADE_DOES_NOT_DEPEND_ON_COMPONENT");
        trade = Trade(spec);
        AssertIncomplete(Risk(spec, calibrated, market, {trade}), trade, market, "TRADE_VALIDATION_FAILED");
    }
}

TEST(JointQuoteRiskTest, TestUnknownGraphUsesDeclarationOrderAndInactiveSourcesStayInactive) {
    const auto spec = JointQuoteRiskFixtures::Spec();
    const auto calibrated = CalibrateJointMultiCurve(spec, Options());
    auto market = JointQuoteRiskFixtures::Market(spec, calibrated);
    market.curveComponents_["z-discount"] = market.curveComponents_.at("curve:0");
    market.curveComponents_["a-forward"] = market.curveComponents_.at("curve:1");
    market.curveComponents_["opaque"] = Flat(spec, "opaque-root", "USD", 0.0, Handle_<DiscountCurve_>(new OpaqueCurve_()));
    auto trade = JointQuoteRiskFixtures::Irs(spec);
    auto& terms = std::get<IrsTradeTerms_>(trade.terms_).value_;
    terms.discountComponentKey_ = "opaque";
    terms.forecastComponentKey_ = "a-forward";
    const RateQuoteRiskProvenanceConfig_ config{"generic-joint", {{"curve:0", "z-discount"}, {"curve:1", "a-forward"}}};
    const auto provenance = BuildJointMultiCurveQuoteRiskProvenance(spec, calibrated, Options(), market, config);
    AssertIncomplete(AggregateRatePortfolioQuoteRisk({trade}, market, {provenance}), trade, market, "AAD_EVALUATION_FAILED", "z-discount");
    market.valuationTime_ = DateTime_(spec.today_.AddDays(1));
    RateCashflowPricingInternal::g_nodeSensitivitySweepCount = 0;
    const auto stale = AggregateRatePortfolioQuoteRisk({trade}, market, {provenance});
    ASSERT_EQ(stale.provenanceFailures_.size(), 1);
    ASSERT_TRUE(stale.meta_.empty());
    ASSERT_EQ(RateCashflowPricingInternal::g_nodeSensitivitySweepCount, 0);
}

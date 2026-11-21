#include <gtest/gtest.h>

#include <array>
#include <iomanip>
#include <sstream>

#include <dal/curve/calibration_internal.hpp>
#include <dal/curve/quoteriskaggregation.hpp>
#include <dal/curve/ratecashflowpricing_internal.hpp>
#include <dal/curve/ycinstrument.hpp>

using namespace Dal;

namespace {
    constexpr double NOTIONAL = 1'000'000.;
    const Date_ TODAY(2024, 1, 15);
    const RateQuoteRiskProvenanceConfig_ CONFIG{"EUR-calibration", {{"stage:0", "ESTR"}, {"stage:1", "Euribor6M"}}};

    MultiCurveCalibrationSpec_ Spec(int bumpedStage = -1, int bumpedQuote = -1, double bump = 0.) {
        MultiCurveCalibrationSpec_ spec;
        spec.ccy_ = "EUR";
        for (int s = 0; s < 2; ++s) {
            CurveCalibrationSpec_ stage;
            stage.curveName_ = s == 0 ? "ESTR" : "Euribor6M";
            stage.today_ = TODAY;
            stage.calibrateDiscountCurve_ = s == 0;
            stage.parameterization_ = CurveParameterization_::Value_::ZERO_RATE;
            stage.knotPolicy_ = CurveKnotPolicy_::Value_::INSTRUMENTS;
            stage.tolerance_ = s == 0 ? 1.e-13 : 5.e-13;
            stage.initialGuess_ = s == 0 ? 0.02 : 0.01;
            RateIndexConvention_ index;
            RateLegConvention_ leg;
            leg.paymentFrequency_ = PeriodLength_("6M");
            if (s == 1) {
                stage.targetTenor_ = leg.paymentFrequency_;
                index.forecastTenor_ = stage.targetTenor_;
                index.useProjectionCurve_ = true;
            }
            // Long quote first so that input ordinals differ from calibration rows.
            for (int q = 0; q < 2; ++q) {
                const auto maturity = Date::AddMonths(TODAY, q == 0 ? 24 : 6);
                const double rate = 0.035 - q * 0.014 + s * 0.007 + (s == bumpedStage && q == bumpedQuote ? bump : 0.);
                if (s == 0)
                    stage.instruments_.emplace_back(new Deposit_(TODAY, TODAY, maturity, rate, index));
                else
                    stage.instruments_.emplace_back(new Swap_(TODAY, TODAY, maturity, rate, leg, index, leg));
            }
            spec.stages_.push_back(stage);
        }
        return spec;
    }

    RatePricingMarket_ Market(const MultiCurveCalibrationResult_& fitted) {
        RatePricingMarket_ market;
        market.valuationTime_ = DateTime_(TODAY, 10, 0);
        market.resultCurrency_ = Ccy_("USD"); // Portfolio buckets use the trade's actual PV currency.
        market.curveComponents_ = {{"ESTR", fitted.discountCurves_.begin()->second}, {"Euribor6M", fitted.forwardCurves_.at(PeriodLength_("6M"))}};
        return market;
    }

    RateTradeDefinition_ SwapTrade(bool payFixed) {
        IrsTradeTerms_ terms;
        auto& swap = terms.value_;
        swap.notional_ = payFixed ? NOTIONAL : 0.4 * NOTIONAL;
        swap.payFixed_ = payFixed;
        swap.contractRate_ = payFixed ? 0.031 : 0.048;
        swap.floatLeg_.paymentFrequency_ = PeriodLength_("6M");
        swap.fixedLeg_.paymentFrequency_ = PeriodLength_("6M");
        swap.floatIndex_.forecastTenor_ = PeriodLength_("6M");
        swap.floatIndex_.useProjectionCurve_ = true;
        swap.fixingIdentity_ = {"Euribor6M", 11, 0};
        swap.forecastComponentKey_ = "Euribor6M";
        swap.discountComponentKey_ = "ESTR";
        return {payFixed ? "payer" : "receiver",
                RateInstrumentType_::Value_::IRS,
                TODAY,
                Date::AddMonths(TODAY, 1),
                Date::AddMonths(TODAY, payFixed ? 19 : 13),
                Ccy_("EUR"),
                terms};
    }
} // namespace

TEST(StagedPortfolioQuoteRiskTest, TestSwapBucketsUseInputOrderAndPhysicalUnits) {
    StagedMultiCurveCalibrationRecord_ record;
    const auto market = Market(CalibrateMultiCurve(Spec(), &record));
    ASSERT_TRUE(record.unavailableReason_.empty()) << record.unavailableReason_;
    const auto provenance = BuildStagedMultiCurveQuoteRiskProvenance(record, market, CONFIG);
    ASSERT_NE(provenance.StagedMapping(), nullptr);
    ASSERT_THROW((void)provenance.EffectiveInverse(), Exception_);
    ASSERT_THROW((void)provenance.Tolerance(), Exception_);
    const auto risk = AggregateRatePortfolioQuoteRisk({SwapTrade(true)}, market, {provenance});
    ASSERT_TRUE(risk.provenanceFailures_.empty());
    ASSERT_EQ(risk.meta_.size(), 1u);
    ASSERT_TRUE(risk.meta_[0].eligible_) << risk.meta_[0].originalNodeRiskReason_;
    ASSERT_EQ(risk.buckets_.size(), 4u);
    for (int i = 0; i < 4; ++i) {
        const auto& bucket = risk.buckets_[i];
        ASSERT_EQ(bucket.quoteKey_, "stage:" + String::FromInt(i / 2) + ":" + String::FromInt(i % 2));
        ASSERT_EQ(bucket.actualPvCcy_, Ccy_("EUR"));
        ASSERT_EQ(bucket.dv01_, 1.e-4 * bucket.dPvDDecimalQuote_);
    }
}

TEST(StagedPortfolioQuoteRiskTest, TestRejectsRecalibratedHandlesEvenWithIdenticalValues) {
    StagedMultiCurveCalibrationRecord_ record;
    const auto market = Market(CalibrateMultiCurve(Spec(), &record));
    const auto source = BuildStagedMultiCurveQuoteRiskProvenance(record, market, CONFIG);
    const auto refitted = Market(CalibrateMultiCurve(Spec()));
    ASSERT_THROW(BuildStagedMultiCurveQuoteRiskProvenance(record, refitted, CONFIG), Exception_);
    const auto risk = AggregateRatePortfolioQuoteRisk({SwapTrade(true)}, refitted, {source});
    ASSERT_TRUE(risk.buckets_.empty());
    ASSERT_EQ(risk.provenanceFailures_.size(), 1u);
    ASSERT_EQ(risk.provenanceFailures_[0].calibrationId_, CONFIG.calibrationId_);
    ASSERT_EQ(risk.provenanceFailures_[0].componentKey_, "ESTR");
}

TEST(StagedPortfolioQuoteRiskTest, TestStaleMarketStateLocatesSourceBeforeSweeps) {
    StagedMultiCurveCalibrationRecord_ record;
    const auto market = Market(CalibrateMultiCurve(Spec(), &record));
    const auto source = BuildStagedMultiCurveQuoteRiskProvenance(record, market, CONFIG);
    for (bool changeFixing : {true, false}) {
        auto changed = market;
        if (changeFixing) {
            MarketFixingSnapshot_::values_t values;
            values["Euribor6M"][DateTime_(TODAY.AddDays(-1), 11, 0)] = 0.035;
            changed.fixings_ = Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_(values));
        } else {
            changed.valuationTime_ = DateTime_(TODAY, 10, 1);
        }
        const int before = RateCashflowPricingInternal::g_nodeSensitivitySweepCount.load();
        const auto risk = AggregateRatePortfolioQuoteRisk({SwapTrade(true)}, changed, {source});
        ASSERT_EQ(RateCashflowPricingInternal::g_nodeSensitivitySweepCount.load(), before);
        ASSERT_TRUE(risk.buckets_.empty());
        ASSERT_EQ(risk.provenanceFailures_.size(), 1u);
        const auto& failure = risk.provenanceFailures_[0];
        ASSERT_EQ(failure.calibrationId_, CONFIG.calibrationId_);
        ASSERT_EQ(failure.reason_, "QUOTE_RISK_CALIBRATION_STATE_MISMATCH");
        ASSERT_NE(failure.expectedStateFingerprint_, failure.actualStateFingerprint_);
    }
}

TEST(StagedPortfolioQuoteRiskTest, TestStageMustDescribeTheBoundSolutionAndForwardBase) {
    StagedMultiCurveCalibrationRecord_ original;
    const auto market = Market(CalibrateMultiCurve(Spec(), &original));
    for (int i = 0; i < 3; ++i) {
        auto record = original;
        auto& stage = record.stages_[1];
        if (i == 0)
            stage.solution_[0] += 1.e-4;
        else if (i == 1)
            std::swap(stage.freeParameters_[0], stage.freeParameters_[1]);
        else
            stage.spec_.discountCurves_.clear();
        ASSERT_THROW(BuildStagedMultiCurveQuoteRiskProvenance(record, market, CONFIG), Exception_);
    }
}

TEST(StagedPortfolioQuoteRiskTest, TestMissingForwardGradientDiscardsOnlyAffectedTrade) {
    StagedMultiCurveCalibrationRecord_ record;
    const auto market = Market(CalibrateMultiCurve(Spec(), &record));
    const auto source = BuildStagedMultiCurveQuoteRiskProvenance(record, market, CONFIG);
    DepositTradeTerms_ terms;
    terms.notional_ = NOTIONAL;
    terms.contractRate_ = 0.02;
    terms.discountComponentKey_ = "ESTR";
    const RateTradeDefinition_ deposit{"deposit", RateInstrumentType_::Value_::DEPOSIT, TODAY, TODAY, Date::AddMonths(TODAY, 8), Ccy_("EUR"), terms};
    const auto healthy = AggregateRatePortfolioQuoteRisk({deposit}, market, {source});
    ASSERT_EQ(healthy.buckets_.size(), 4u);
    struct MissingGradient_ {
        MissingGradient_() {
            RateCashflowPricingInternal::g_quoteRiskRecordedSweepHook = [](const String_& key, RateTradeNodeSensitivityResult_* cell) {
                if (key == "Euribor6M")
                    cell->gradient_.clear();
            };
        }
        ~MissingGradient_() { RateCashflowPricingInternal::g_quoteRiskRecordedSweepHook = nullptr; }
    } missing;
    const auto mixed = AggregateRatePortfolioQuoteRisk({SwapTrade(true), deposit}, market, {source});
    ASSERT_EQ(mixed.meta_.size(), 2u);
    const auto& failure = mixed.meta_[0];
    ASSERT_FALSE(failure.eligible_);
    ASSERT_EQ(failure.instrumentId_, "payer");
    ASSERT_EQ(failure.calibrationId_, CONFIG.calibrationId_);
    ASSERT_EQ(failure.failingComponentKey_, "Euribor6M");
    ASSERT_EQ(failure.originalNodeRiskReason_, "AAD_EVALUATION_FAILED");
    ASSERT_TRUE(mixed.meta_[1].eligible_);
    ASSERT_EQ(mixed.buckets_.size(), healthy.buckets_.size());
    for (size_t i = 0; i < mixed.buckets_.size(); ++i)
        ASSERT_EQ(mixed.buckets_[i].dPvDDecimalQuote_, healthy.buckets_[i].dPvDDecimalQuote_);
}

TEST(StagedPortfolioQuoteRiskTest, TestUnavailableCalibrationPreservesStageReasonAndPassivePv) {
    auto spec = Spec();
    spec.stages_[1].solveMode_ = CurveSolveMode_::Value_::APPROXIMATE;
    StagedMultiCurveCalibrationRecord_ record;
    const auto market = Market(CalibrateMultiCurve(spec, &record));
    ASSERT_FALSE(record.unavailableReason_.empty());
    const auto source = BuildStagedMultiCurveQuoteRiskProvenance(record, market, CONFIG);
    const auto trade = SwapTrade(true);
    const auto risk = AggregateRatePortfolioQuoteRisk({trade}, market, {source});
    ASSERT_EQ(risk.provenanceFailures_.size(), 1u);
    ASSERT_EQ(risk.provenanceFailures_[0].reason_, record.unavailableStage_ + " " + record.unavailableReason_);
    ASSERT_EQ(risk.pvByActualPvCcy_.at("EUR"), PriceRateTrade(trade, market).pv_);
    ASSERT_TRUE(risk.buckets_.empty());
}

namespace {
    using Prices_ = std::array<double, 3>;

    Prices_ Prices(const Vector_<RateTradeDefinition_>& trades, const RatePricingMarket_& market) {
        Prices_ pv{};
        for (int i = 0; i < 2; ++i) {
            const auto result = PriceRateTrade(trades[i], market);
            REQUIRE(result.succeeded_ && std::isfinite(result.pv_), result.error_);
            pv[i] = result.pv_;
        }
        pv[2] = pv[0] + pv[1];
        return pv;
    }

    struct BumpedPrices_ {
        Prices_ full_;
        Prices_ frozenSpread_{};
        Prices_ frozenCurve_{};
    };

    BumpedPrices_ Reprice(const Vector_<RateTradeDefinition_>& trades, const CurveCalibrationStageRecord_& forward, int stage, int quote, double h) {
        auto bumped = Spec(stage, quote, h);
        const auto fit = CalibrateMultiCurve(bumped);
        for (const auto& diagnostics : fit.diagnostics_)
            REQUIRE(diagnostics.maxAbsResidual_ <= 1.e-12, "Bumped calibration residual exceeds tolerance");
        auto market = Market(fit);
        BumpedPrices_ pv{Prices(trades, market)};
        if (stage == 0) {
            // Refit only the discount stage. Compare a frozen outright curve and frozen native spread on the new base.
            bumped.stages_.Resize(1);
            market.curveComponents_["ESTR"] = CalibrateMultiCurve(bumped).discountCurves_.begin()->second;
            market.curveComponents_["Euribor6M"] = forward.curve_;
            pv.frozenCurve_ = Prices(trades, market);
            const auto& spec = forward.spec_;
            const auto def =
                MakeCurveDefinition(spec.curveName_, spec.ccy_, spec.parameterization_, spec.logDfScheme_, spec.knotDates_, TODAY, spec.liborBasis_);
            market.curveComponents_["Euribor6M"] =
                Handle_<DiscountCurve_>(BuildDiscountCurveUniqueT<double>(def, forward.solution_, market.curveComponents_.at("ESTR")));
            pv.frozenSpread_ = Prices(trades, market);
        }
        return pv;
    }
} // namespace

TEST(StagedPortfolioQuoteRiskTest, TestEveryBucketAgainstTwoFullSequentialRecalibrations) {
    const Vector_<RateTradeDefinition_> trades{SwapTrade(true), SwapTrade(false)};
    StagedMultiCurveCalibrationRecord_ record;
    const int beforeFit = CurveCalibrationInvocationCount();
    const auto market = Market(CalibrateMultiCurve(Spec(), &record));
    ASSERT_EQ(CurveCalibrationInvocationCount() - beforeFit, 2);
    const auto source = BuildStagedMultiCurveQuoteRiskProvenance(record, market, CONFIG);
    const int beforeAggregation = CurveCalibrationInvocationCount();
    const RateCashflowPricingInternal::CashflowLegBuildObservation_ observation;
    const int beforeLegs = RateCashflowPricingInternal::g_rateCashflowLegBuildCount.load();
    const std::array<RatePortfolioQuoteRisk_, 3> risk{AggregateRatePortfolioQuoteRisk({trades[0]}, market, {source}),
                                                      AggregateRatePortfolioQuoteRisk({trades[1]}, market, {source}),
                                                      AggregateRatePortfolioQuoteRisk(trades, market, {source})};
    const int calls = CurveCalibrationInvocationCount() - beforeAggregation;
    RecordProperty("aggregation_calibrations", calls);
    ASSERT_EQ(calls, 0);
    const int legBuilds = RateCashflowPricingInternal::g_rateCashflowLegBuildCount.load() - beforeLegs;
    RecordProperty("aggregation_leg_builds", legBuilds);
    ASSERT_EQ(legBuilds, 8); // Two legs per swap in each separate and combined request.
    for (const auto& result : risk) {
        ASSERT_TRUE(result.provenanceFailures_.empty());
        ASSERT_EQ(result.buckets_.size(), 4u);
        for (const auto& meta : result.meta_)
            ASSERT_TRUE(meta.eligible_) << meta.originalNodeRiskReason_;
    }
    for (int i = 0; i < 4; ++i) {
        const auto& bucket = risk[2].buckets_[i];
        ASSERT_EQ(bucket.actualPvCcy_, Ccy_("EUR"));
        ASSERT_EQ(bucket.quoteKey_, "stage:" + String::FromInt(i / 2) + ":" + String::FromInt(i % 2));
        ASSERT_NEAR(bucket.dPvDDecimalQuote_, risk[0].buckets_[i].dPvDDecimalQuote_ + risk[1].buckets_[i].dPvDDecimalQuote_, 1.e-8);
        for (const double h : {1.e-4, 5.e-5}) {
            const auto up = Reprice(trades, record.stages_[1], i / 2, i % 2, h);
            const auto down = Reprice(trades, record.stages_[1], i / 2, i % 2, -h);
            const double width = 2. * h * NOTIONAL;
            for (int k = 0; k < 3; ++k) {
                SCOPED_TRACE(::testing::Message() << bucket.quoteKey_ << " trade set " << k << " h=" << h);
                const double reference = (up.full_[k] - down.full_[k]) / width;
                const double tolerance = 1.e-7 + 1.e-5 * std::abs(reference);
                const double analytic = risk[k].buckets_[i].dPvDDecimalQuote_ / NOTIONAL;
                ASSERT_NEAR(analytic, reference, tolerance);
                std::ostringstream row;
                row << std::setprecision(12) << analytic << " " << reference;
                if (i < 2) {
                    const double frozenSpread = (up.frozenSpread_[k] - down.frozenSpread_[k]) / width;
                    const double frozenCurve = (up.frozenCurve_[k] - down.frozenCurve_[k]) / width;
                    ASSERT_GT(std::abs(frozenSpread - reference), tolerance);
                    ASSERT_GT(std::abs(frozenCurve - reference), tolerance);
                    row << " " << frozenSpread << " " << frozenCurve;
                }
                RecordProperty("bucket_" + std::to_string(i) + "_set_" + std::to_string(k) + (h == 1.e-4 ? "_1bp" : "_half_bp"), row.str());
            }
        }
    }
}

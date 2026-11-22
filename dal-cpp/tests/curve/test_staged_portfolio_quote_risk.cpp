#include <gtest/gtest.h>

#include <array>
#include <iomanip>
#include <sstream>

#include <dal/curve/calibration_internal.hpp>
#include <dal/curve/piecewiseconstant.hpp>
#include <dal/curve/quoteriskaggregation.hpp>
#include <dal/curve/quoteriskmapping.hpp>
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
            stage.today_ = TODAY;
            stage.calibrateDiscountCurve_ = s == 0;
            stage.parameterization_ = CurveParameterization_::Value_::ZERO_RATE;
            stage.knotPolicy_ = CurveKnotPolicy_::Value_::INSTRUMENTS;
            RateIndexConvention_ index;
            RateLegConvention_ leg;
            leg.paymentFrequency_ = PeriodLength_("6M");
            if (stage.calibrateDiscountCurve_) {
                stage.curveName_ = "ESTR";
                stage.tolerance_ = 1.e-13;
                stage.initialGuess_ = 0.02;
            } else {
                stage.curveName_ = "Euribor6M";
                stage.tolerance_ = 5.e-13;
                stage.initialGuess_ = 0.01;
                stage.targetTenor_ = leg.paymentFrequency_;
                index.forecastTenor_ = stage.targetTenor_;
                index.useProjectionCurve_ = true;
            }
            // Long quote first so that input ordinals differ from calibration rows.
            for (int q = 0; q < 2; ++q) {
                const auto maturity = Date::AddMonths(TODAY, q == 0 ? 24 : 6);
                const double rate = 0.035 - q * 0.014 + s * 0.007 + (s == bumpedStage && q == bumpedQuote ? bump : 0.);
                if (stage.calibrateDiscountCurve_)
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

    RateTradeDefinition_ XccyTrade() {
        const auto swap = std::get<IrsTradeTerms_>(SwapTrade(true).terms_).value_;
        XccyTradeTerms_ terms;
        terms.positionCount_ = 1.;
        auto& config = terms.config_;
        config.pair_ = CurrencyPair_(Ccy_("EUR"), Ccy_("USD"));
        config.domesticNotional_ = 900000.;
        config.foreignNotional_ = NOTIONAL;
        config.convention_.domesticLeg_ = config.convention_.foreignLeg_ = swap.floatLeg_;
        config.convention_.domesticIndex_ = config.convention_.foreignIndex_ = swap.floatIndex_;
        config.domesticRateFixing_ = {"Euribor6M", 10, 0};
        config.foreignRateFixing_ = {"USD6M", 11, 0};
        return {"EUR-USD", RateInstrumentType_::Value_::XCCY, TODAY, TODAY, Date::AddMonths(TODAY, 18), Ccy_("EUR"), terms};
    }

    RatePricingMarket_ XccyMarket(const MultiCurveCalibrationResult_& fitted,
                                  const Handle_<DiscountCurve_>& routedForward,
                                  const Handle_<MarketFixingSnapshot_>& fixings = {}) {
        auto market = Market(fitted);
        const auto tenor = PeriodLength_("6M");
        const Handle_<CurveBlock_> eur(new CurveBlock_("EUR", "EUR", fitted.discountCurves_, {{tenor, routedForward}}, DayBasis::Act365F()));
        const Handle_<DiscountCurve_> usd(NewDiscountPWC("USD", "USD", PiecewiseConstant_({TODAY}, {0.04})));
        const Handle_<CurveBlock_> dollar(new CurveBlock_("USD", "USD", {{CollateralType_("OIS"), usd}}, {{tenor, usd}}, DayBasis::Act365F()));
        market.curveComponents_["USD"] = usd;
        market.xccyMarket_ = std::make_shared<CrossCurrencyMarket_>(eur, dollar, 0.9, market.valuationTime_, Ccy_("EUR"), fixings);
        return market;
    }

    void AssertXccyTradeIsolated(const RatePricingMarket_& market, const RateQuoteRiskProvenance_& source) {
        const auto xccy = XccyTrade(), healthy = SwapTrade(true);
        const auto price = PriceRateTrade(xccy, market);
        ASSERT_TRUE(price.succeeded_) << price.error_;
        const int before = RateCashflowPricingInternal::g_nodeSensitivitySweepCount.load();
        const auto single = AggregateRatePortfolioQuoteRisk({xccy}, market, {source});
        ASSERT_EQ(RateCashflowPricingInternal::g_nodeSensitivitySweepCount.load(), before);
        ASSERT_EQ(single.meta_.size(), 1u);
        const auto& rejection = single.meta_[0];
        ASSERT_EQ(rejection.instrumentId_, xccy.instrumentId_);
        ASSERT_EQ(rejection.calibrationId_, source.CalibrationId());
        ASSERT_FALSE(rejection.eligible_);
        ASSERT_FALSE(rejection.structuralZero_);
        ASSERT_EQ(rejection.originalNodeRiskReason_, "STAGED_MULTI_CURVE_XCCY_TRADE_UNSUPPORTED");
        ASSERT_EQ(rejection.pv_, price.pv_);
        ASSERT_TRUE(single.buckets_.empty());
        ASSERT_TRUE(single.provenanceFailures_.empty());
        const auto expected = AggregateRatePortfolioQuoteRisk({healthy}, market, {source});
        const auto mixed = AggregateRatePortfolioQuoteRisk({xccy, healthy}, market, {source});
        ASSERT_EQ(mixed.meta_.size(), 2u);
        ASSERT_EQ(mixed.meta_[0].originalNodeRiskReason_, rejection.originalNodeRiskReason_);
        ASSERT_TRUE(mixed.meta_[1].eligible_);
        ASSERT_TRUE(mixed.provenanceFailures_.empty());
        ASSERT_EQ(expected.buckets_.size(), 4u);
        ASSERT_EQ(mixed.buckets_.size(), expected.buckets_.size());
        for (size_t i = 0; i < expected.buckets_.size(); ++i) {
            ASSERT_EQ(mixed.buckets_[i].quoteKey_, expected.buckets_[i].quoteKey_);
            ASSERT_EQ(mixed.buckets_[i].dPvDDecimalQuote_, expected.buckets_[i].dPvDDecimalQuote_);
            ASSERT_EQ(mixed.buckets_[i].dv01_, expected.buckets_[i].dv01_);
        }
    }
} // namespace

TEST(StagedPortfolioQuoteRiskTest, TestXccyForwardSlotCanDifferFromRegisteredComponent) {
    StagedMultiCurveCalibrationRecord_ record;
    const auto fitted = CalibrateMultiCurve(Spec(), &record);
    const auto other = CalibrateMultiCurve(Spec());
    const auto market = XccyMarket(fitted, other.forwardCurves_.at(PeriodLength_("6M")));
    const auto source = BuildStagedMultiCurveQuoteRiskProvenance(record, market, CONFIG);
    ASSERT_NO_FATAL_FAILURE(AssertXccyTradeIsolated(market, source));
}

TEST(StagedPortfolioQuoteRiskTest, TestXccyFixingFallbackAndExplicitEmptyBothExcludeOnlyXccy) {
    StagedMultiCurveCalibrationRecord_ record;
    const auto fitted = CalibrateMultiCurve(Spec(), &record);
    MarketFixingSnapshot_::values_t values;
    values["Euribor6M"][DateTime_(TODAY, 10, 0)] = 0.06;
    const Handle_<MarketFixingSnapshot_> snapshot(new MarketFixingSnapshot_(values));
    const auto fallback = XccyMarket(fitted, record.stages_[1].curve_, snapshot);
    const auto source = BuildStagedMultiCurveQuoteRiskProvenance(record, fallback, CONFIG);
    auto empty = fallback;
    empty.fixings_ = Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_());
    const auto fixed = PriceRateTrade(XccyTrade(), fallback), projected = PriceRateTrade(XccyTrade(), empty);
    ASSERT_TRUE(fixed.succeeded_) << fixed.error_;
    ASSERT_TRUE(projected.succeeded_) << projected.error_;
    ASSERT_NE(fixed.pv_, projected.pv_);
    ASSERT_NO_FATAL_FAILURE(AssertXccyTradeIsolated(empty, source));
    ASSERT_NO_FATAL_FAILURE(AssertXccyTradeIsolated(fallback, source));
}

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
    const auto check = [&](const StagedMultiCurveCalibrationRecord_& record, const char* reason) {
        try {
            BuildStagedMultiCurveQuoteRiskProvenance(record, market, CONFIG);
            FAIL() << "Expected " << reason;
        } catch (const Exception_& e) {
            ASSERT_NE(std::string(e.what()).find(reason), std::string::npos) << e.what();
        }
    };

    auto wrongSolution = original;
    wrongSolution.stages_[1].solution_[0] += 1.e-4;
    check(wrongSolution, "QUOTE_RISK_STAGED_CAPTURE_MISMATCH: Euribor6M");
    auto wrongOrder = original;
    auto& parameters = wrongOrder.stages_[1].freeParameters_;
    std::swap(parameters[0], parameters[1]);
    check(wrongOrder, "QUOTE_RISK_SPEC_RESULT_MISMATCH: Euribor6M");
    auto missingBase = original;
    missingBase.stages_[1].spec_.discountCurves_.clear();
    check(missingBase, "QUOTE_RISK_BOUND_MARKET_COMPONENT_MISMATCH: Euribor6M");
    auto wrongWidth = original;
    wrongWidth.stages_[1].freeParameters_.pop_back();
    check(wrongWidth, "QUOTE_RISK_PROVENANCE_AXIS_INVALID");
}

TEST(StagedPortfolioQuoteRiskTest, TestSameWidthFactorMustBelongToOriginalCapture) {
    const auto spec = Spec();
    StagedMultiCurveCalibrationRecord_ original;
    const auto market = Market(CalibrateMultiCurve(spec, &original));
    for (const auto& input : {Spec(1, 0, 0.01), spec}) {
        StagedMultiCurveCalibrationRecord_ other;
        CalibrateMultiCurve(input, &other);
        ASSERT_TRUE(other.unavailableReason_.empty()) << other.unavailableReason_;
        for (size_t s = 0; s < 2; ++s) {
            auto mixed = original;
            auto& stage = mixed.stages_[s];
            ASSERT_EQ(stage.solution_.size(), other.stages_[s].solution_.size());
            ASSERT_EQ(stage.residualRowByInput_, other.stages_[s].residualRowByInput_);
            stage.quoteRiskBlock_ = other.stages_[s].quoteRiskBlock_;
            ASSERT_THROW(BuildStagedMultiCurveQuoteRiskProvenance(mixed, market, CONFIG), Exception_);
            const auto response = MapStagedQuoteRisk(mixed, {1., 0., 1., 0.});
            ASSERT_TRUE(response.sensitivities_.empty());
            ASSERT_NE(response.unavailableReason_.find("QUOTE_RISK_STAGED_CAPTURE_MISMATCH"), String_::npos);
        }
    }
}

TEST(StagedPortfolioQuoteRiskTest, TestOriginalQuoteHandlesAndInputOrdinalsBelongToCapture) {
    StagedMultiCurveCalibrationRecord_ original;
    const auto market = Market(CalibrateMultiCurve(Spec(), &original));
    auto reordered = original;
    auto& quotes = reordered.stages_[1].spec_.instruments_;
    std::swap(quotes[0], quotes[1]);
    ASSERT_THROW(BuildStagedMultiCurveQuoteRiskProvenance(reordered, market, CONFIG), Exception_);
    auto replaced = original;
    replaced.stages_[1].spec_.instruments_ = Spec(1, 0, 0.01).stages_[1].instruments_;
    ASSERT_THROW(BuildStagedMultiCurveQuoteRiskProvenance(replaced, market, CONFIG), Exception_);
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

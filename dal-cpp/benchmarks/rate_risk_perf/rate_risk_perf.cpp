//
// Created by dal-implementer on 2026/8/24.
//
// Rate node-risk sweep benchmarks: the long-batch serial sweep (tape memory/latency
// driver — one passive PV per trade, one preparation per component, one AAD sweep per
// (trade, component) cell), the OIS daily-compounding recording shape, and the XCCY
// active-typed assembly shape (frozen P0 contract 8).

#include <algorithm>
#include <cstdio>
#include <memory>

#include <dal/benchmarks/bench.hpp>
#include <dal/curve/calibration_internal.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/piecewiseconstant.hpp>
#include <dal/curve/ratecashflowpricing.hpp>
#include <dal/curve/ratecashflowpricing_internal.hpp>
#include <dal/curve/xccycalibration.hpp>
#include <dal/curve/xccyinstrument.hpp>
#include <dal/indice/fixingsnapshot.hpp>
#include <dal/platform/platform.hpp>
#include <dal/protocol/rateconvention.hpp>

#include "quoteriskbenchfixtures.hpp"

using namespace Dal;

namespace {
    constexpr int kBatchTrades = 120;

    RateIndexConvention_ QuarterlyIndex() {
        RateIndexConvention_ result;
        result.forecastTenor_ = PeriodLength_("3M");
        result.dayBasis_ = DayBasis_("ACT_365F");
        result.collateral_ = CollateralType_(CollateralType_::Value_::OIS);
        return result;
    }

    RateLegConvention_ AnnualLeg() {
        RateLegConvention_ result;
        result.paymentFrequency_ = PeriodLength_("12M");
        result.dayBasis_ = DayBasis_("ACT_365F");
        return result;
    }

    Handle_<DiscountCurve_> KnottedCurve(const String_& name, const Date_& horizon, double flatForward, const String_& ccy = "USD") {
        static const Vector_<Date_> knots{Date_(2026, 7, 15), Date_(2027, 1, 15), Date_(2028, 1, 15), Date_(2029, 1, 15),
                                          Date_(2030, 1, 15), Date_(2031, 1, 15), Date_(2032, 1, 15), Date_(2033, 1, 15)};
        (void)horizon;
        return Handle_<DiscountCurve_>(NewDiscountPWC(name, ccy, PiecewiseConstant_(knots, Vector_<>(knots.size(), flatForward))));
    }

    RatePricingMarket_ Market(const Date_& today, const Date_& horizon) {
        RatePricingMarket_ result;
        result.valuationTime_ = DateTime_(today, 10, 30);
        result.resultCurrency_ = Ccy_("USD");
        result.curveComponents_["discount"] = KnottedCurve("discount", horizon, 0.03);
        result.curveComponents_["forecast"] = KnottedCurve("forecast", horizon, 0.035);
        result.fixings_ = Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_());
        return result;
    }

    FixedFloatTradeTerms_ FixedFloatTerms() {
        FixedFloatTradeTerms_ result;
        result.notional_ = 1'000'000.0;
        result.contractRate_ = 0.03;
        result.payFixed_ = true;
        result.fixedLeg_ = AnnualLeg();
        result.floatLeg_ = AnnualLeg();
        result.floatIndex_ = QuarterlyIndex();
        result.fixingIdentity_ = {"USD-SOFR", 11, 0};
        result.forecastComponentKey_ = "forecast";
        result.discountComponentKey_ = "discount";
        return result;
    }

    RateTradeDefinition_ IrsTrade(const Date_& today, const Date_& start, const Date_& maturity, bool payFixed) {
        FixedFloatTradeTerms_ terms = FixedFloatTerms();
        terms.payFixed_ = payFixed;
        return {"irs", RateInstrumentType_(RateInstrumentType_::Value_::IRS), today, start, maturity, Ccy_("USD"), IrsTradeTerms_{terms}};
    }

    RateTradeDefinition_ OisTrade(const Date_& today, const Date_& start, const Date_& maturity) {
        return {"ois", RateInstrumentType_(RateInstrumentType_::Value_::OIS), today, start, maturity, Ccy_("USD"), OisTradeTerms_{FixedFloatTerms()}};
    }

    RateIndexConvention_ XccyIndex() {
        RateIndexConvention_ result = QuarterlyIndex();
        result.useProjectionCurve_ = true;
        result.forecastTenor_ = PeriodLength_("3M");
        return result;
    }

    XccyTradeTerms_ XccyTerms(bool receiveNonSpread) {
        XccyTradeTerms_ result;
        result.positionCount_ = 1.0;
        result.contractSpread_ = 0.001;
        result.spreadOnForeignLeg_ = true;
        result.receiveNonSpreadPaySpread_ = receiveNonSpread;
        result.config_.pair_ = CurrencyPair_(Ccy_("USD"), Ccy_("EUR"));
        result.config_.domesticNotional_ = 1'000'000.0;
        result.config_.foreignNotional_ = 900'000.0;
        result.config_.convention_.domesticLeg_ = AnnualLeg();
        result.config_.convention_.foreignLeg_ = AnnualLeg();
        result.config_.convention_.domesticIndex_ = XccyIndex();
        result.config_.convention_.foreignIndex_ = XccyIndex();
        result.config_.convention_.spreadOnForeignLeg_ = true;
        result.config_.domesticRateFixing_ = {"USD-INDEX", 11, 0};
        result.config_.foreignRateFixing_ = {"EUR-INDEX", 11, 0};
        return result;
    }

    // Pointer-identity contract: the block slots and the curveComponents_ entries share the same
    // Handles, so every consumed curve is addressable under its key.
    RatePricingMarket_ XccyMarket(const Date_& today, const Date_& horizon) {
        const auto domesticOis = KnottedCurve("domOis", horizon, 0.03);
        const auto domesticFwd = KnottedCurve("domFwd3M", horizon, 0.032);
        const auto foreignOis = KnottedCurve("forOis", horizon, 0.02, "EUR");
        const auto foreignFwd = KnottedCurve("forFwd3M", horizon, 0.022, "EUR");
        const auto basis = KnottedCurve("basis", horizon, 0.001);
        const auto domesticBlock = Handle_<CurveBlock_>(new CurveBlock_("domestic",
                                                                        "USD",
                                                                        {{CollateralType_(CollateralType_::Value_::OIS), domesticOis}},
                                                                        {{PeriodLength_("3M"), domesticFwd}},
                                                                        DayBasis::Act365F()));
        const auto foreignBlock = Handle_<CurveBlock_>(new CurveBlock_("foreign",
                                                                       "EUR",
                                                                       {{CollateralType_(CollateralType_::Value_::OIS), foreignOis}},
                                                                       {{PeriodLength_("3M"), foreignFwd}},
                                                                       DayBasis::Act365F()));
        const auto fixings = Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_());
        auto native = std::make_shared<CrossCurrencyMarket_>(domesticBlock, foreignBlock, 1.2, DateTime_(today, 10, 30), Ccy_("USD"), fixings);
        native->SetBasisCurve(basis);

        RatePricingMarket_ result;
        result.valuationTime_ = DateTime_(today, 10, 30);
        result.resultCurrency_ = Ccy_("USD");
        result.xccyMarket_ = native;
        result.fixings_ = fixings;
        result.curveComponents_["domOis"] = domesticOis;
        result.curveComponents_["domFwd3M"] = domesticFwd;
        result.curveComponents_["forOis"] = foreignOis;
        result.curveComponents_["forFwd3M"] = foreignFwd;
        result.curveComponents_["basis"] = basis;
        return result;
    }

    RateTradeDefinition_ XccyTrade(const Date_& today, const Date_& start, const Date_& maturity, bool receiveNonSpread) {
        return {"xccy", RateInstrumentType_(RateInstrumentType_::Value_::XCCY), today, start, maturity, Ccy_("USD"), XccyTerms(receiveNonSpread)};
    }

    void RunQuoteRiskCase(const char* name, const RateRiskPerf::QuoteRiskBenchmarkCase_& input) {
        constexpr int kWarmup = 3;
        constexpr int kRepeats = 10;
        constexpr int kInvocationCount = kWarmup + kRepeats;
        const int calibrationCount = CurveCalibrationInvocationCount();
        const int passivePriceCount = RateCashflowPricingInternal::g_nodeSensitivityPassivePriceCount.load(std::memory_order_relaxed);
        const int preparationCount = RateCashflowPricingInternal::g_nodeSensitivityPreparationCount.load(std::memory_order_relaxed);
        const int sweepCount = RateCashflowPricingInternal::g_nodeSensitivitySweepCount.load(std::memory_order_relaxed);
#if DAL_RATE_RISK_NATIVE_AAD
        int tapeHighWater = 0;
        RateCashflowPricingInternal::g_nodeSensitivityTapeSizeSink = &tapeHighWater;
#endif
        double sink = 0.0;
        const auto result = Bench::Run(
            name,
            [&]() {
                const auto risk = AggregateRatePortfolioQuoteRisk(input.trades_, input.market_, input.provenances_);
                REQUIRE(!risk.buckets_.empty(), "Quote-risk benchmark produced no buckets");
                REQUIRE(risk.provenanceFailures_.empty(), "Quote-risk benchmark reported a stale provenance");
                REQUIRE(risk.meta_.size() == input.trades_.size(), "Quote-risk benchmark did not produce one observation per trade");
                sink += risk.buckets_.front().dv01_ + risk.buckets_.back().dPvDDecimalQuote_;
            },
            kWarmup, kRepeats);
#if DAL_RATE_RISK_NATIVE_AAD
        RateCashflowPricingInternal::g_nodeSensitivityTapeSizeSink = nullptr;
#endif
        const int passivePriceDelta =
            RateCashflowPricingInternal::g_nodeSensitivityPassivePriceCount.load(std::memory_order_relaxed) - passivePriceCount;
        const int preparationDelta =
            RateCashflowPricingInternal::g_nodeSensitivityPreparationCount.load(std::memory_order_relaxed) - preparationCount;
        const int sweepDelta = RateCashflowPricingInternal::g_nodeSensitivitySweepCount.load(std::memory_order_relaxed) - sweepCount;
        const int observedPassivePrices = passivePriceDelta / kInvocationCount;
        const int observedPreparations = preparationDelta / kInvocationCount;
        const int observedSweeps = sweepDelta / kInvocationCount;
        Bench::Print(result);
#if DAL_RATE_RISK_NATIVE_AAD
        std::fprintf(stderr, "%s observations: passive=%d preparations=%d sweeps=%d tape_high_water=%d\n", name, observedPassivePrices,
                     observedPreparations, observedSweeps, tapeHighWater);
#else
        std::fprintf(stderr, "%s observations: passive=%d preparations=%d sweeps=%d\n", name, observedPassivePrices, observedPreparations,
                     observedSweeps);
#endif
        REQUIRE(CurveCalibrationInvocationCount() == calibrationCount, "Quote-risk benchmark aggregate recalibrated inside the timed region");
        REQUIRE(passivePriceDelta == kInvocationCount * input.expectedPassivePriceCount_, "Quote-risk benchmark passive-price count drifted");
        REQUIRE(preparationDelta == kInvocationCount * input.expectedPreparationCount_, "Quote-risk benchmark preparation count drifted");
        REQUIRE(sweepDelta == kInvocationCount * input.expectedSweepCount_, "Quote-risk benchmark sweep count drifted");
        Bench::DoNotOptimize(&sink);
    }
} // namespace

int main() {
    const Date_ today(2026, 1, 15);
    const Date_ start(2026, 4, 15);
    const Date_ batchMaturity(2036, 4, 15);
    const Date_ dailyMaturity(2031, 4, 15);
    const auto market = Market(today, batchMaturity);
    const Vector_<String_> keys{"forecast", "discount"};
    const auto singleQuoteRisk = RateRiskPerf::MakeSingleCurveQuoteRiskCase();
    const auto jointQuoteRisk = RateRiskPerf::MakeJointXccyQuoteRiskCase();
    const auto stagedQuoteRisk = RateRiskPerf::MakeStagedXccyBasisQuoteRiskCase();
    const auto singleAnalyticPortfolio = RateRiskPerf::MakeSingleCurveQuoteRiskCase(5, CurveJacobianMode_::Value_::ANALYTIC, 120);
    const auto singleBumpedPortfolio = RateRiskPerf::MakeSingleCurveQuoteRiskCase(16, CurveJacobianMode_::Value_::BUMPED, 120);
    const auto jointAnalyticPortfolio = RateRiskPerf::MakeJointXccyQuoteRiskCase(10, CurveJacobianMode_::Value_::ANALYTIC, 24);
    const auto jointBumpedPortfolio = RateRiskPerf::MakeJointXccyQuoteRiskCase(10, CurveJacobianMode_::Value_::BUMPED, 24);
    const auto stagedAnalyticPortfolio = RateRiskPerf::MakeStagedXccyBasisQuoteRiskCase(16, CurveJacobianMode_::Value_::ANALYTIC, 24);
    const auto stagedBumpedPortfolio = RateRiskPerf::MakeStagedXccyBasisQuoteRiskCase(5, CurveJacobianMode_::Value_::BUMPED, 24);

    Vector_<RateTradeDefinition_> trades;
    trades.reserve(kBatchTrades);
    for (int index = 0; index < kBatchTrades; ++index)
        trades.push_back(IrsTrade(today, start, batchMaturity, index % 2 == 0));

    Bench::PrintHeader();

    {
        // The long batch: kBatchTrades trades x 2 components = 240 serial AAD sweeps, one passive
        // PV per trade and one preparation per component. Steady-state tape memory is bounded by
        // the largest single sweep (every sweep rewinds on scope exit), so this case and the
        // daily-compounding case below guard the tape's per-sweep high-water mark and the sweep
        // loop's latency together.
        double sink = 0.0;
        const auto result = Bench::Run("Rate batch serial (120 IRS x 2 components)", [&]() {
            const auto cells = RateTradeNodeSensitivitiesBatch(trades, market, keys);
            sink += cells.front().result_.pv_ + cells.back().result_.pv_;
        });
        Bench::Print(result);
        Bench::DoNotOptimize(&sink);
    }

    {
        // Per-sweep reference: the same 240 sweeps through the single-trade entry point. A batch
        // regression shows up as the batch case drifting away from this baseline.
        double sink = 0.0;
        const auto result = Bench::Run("Rate single-trade sweeps (240 IRS calls)", [&]() {
            for (int index = 0; index < kBatchTrades; ++index) {
                const auto first = RateTradeNodeSensitivities(trades[index], market, keys[0]);
                const auto second = RateTradeNodeSensitivities(trades[index], market, keys[1]);
                sink += first.pv_ + second.pv_;
            }
        });
        Bench::Print(result);
        Bench::DoNotOptimize(&sink);
    }

    {
        // The OIS daily-compounding shape: five years of quarterly periods compound daily on the
        // tape, the amplification the node-sensitivity stage guards against for passive curves.
        double sink = 0.0;
        const auto trade = OisTrade(today, start, dailyMaturity);
        const auto result = Bench::Run("Rate OIS daily compounding sweep (5Y quarterly x daily)", [&]() {
            const auto sensitivity = RateTradeNodeSensitivities(trade, market, keys[0]);
            sink += sensitivity.pv_;
        });
        Bench::Print(result);
        Bench::DoNotOptimize(&sink);
    }

#if DAL_RATE_RISK_NATIVE_AAD
    {
        // Informational (stderr, not a gated row): the native tape node count of one OIS
        // daily-compounding sweep — the per-sweep high-water the latency cases above stand in for.
        int tapeNodes = 0;
        RateCashflowPricingInternal::g_nodeSensitivityTapeSizeSink = &tapeNodes;
        const auto sensitivity = RateTradeNodeSensitivities(OisTrade(today, start, dailyMaturity), market, keys[0]);
        RateCashflowPricingInternal::g_nodeSensitivityTapeSizeSink = nullptr;
        std::fprintf(stderr, "OIS daily sweep native tape nodes: %d (eligible=%d)\n", tapeNodes, static_cast<int>(sensitivity.eligible_));
    }
#endif

    {
        // The XCCY shape of frozen P0 contract 8: every consumed curve is rebuilt active-typed on
        // each (trade, component) sweep and only the addressed component is registered, so the
        // per-sweep tape is bounded by periods x knots touched. 24 trades x 5 consumed components
        // = 120 serial sweeps; this is the regression-gate guard for that bound.
        const Date_ xccyMaturity(2031, 4, 15);
        const auto xccyMarket = XccyMarket(today, xccyMaturity);
        const Vector_<String_> xccyKeys{"domOis", "domFwd3M", "forOis", "forFwd3M", "basis"};
        Vector_<RateTradeDefinition_> xccyTrades;
        xccyTrades.reserve(24);
        for (int index = 0; index < 24; ++index)
            xccyTrades.push_back(XccyTrade(today, start, xccyMaturity, index % 2 == 0));
        const auto probe = RateTradeNodeSensitivitiesBatch(xccyTrades, xccyMarket, xccyKeys);
        const int eligible = static_cast<int>(std::count_if(probe.begin(), probe.end(), [](const auto& cell) { return cell.result_.eligible_; }));
        std::fprintf(stderr, "XCCY batch eligible cells: %d/%d\n", eligible, static_cast<int>(probe.size()));
        double sink = 0.0;
        const auto result = Bench::Run("Rate XCCY batch serial (24 XCCY x 5 components)", [&]() {
            const auto cells = RateTradeNodeSensitivitiesBatch(xccyTrades, xccyMarket, xccyKeys);
            sink += cells.front().result_.pv_ + cells.back().result_.pv_;
        });
        Bench::Print(result);
        Bench::DoNotOptimize(&sink);
    }
    RunQuoteRiskCase("Quote risk aggregate (single curve)", singleQuoteRisk);
    RunQuoteRiskCase("Quote risk aggregate (joint XCCY)", jointQuoteRisk);
    RunQuoteRiskCase("Quote risk aggregate (staged XCCY basis)", stagedQuoteRisk);
    RunQuoteRiskCase("Quote risk portfolio single ANALYTIC (120 deposits x N=5)", singleAnalyticPortfolio);
    RunQuoteRiskCase("Quote risk portfolio single BUMPED (120 deposits x N=16)", singleBumpedPortfolio);
    RunQuoteRiskCase("Quote risk portfolio joint ANALYTIC (24 XCCY x N=10/block)", jointAnalyticPortfolio);
    RunQuoteRiskCase("Quote risk portfolio joint BUMPED (24 XCCY x N=10/block)", jointBumpedPortfolio);
    RunQuoteRiskCase("Quote risk portfolio staged ANALYTIC (24 XCCY x N=16)", stagedAnalyticPortfolio);
    RunQuoteRiskCase("Quote risk portfolio staged BUMPED (24 XCCY x N=5)", stagedBumpedPortfolio);
    return 0;
}

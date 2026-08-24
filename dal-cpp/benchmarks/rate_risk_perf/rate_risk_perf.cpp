//
// Created by dal-implementer on 2026/8/24.
//
// Rate node-risk sweep benchmarks: the long-batch serial sweep (tape memory/latency
// driver — one passive PV per trade, one preparation per component, one AAD sweep per
// (trade, component) cell) and the OIS daily-compounding recording shape.

#include <dal/benchmarks/bench.hpp>
#include <dal/curve/piecewiseconstant.hpp>
#include <dal/curve/ratecashflowpricing.hpp>
#include <dal/curve/ratecashflowpricing_internal.hpp>
#include <dal/indice/fixingsnapshot.hpp>
#include <dal/platform/platform.hpp>
#include <dal/protocol/rateconvention.hpp>

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

    Handle_<DiscountCurve_> KnottedCurve(const String_& name, const Date_& horizon, double flatForward) {
        static const Vector_<Date_> knots{Date_(2026, 7, 15), Date_(2027, 1, 15), Date_(2028, 1, 15), Date_(2029, 1, 15),
                                          Date_(2030, 1, 15), Date_(2031, 1, 15), Date_(2032, 1, 15), Date_(2033, 1, 15)};
        (void)horizon;
        return Handle_<DiscountCurve_>(NewDiscountPWC(name, "USD", PiecewiseConstant_(knots, Vector_<>(knots.size(), flatForward))));
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
} // namespace

int main() {
    const Date_ today(2026, 1, 15);
    const Date_ start(2026, 4, 15);
    const Date_ batchMaturity(2036, 4, 15);
    const Date_ dailyMaturity(2031, 4, 15);
    const auto market = Market(today, batchMaturity);
    const Vector_<String_> keys{"forecast", "discount"};

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
    return 0;
}

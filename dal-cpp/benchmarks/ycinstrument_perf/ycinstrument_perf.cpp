//
// Created by dal-implementer on 2026/7/12.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include <dal/benchmarks/bench.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/piecewiselinear.hpp>
#include <dal/curve/ycimp.hpp>
#include <dal/curve/ycinstrument.hpp>
#include <dal/curve/ycpwlf.hpp>
#include <dal/platform/initall.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/time/date.hpp>
#include <dal/time/daybasis.hpp>
#include <dal/time/holidays.hpp>
#include <dal/time/periodlength.hpp>

using namespace Dal;

namespace {
    constexpr int kWarmups = 3;
    constexpr int kRepeats = 10;
    constexpr int kCheapPrecomputeBatch = 250000;
    constexpr int kCheapPriceBatch = 75000;
    constexpr int kLongPrecomputeBatch = 1100;
    constexpr int kSwapPriceBatch = 1000;
    constexpr int kOisSwapPriceBatch = 5000;
    constexpr int kBasisSwapPriceBatch = 750;
    constexpr int kDiscountBasketPasses = 500;
    constexpr int kProjectionBasketPasses = 200;
    constexpr int kBasisBasketPasses = 100;
    constexpr double kFutureConvexity = 0.0015;

    using InstrumentHandle_ = Handle_<YCInstrument_>;
    using RateHandle_ = Handle_<YCInstrument_::Rate_>;

    struct InstrumentSet_ {
        InstrumentHandle_ deposit_;
        InstrumentHandle_ fra_;
        InstrumentHandle_ future_;
        InstrumentHandle_ stir_;
        InstrumentHandle_ swap_;
        InstrumentHandle_ oisSwap_;
        InstrumentHandle_ basisSwap_;
    };

    struct InstrumentCase_ {
        std::string label_;
        InstrumentHandle_ instrument_;
        int precomputeBatch_;
        int priceBatch_;
    };

    RateLegConvention_ MakeLeg(const char* frequency, const DayBasis_& basis) {
        RateLegConvention_ result;
        result.paymentLag_ = 0;
        result.paymentFrequency_ = PeriodLength_(frequency);
        result.dayBasis_ = basis;
        result.businessDayConvention_ = BizDayConvention_("Unadjusted");
        result.paymentConvention_ = BizDayConvention_("Unadjusted");
        result.accrualHolidays_ = Holidays::None();
        result.paymentHolidays_ = Holidays::None();
        return result;
    }

    RateIndexConvention_ MakeIndex(const char* tenor, const DayBasis_& basis, bool useProjection) {
        RateIndexConvention_ result;
        result.spotLag_ = 0;
        result.fixingLag_ = 0;
        result.useProjectionCurve_ = useProjection;
        result.forecastTenor_ = PeriodLength_(tenor);
        result.dayBasis_ = basis;
        result.businessDayConvention_ = BizDayConvention_("Unadjusted");
        result.fixingHolidays_ = Holidays::None();
        result.accrualHolidays_ = Holidays::None();
        result.collateral_ = CollateralType_(CollateralType_::Value_::OIS);
        return result;
    }

    Handle_<YieldCurve_> BuildMarket(const Date_& today, const DayBasis_& basis, double sixMonthSpread = 0.01) {
        const Vector_<Date_> knots = {
            Date::AddMonths(today, 3),  Date::AddMonths(today, 6),   Date::AddMonths(today, 12),  Date::AddMonths(today, 24),
            Date::AddMonths(today, 60), Date::AddMonths(today, 120), Date::AddMonths(today, 240), Date::AddMonths(today, 480),
        };
        const Vector_<> oisValues(knots.size(), 0.02);
        const Vector_<> threeMonthSpread(knots.size(), 0.005);
        const Vector_<> sixMonthSpreadValues(knots.size(), sixMonthSpread);

        const Handle_<DiscountCurve_> ois(NewDiscountPWLF("pricing_ois", "USD", PiecewiseLinear_(knots, oisValues, oisValues)));
        const Handle_<DiscountCurve_> threeMonth(
            NewDiscountPWLF("pricing_3m", "USD", PiecewiseLinear_(knots, threeMonthSpread, threeMonthSpread), ois));
        const Handle_<DiscountCurve_> sixMonth(
            NewDiscountPWLF("pricing_6m", "USD", PiecewiseLinear_(knots, sixMonthSpreadValues, sixMonthSpreadValues), ois));

        return Handle_<YieldCurve_>(new CurveBlock_("pricing_market", "USD", {{CollateralType_(CollateralType_::Value_::OIS), ois}},
                                                    {{PeriodLength_("3M"), threeMonth}, {PeriodLength_("6M"), sixMonth}}, basis));
    }

    InstrumentSet_ BuildInstrumentSet(const Date_& today, const DayBasis_& basis) {
        const Date_ threeMonths = Date::AddMonths(today, 3);
        const Date_ sixMonths = Date::AddMonths(today, 6);
        const Date_ tenYears = Date::AddMonths(today, 120);
        const RateIndexConvention_ discountIndex = MakeIndex("3M", basis, false);
        const RateIndexConvention_ projected3m = MakeIndex("3M", basis, true);
        const RateIndexConvention_ projected6m = MakeIndex("6M", basis, true);
        const RateLegConvention_ annualLeg = MakeLeg("12M", basis);
        const RateLegConvention_ quarterlyLeg = MakeLeg("3M", basis);
        const RateLegConvention_ semiannualLeg = MakeLeg("6M", basis);

        InstrumentSet_ result;
        result.deposit_.reset(new Deposit_(today, today, sixMonths, 0.0, discountIndex));
        result.fra_.reset(new FRA_(today, threeMonths, sixMonths, 0.0, projected3m));
        result.future_.reset(new Future_(today, threeMonths, sixMonths, 0.0, projected3m, kFutureConvexity));
        result.stir_.reset(new STIR_(today, threeMonths, sixMonths, 0.0, discountIndex));
        result.swap_.reset(new Swap_(today, today, tenYears, 0.0, annualLeg, projected3m, quarterlyLeg));
        result.oisSwap_.reset(new OISSwap_(today, today, tenYears, 0.0, annualLeg, discountIndex, annualLeg));
        result.basisSwap_.reset(new BasisSwap_(today, today, tenYears, 0.0, projected3m, quarterlyLeg, projected6m, semiannualLeg));
        return result;
    }

    double Price(const InstrumentHandle_& instrument, const YieldCurve_& market) {
        const RateHandle_ rate = instrument->Precompute(Handle_<YieldCurve_>());
        return (*rate)(market);
    }

    void ValidateInstrumentSet(const InstrumentSet_& instruments, const Date_& today, const DayBasis_& basis, const YieldCurve_& market) {
        const std::vector<InstrumentHandle_> all = {
            instruments.deposit_, instruments.fra_,     instruments.future_,    instruments.stir_,
            instruments.swap_,    instruments.oisSwap_, instruments.basisSwap_,
        };
        for (const auto& instrument : all)
            REQUIRE(std::isfinite(Price(instrument, market)), "Instrument pricing benchmark requires finite model rates");

        const Date_ threeMonths = Date::AddMonths(today, 3);
        const Date_ sixMonths = Date::AddMonths(today, 6);
        const Date_ tenYears = Date::AddMonths(today, 120);
        const RateIndexConvention_ discountIndex = MakeIndex("3M", basis, false);
        const RateIndexConvention_ projected3m = MakeIndex("3M", basis, true);
        const RateLegConvention_ annualLeg = MakeLeg("12M", basis);

        const InstrumentHandle_ discountFra(new FRA_(today, threeMonths, sixMonths, 0.0, discountIndex));
        const InstrumentHandle_ projectedFra(new FRA_(today, threeMonths, sixMonths, 0.0, projected3m));
        const InstrumentHandle_ discountFraForStir(new FRA_(today, threeMonths, sixMonths, 0.0, discountIndex));
        const InstrumentHandle_ discountSwap(new Swap_(today, today, tenYears, 0.0, annualLeg, discountIndex, annualLeg));

        REQUIRE(std::fabs(Price(projectedFra, market) - Price(discountFra, market)) > 1.0e-8,
                "Projection benchmark must not silently fall back to discount pricing");
        REQUIRE(std::fabs(Price(instruments.future_, market) - (Price(projectedFra, market) - kFutureConvexity)) < 1.0e-12,
                "Future benchmark must apply its convexity adjustment");
        REQUIRE(std::fabs(Price(instruments.stir_, market) - Price(discountFraForStir, market)) < 1.0e-12,
                "STIR benchmark must retain inherited FRA pricing");
        REQUIRE(std::fabs(Price(instruments.oisSwap_, market) - Price(discountSwap, market)) < 1.0e-12,
                "OIS benchmark must retain inherited discount-only swap pricing");

        const Handle_<YieldCurve_> alternateMarket = BuildMarket(today, basis, 0.015);
        REQUIRE(std::fabs(Price(instruments.basisSwap_, market) - Price(instruments.basisSwap_, *alternateMarket)) > 1.0e-8,
                "Basis-swap benchmark must read the 6M projection curve");
    }

    std::vector<InstrumentCase_> IndividualCases(const InstrumentSet_& instruments) {
        return {
            {"Deposit_ 6M", instruments.deposit_, kCheapPrecomputeBatch, kCheapPriceBatch},
            {"FRA_ 3x6 3M projection", instruments.fra_, kCheapPrecomputeBatch, kCheapPriceBatch},
            {"Future_ 3x6 3M projection", instruments.future_, kCheapPrecomputeBatch, kCheapPriceBatch},
            {"STIR_ 3x6", instruments.stir_, kCheapPrecomputeBatch, kCheapPriceBatch},
            {"Swap_ 10Y annual-vs-3M", instruments.swap_, kLongPrecomputeBatch, kSwapPriceBatch},
            {"OISSwap_ 10Y annual", instruments.oisSwap_, kLongPrecomputeBatch, kOisSwapPriceBatch},
            {"BasisSwap_ 10Y 3M-vs-6M PASSIVE", instruments.basisSwap_, kLongPrecomputeBatch, kBasisSwapPriceBatch},
        };
    }

    Bench::Result_ Normalize(const Bench::Result_& raw, const std::string& name, int64_t divisor) {
        REQUIRE(divisor > 0, "Benchmark normalization divisor must be positive");
        return Bench::Result_{name, raw.medianNs / divisor, raw.minNs / divisor, raw.maxNs / divisor, raw.repeats};
    }

    void RunPrecompute(const InstrumentCase_& instrumentCase) {
        RateHandle_ sink = instrumentCase.instrument_->Precompute(Handle_<YieldCurve_>());
        const Bench::Result_ raw = Bench::Run(
            instrumentCase.label_,
            [&]() {
                for (int i = 0; i < instrumentCase.precomputeBatch_; ++i)
                    sink = instrumentCase.instrument_->Precompute(Handle_<YieldCurve_>());
            },
            kWarmups, kRepeats);
        Bench::DoNotOptimize(sink.get());
        REQUIRE(sink, "Instrument PRECOMPUTE sink must retain a rate handle");
        Bench::Print(Normalize(raw, instrumentCase.label_ + " PRECOMPUTE lifecycle / operation", instrumentCase.precomputeBatch_));
    }

    void RunPrice(const InstrumentCase_& instrumentCase, const YieldCurve_& market) {
        const RateHandle_ rate = instrumentCase.instrument_->Precompute(Handle_<YieldCurve_>());
        double checksum = 0.0;
        const Bench::Result_ raw = Bench::Run(
            instrumentCase.label_,
            [&]() {
                for (int i = 0; i < instrumentCase.priceBatch_; ++i)
                    checksum += (*rate)(market);
            },
            kWarmups, kRepeats);
        Bench::DoNotOptimize(&checksum);
        REQUIRE(std::isfinite(checksum) && std::fabs(checksum) > 0.0, "Instrument PRICE checksum must be finite and non-zero");
        Bench::Print(Normalize(raw, instrumentCase.label_ + " PRICE / operation", instrumentCase.priceBatch_));
    }

    std::vector<InstrumentHandle_> DiscountBasket(const Date_& today, const DayBasis_& basis) {
        const RateIndexConvention_ discountIndex = MakeIndex("3M", basis, false);
        const RateLegConvention_ annualLeg = MakeLeg("12M", basis);
        std::vector<InstrumentHandle_> result;
        result.reserve(20);
        for (int month = 1; month <= 5; ++month)
            result.emplace_back(new Deposit_(today, today, Date::AddMonths(today, month), 0.0, discountIndex));
        for (int startMonth : {6, 9, 12})
            result.emplace_back(new STIR_(today, Date::AddMonths(today, startMonth), Date::AddMonths(today, startMonth + 3), 0.0, discountIndex));
        for (int year = 2; year <= 13; ++year)
            result.emplace_back(new OISSwap_(today, today, Date::AddMonths(today, 12 * year), 0.0, annualLeg, discountIndex, annualLeg));
        return result;
    }

    std::vector<InstrumentHandle_> ProjectionBasket(const Date_& today, const DayBasis_& basis) {
        const RateIndexConvention_ projected3m = MakeIndex("3M", basis, true);
        const RateLegConvention_ annualLeg = MakeLeg("12M", basis);
        const RateLegConvention_ quarterlyLeg = MakeLeg("3M", basis);
        std::vector<InstrumentHandle_> result;
        result.reserve(20);
        for (int startMonth = 1; startMonth <= 8; ++startMonth)
            result.emplace_back(new FRA_(today, Date::AddMonths(today, startMonth), Date::AddMonths(today, startMonth + 3), 0.0, projected3m));
        for (int startMonth = 9; startMonth <= 12; ++startMonth)
            result.emplace_back(
                new Future_(today, Date::AddMonths(today, startMonth), Date::AddMonths(today, startMonth + 3), 0.0, projected3m, kFutureConvexity));
        for (int year = 3; year <= 10; ++year)
            result.emplace_back(new Swap_(today, today, Date::AddMonths(today, 12 * year), 0.0, annualLeg, projected3m, quarterlyLeg));
        return result;
    }

    std::vector<InstrumentHandle_> BasisBasket(const Date_& today, const DayBasis_& basis) {
        const RateIndexConvention_ projected3m = MakeIndex("3M", basis, true);
        const RateIndexConvention_ projected6m = MakeIndex("6M", basis, true);
        const RateLegConvention_ quarterlyLeg = MakeLeg("3M", basis);
        const RateLegConvention_ semiannualLeg = MakeLeg("6M", basis);
        std::vector<InstrumentHandle_> result;
        result.reserve(10);
        for (int year = 2; year <= 11; ++year)
            result.emplace_back(
                new BasisSwap_(today, today, Date::AddMonths(today, 12 * year), 0.0, projected3m, quarterlyLeg, projected6m, semiannualLeg));
        return result;
    }

    std::vector<RateHandle_> PrecomputeAll(const std::vector<InstrumentHandle_>& instruments) {
        std::vector<RateHandle_> result;
        result.reserve(instruments.size());
        for (const auto& instrument : instruments)
            result.push_back(instrument->Precompute(Handle_<YieldCurve_>()));
        return result;
    }

    void ValidateBasket(const std::vector<RateHandle_>& rates, const YieldCurve_& market) {
        REQUIRE(!rates.empty(), "Pricing basket must not be empty");
        double checksum = 0.0;
        for (const auto& rate : rates) {
            const double value = (*rate)(market);
            REQUIRE(std::isfinite(value), "Pricing basket requires finite model rates");
            checksum += value;
        }
        REQUIRE(std::isfinite(checksum) && std::fabs(checksum) > 0.0, "Pricing basket checksum must be finite and non-zero");
    }

    void RunBasket(const std::string& label, const std::vector<RateHandle_>& rates, const YieldCurve_& market, int passes) {
        double checksum = 0.0;
        const Bench::Result_ raw = Bench::Run(
            label,
            [&]() {
                for (int pass = 0; pass < passes; ++pass)
                    for (const auto& rate : rates)
                        checksum += (*rate)(market);
            },
            kWarmups, kRepeats);
        Bench::DoNotOptimize(&checksum);
        REQUIRE(std::isfinite(checksum) && std::fabs(checksum) > 0.0, "Pricing BASKET checksum must be finite and non-zero");
        Bench::Print(Normalize(raw, label + " BASKET / pass", passes));
        Bench::Print(Normalize(raw, label + " PER-INSTRUMENT", passes * static_cast<int64_t>(rates.size())));
    }
} // namespace

int main() {
    RegisterAll_::Init();
    const Date_ today(2024, 1, 15);
    const DayBasis_ basis("ACT_365F");
    const Handle_<YieldCurve_> market = BuildMarket(today, basis);
    const InstrumentSet_ instruments = BuildInstrumentSet(today, basis);
    ValidateInstrumentSet(instruments, today, basis, *market);

    const std::vector<InstrumentHandle_> discountInstruments = DiscountBasket(today, basis);
    const std::vector<InstrumentHandle_> projectionInstruments = ProjectionBasket(today, basis);
    const std::vector<InstrumentHandle_> basisInstruments = BasisBasket(today, basis);
    const std::vector<RateHandle_> discountRates = PrecomputeAll(discountInstruments);
    const std::vector<RateHandle_> projectionRates = PrecomputeAll(projectionInstruments);
    const std::vector<RateHandle_> basisRates = PrecomputeAll(basisInstruments);
    ValidateBasket(discountRates, *market);
    ValidateBasket(projectionRates, *market);
    ValidateBasket(basisRates, *market);

    Bench::PrintHeader();
    for (const auto& instrumentCase : IndividualCases(instruments)) {
        RunPrecompute(instrumentCase);
        RunPrice(instrumentCase, *market);
    }
    RunBasket("Discount 20-instrument", discountRates, *market, kDiscountBasketPasses);
    RunBasket("3M projection 20-instrument", projectionRates, *market, kProjectionBasketPasses);
    RunBasket("3M-vs-6M PASSIVE 10-instrument", basisRates, *market, kBasisBasketPasses);
    return 0;
}

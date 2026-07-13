#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include <dal/benchmarks/bench.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/piecewiseconstant.hpp>
#include <dal/curve/piecewiselinear.hpp>
#include <dal/curve/xccycalibration.hpp>
#include <dal/curve/ycconst.hpp>
#include <dal/curve/ycimp.hpp>
#include <dal/platform/initall.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/storage/globals.hpp>
#include <dal/time/date.hpp>
#include <dal/time/daybasis.hpp>
#include <dal/time/holidays.hpp>
#include <dal/time/periodlength.hpp>

using namespace Dal;

namespace {
    constexpr int kWarmups = 3;
    constexpr int kRepeats = 10;
    constexpr int kPrecomputeBatch = 2000;
    constexpr int kPriceBatch = 2000;
    constexpr int kBasketPasses = 250;
    constexpr double kFxSpot = 1.10;

    using SwapHandle_ = Handle_<CrossCurrencySwap_>;
    using RateHandle_ = Handle_<CrossCurrencySwap_::Rate_>;

    struct Fixture_ {
        Date_ today_;
        Handle_<CurveBlock_> domesticBlock_;
        Handle_<CurveBlock_> foreignBlock_;
        Vector_<Date_> basisKnots_;
        CrossCurrencyConvention_ convention_;
    };

    Bench::Result_ Normalize(const Bench::Result_& raw, const std::string& name, int64_t divisor) {
        REQUIRE(divisor > 0, "XCCY benchmark normalization divisor must be positive");
        return Bench::Result_{name, raw.medianNs / divisor, raw.minNs / divisor, raw.maxNs / divisor, raw.repeats};
    }

    Handle_<DiscountCurve_> MakeFlatCurve(const String_& name, const String_& ccy, const Date_& today, double rate) {
        const Vector_<Date_> knots = {
            Date::AddMonths(today, 6),  Date::AddMonths(today, 12),  Date::AddMonths(today, 24),
            Date::AddMonths(today, 60), Date::AddMonths(today, 120), Date::AddMonths(today, 240),
        };
        const Vector_<> values(knots.size(), rate);
        return Handle_<DiscountCurve_>(NewDiscountPWLF(name, ccy, PiecewiseLinear_(knots, values, values)));
    }

    Handle_<CurveBlock_> MakeBlock(const String_& name, const String_& ccy, const Date_& today, double rate) {
        return Handle_<CurveBlock_>(new CurveBlock_(MakeFlatCurve(name, ccy, today, rate)));
    }

    RateIndexConvention_ MakeIndex() {
        RateIndexConvention_ result;
        result.spotLag_ = 0;
        result.fixingLag_ = 0;
        result.useProjectionCurve_ = true;
        result.forecastTenor_ = PeriodLength_("3M");
        result.dayBasis_ = DayBasis_("ACT_365F");
        result.businessDayConvention_ = BizDayConvention_("Unadjusted");
        result.fixingHolidays_ = Holidays::None();
        result.accrualHolidays_ = Holidays::None();
        result.collateral_ = CollateralType_(CollateralType_::Value_::OIS);
        return result;
    }

    RateLegConvention_ MakeLeg() {
        RateLegConvention_ result;
        result.paymentLag_ = 0;
        result.paymentFrequency_ = PeriodLength_("3M");
        result.dayBasis_ = DayBasis_("ACT_365F");
        result.businessDayConvention_ = BizDayConvention_("Unadjusted");
        result.paymentConvention_ = BizDayConvention_("Unadjusted");
        result.accrualHolidays_ = Holidays::None();
        result.paymentHolidays_ = Holidays::None();
        return result;
    }

    Fixture_ MakeFixture() {
        Fixture_ result;
        result.today_ = Date_(2024, 1, 15);
        result.domesticBlock_ = MakeBlock("xccy_perf_usd", "USD", result.today_, 0.02);
        result.foreignBlock_ = MakeBlock("xccy_perf_eur", "EUR", result.today_, 0.01);
        result.basisKnots_ = {
            Date::AddMonths(result.today_, 6),  Date::AddMonths(result.today_, 12),  Date::AddMonths(result.today_, 24),
            Date::AddMonths(result.today_, 60), Date::AddMonths(result.today_, 120),
        };
        result.convention_.initialNotionalExchange_ = true;
        result.convention_.finalNotionalExchange_ = true;
        result.convention_.spreadOnForeignLeg_ = true;
        result.convention_.domesticIndex_ = MakeIndex();
        result.convention_.domesticLeg_ = MakeLeg();
        result.convention_.foreignIndex_ = MakeIndex();
        result.convention_.foreignLeg_ = MakeLeg();
        return result;
    }

    CrossCurrencyMarket_ MakeMarket(const Fixture_& fixture, double basisRate) {
        CrossCurrencyMarket_ result(fixture.domesticBlock_, fixture.foreignBlock_, kFxSpot);
        if (basisRate != 0.0) {
            const Vector_<> values(fixture.basisKnots_.size(), basisRate);
            result.SetBasisCurve(Handle_<DiscountCurve_>(NewDiscountPWC("xccy_perf_basis", "USD", PiecewiseConstant_(fixture.basisKnots_, values))));
        }
        return result;
    }

    SwapHandle_ MakeSwap(const Fixture_& fixture, int maturityMonths, double marketRate = 0.0) {
        return SwapHandle_(new CrossCurrencySwap_(fixture.today_, fixture.today_, Date::AddMonths(fixture.today_, maturityMonths), marketRate,
                                                  CurrencyPair_(Ccy_("USD"), Ccy_("EUR")), 110.0, 100.0, fixture.convention_));
    }

    std::vector<SwapHandle_> MakePricingBasket(const Fixture_& fixture) {
        std::vector<SwapHandle_> result;
        result.reserve(10);
        for (int year = 1; year <= 10; ++year)
            result.push_back(MakeSwap(fixture, 12 * year));
        return result;
    }

    std::vector<RateHandle_> PrecomputeAll(const std::vector<SwapHandle_>& instruments) {
        std::vector<RateHandle_> result;
        result.reserve(instruments.size());
        for (const auto& instrument : instruments)
            result.push_back(instrument->Precompute());
        return result;
    }

    CrossCurrencyCalibrationSpec_ MakeCalibrationSpec(const Fixture_& fixture) {
        const CrossCurrencyMarket_ quoteMarket = MakeMarket(fixture, 0.0020);
        const Vector_<int> maturities = {6, 12, 18, 24, 30, 36, 42, 48, 54, 60, 72, 84, 96, 108, 120};

        CrossCurrencyCalibrationSpec_ result;
        result.today_ = fixture.today_;
        result.basisPair_ = CurrencyPair_(Ccy_("USD"), Ccy_("EUR"));
        result.domesticCurveBlock_ = fixture.domesticBlock_;
        result.foreignCurveBlock_ = fixture.foreignBlock_;
        result.fxSpot_ = kFxSpot;
        result.knotDates_ = fixture.basisKnots_;
        result.tolerance_ = 1.0e-8;
        result.fitTolerance_ = 1.0e-7;
        result.initialGuess_ = 0.0;
        result.instruments_.reserve(maturities.size());
        for (const int maturity : maturities) {
            const SwapHandle_ prototype = MakeSwap(fixture, maturity);
            const double quote = (*prototype->Precompute())(quoteMarket);
            result.instruments_.push_back(MakeSwap(fixture, maturity, quote));
        }
        return result;
    }

    void ValidatePricing(const RateHandle_& fixedRate, const std::vector<RateHandle_>& basket, const CrossCurrencyMarket_& market) {
        const double fixedValue = (*fixedRate)(market);
        REQUIRE(std::isfinite(fixedValue) && std::fabs(fixedValue) > 0.0, "Fixed XCCY benchmark requires a finite, non-zero 10Y price");
        REQUIRE(basket.size() == 10, "Fixed XCCY benchmark requires exactly ten basket instruments");
        double checksum = 0.0;
        for (const auto& rate : basket) {
            const double value = (*rate)(market);
            REQUIRE(std::isfinite(value), "Fixed XCCY basket requires finite prices");
            checksum += value;
        }
        REQUIRE(std::isfinite(checksum) && std::fabs(checksum) > 0.0, "Fixed XCCY basket checksum must be finite and non-zero");
    }

    void ValidateCalibration(const CrossCurrencyCalibrationSpec_& spec) {
        const auto result = CalibrateCrossCurrencyMarket(spec);
        REQUIRE(result.diagnostics_.maxAbsResidual_ <= spec.tolerance_,
                "Basis-only XCCY calibration benchmark must reprice inside its configured tolerance");
        REQUIRE(result.diagnostics_.residuals_.size() == spec.instruments_.size(),
                "Basis-only XCCY calibration benchmark must report every instrument residual");
    }

    void RunPrecompute(const SwapHandle_& fixedSwap) {
        RateHandle_ sink = fixedSwap->Precompute();
        const auto raw = Bench::Run(
            "XCCY fixed 10Y PRECOMPUTE",
            [&]() {
                for (int i = 0; i < kPrecomputeBatch; ++i)
                    sink = fixedSwap->Precompute();
            },
            kWarmups, kRepeats);
        Bench::DoNotOptimize(sink.get());
        REQUIRE(sink, "Fixed XCCY PRECOMPUTE sink must retain a rate handle");
        Bench::Print(Normalize(raw, "XCCY fixed 10Y PRECOMPUTE / operation", kPrecomputeBatch));
    }

    void RunPrice(const RateHandle_& fixedRate, const CrossCurrencyMarket_& market) {
        double checksum = 0.0;
        const auto raw = Bench::Run(
            "XCCY fixed 10Y PRICE",
            [&]() {
                for (int i = 0; i < kPriceBatch; ++i)
                    checksum += (*fixedRate)(market);
            },
            kWarmups, kRepeats);
        Bench::DoNotOptimize(&checksum);
        REQUIRE(std::isfinite(checksum) && std::fabs(checksum) > 0.0, "Fixed XCCY PRICE checksum must be finite and non-zero");
        Bench::Print(Normalize(raw, "XCCY fixed 10Y PRICE / operation", kPriceBatch));
    }

    void RunBasket(const std::vector<RateHandle_>& basket, const CrossCurrencyMarket_& market) {
        double checksum = 0.0;
        const auto raw = Bench::Run(
            "XCCY fixed 10-instrument basket",
            [&]() {
                for (int pass = 0; pass < kBasketPasses; ++pass)
                    for (const auto& rate : basket)
                        checksum += (*rate)(market);
            },
            kWarmups, kRepeats);
        Bench::DoNotOptimize(&checksum);
        REQUIRE(std::isfinite(checksum) && std::fabs(checksum) > 0.0, "Fixed XCCY BASKET checksum must be finite and non-zero");
        Bench::Print(Normalize(raw, "XCCY fixed 10-instrument BASKET / pass", kBasketPasses));
        Bench::Print(Normalize(raw, "XCCY fixed 10-instrument PER-INSTRUMENT", kBasketPasses * static_cast<int64_t>(basket.size())));
    }

    void RunCalibration(const CrossCurrencyCalibrationSpec_& spec) {
        double checksum = 0.0;
        const auto result = Bench::Run(
            "XCCY basis-only CALIBRATION (15 instruments, 5 knots)",
            [&]() {
                const auto calibration = CalibrateCrossCurrencyMarket(spec);
                checksum += calibration.diagnostics_.maxAbsResidual_;
                checksum += calibration.fxForwardCurve_.forwards_.back();
            },
            1, kRepeats);
        Bench::DoNotOptimize(&checksum);
        REQUIRE(std::isfinite(checksum) && std::fabs(checksum) > 0.0, "Basis-only XCCY CALIBRATION checksum must be finite and non-zero");
        Bench::Print(result);
    }
} // namespace

int main() {
    RegisterAll_::Init();
    const Fixture_ fixture = MakeFixture();
    XGLOBAL::SetEvaluationDate(fixture.today_);

    const CrossCurrencyMarket_ quoteMarket = MakeMarket(fixture, 0.0020);
    const SwapHandle_ fixedSwap = MakeSwap(fixture, 120);
    const RateHandle_ fixedRate = fixedSwap->Precompute();
    const std::vector<RateHandle_> basket = PrecomputeAll(MakePricingBasket(fixture));
    const CrossCurrencyCalibrationSpec_ calibrationSpec = MakeCalibrationSpec(fixture);

    ValidatePricing(fixedRate, basket, quoteMarket);
    ValidateCalibration(calibrationSpec);

    Bench::PrintHeader();
    RunPrecompute(fixedSwap);
    RunPrice(fixedRate, quoteMarket);
    RunBasket(basket, quoteMarket);
    RunCalibration(calibrationSpec);
    return 0;
}

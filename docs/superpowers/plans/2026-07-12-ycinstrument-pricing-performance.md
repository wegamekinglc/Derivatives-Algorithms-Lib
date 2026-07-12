# Yield-Curve Instrument Pricing Performance Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a cross-platform `ycinstrument_perf` executable that separately measures precompute lifecycle and steady-state pricing for all seven concrete yield-curve instruments, plus three calibration-shaped pricing baskets.

**Architecture:** Build one immutable map-backed OIS/3M/6M market fixture, construct every instrument and precomputed rate outside steady-state timed regions, and use the shared `Dal::Bench` harness for batched measurements. Keep correctness gates untimed, normalize samples without modifying the shared harness, and report the new executable in Linux and Windows CI without adding a noisy regression threshold.

**Tech Stack:** C++17, DAL curve and instrument APIs, CMake, `Dal::Bench`, GitHub Actions YAML, Google Test.

## Global Constraints

- Benchmark all seven concrete types: `Deposit_`, `FRA_`, `Future_`, `STIR_`, `Swap_`, `OISSwap_`, and `BasisSwap_`.
- Print separate `PRECOMPUTE` and steady-state `PRICE` rows for each concrete type.
- Print discount, 3M projection, and 3M-vs-6M baskets as both `BASKET` and `PER-INSTRUMENT` rows.
- Use a map-backed `CurveBlock_` with OIS discounting and base-layered 3M/6M forward curves.
- Construct curves and instruments outside timed regions; construct precomputed `Rate_` handles outside `PRICE` rows.
- Use three warmups and ten measured samples; batch work so samples target 5-20ms.
- Add no absolute timing assertion and do not add `ycinstrument_perf` to the calibrated eight-target regression gate.
- Label `BasisSwap_` rows `PASSIVE`; do not imply analytical-Jacobian support.
- Preserve `dal-cpp/dal/benchmarks/bench.hpp` and all production pricing/calibration code unchanged.
- Keep `dal/platform/platform.hpp` first and format the new translation unit with `clang-format -sort-includes=0`.
- The original checkout rejects `.git/index.lock` writes. Execute in the writable isolated feature checkout, create task-scoped commits there, and leave the original checkout untouched.

---

### Task 1: Add the instrument-pricing benchmark executable

**Files:**
- Modify: `dal-cpp/benchmarks/CMakeLists.txt`
- Create: `dal-cpp/benchmarks/ycinstrument_perf/CMakeLists.txt`
- Create: `dal-cpp/benchmarks/ycinstrument_perf/ycinstrument_perf.cpp`
- Test: `dal-cpp/tests/curve/test_ycinstrument.cpp`

**Interfaces:**
- Consumes: `Dal::Bench::Run`, `Dal::Bench::Print`, `Dal::Bench::DoNotOptimize`, `YCInstrument_::Precompute`, `YCInstrument_::Rate_::operator()`, `CurveBlock_`, and `NewDiscountPWLF`.
- Produces: CMake target and installed executable `ycinstrument_perf`, with exactly 20 result rows.

- [ ] **Step 1: Register the missing target to establish the red build gate**

Edit `dal-cpp/benchmarks/CMakeLists.txt` so the tail of `DAL_BENCHMARK_TARGETS` reads:

```cmake
    iv_brent_perf
    script_mc_perf
    ycinstrument_perf
    curve_calibration_perf
    threadpool_perf
    stacks_perf)
```

- [ ] **Step 2: Run the red gate and confirm the target is genuinely missing**

Run:

```bash
cmake --build build --target ycinstrument_perf -j2
```

Expected: CMake regeneration fails because `dal-cpp/benchmarks/ycinstrument_perf` does not exist. A compiler error or an unrelated configuration error is not the expected red result and must be diagnosed before proceeding.

- [ ] **Step 3: Add the standard target definition**

Create `dal-cpp/benchmarks/ycinstrument_perf/CMakeLists.txt` with exactly:

```cmake
file(GLOB_RECURSE YCINSTRUMENT_PERF_FILES CONFIGURE_DEPENDS "*.hpp" "*.cpp")

add_executable(ycinstrument_perf ${YCINSTRUMENT_PERF_FILES})

target_link_libraries(ycinstrument_perf dal_library)

if(DAL_USE_XAD_AAD)
    target_link_libraries(ycinstrument_perf XAD::xad)
elseif(DAL_USE_CODIPACK_AAD)
    target_link_libraries(ycinstrument_perf CoDiPack)
elseif(DAL_USE_ADEPT_AAD)
    target_link_libraries(ycinstrument_perf adept)
endif ()

if(MSVC)
else()
    target_link_libraries(ycinstrument_perf pthread)
endif()

install(TARGETS ycinstrument_perf
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )
```

- [ ] **Step 4: Implement the complete benchmark**

Create `dal-cpp/benchmarks/ycinstrument_perf/ycinstrument_perf.cpp` with the following implementation:

```cpp
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
    constexpr int kCheapBatch = 100000;
    constexpr int kLongPrecomputeBatch = 1000;
    constexpr int kLongPriceBatch = 5000;
    constexpr int kBasketPasses = 1000;
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
            Date::AddMonths(today, 3),   Date::AddMonths(today, 6),   Date::AddMonths(today, 12),  Date::AddMonths(today, 24),
            Date::AddMonths(today, 60),  Date::AddMonths(today, 120), Date::AddMonths(today, 240), Date::AddMonths(today, 480),
        };
        const Vector_<> oisValues(knots.size(), 0.02);
        const Vector_<> threeMonthSpread(knots.size(), 0.005);
        const Vector_<> sixMonthSpreadValues(knots.size(), sixMonthSpread);

        const Handle_<DiscountCurve_> ois(NewDiscountPWLF("pricing_ois", "USD", PiecewiseLinear_(knots, oisValues, oisValues)));
        const Handle_<DiscountCurve_> threeMonth(
            NewDiscountPWLF("pricing_3m", "USD", PiecewiseLinear_(knots, threeMonthSpread, threeMonthSpread), ois));
        const Handle_<DiscountCurve_> sixMonth(
            NewDiscountPWLF("pricing_6m", "USD", PiecewiseLinear_(knots, sixMonthSpreadValues, sixMonthSpreadValues), ois));

        return Handle_<YieldCurve_>(new CurveBlock_("pricing_market",
                                                     "USD",
                                                     {{CollateralType_(CollateralType_::Value_::OIS), ois}},
                                                     {{PeriodLength_("3M"), threeMonth}, {PeriodLength_("6M"), sixMonth}},
                                                     basis));
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
            instruments.deposit_, instruments.fra_, instruments.future_, instruments.stir_,
            instruments.swap_, instruments.oisSwap_, instruments.basisSwap_,
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
            {"Deposit_ 6M", instruments.deposit_, kCheapBatch, kCheapBatch},
            {"FRA_ 3x6 3M projection", instruments.fra_, kCheapBatch, kCheapBatch},
            {"Future_ 3x6 3M projection", instruments.future_, kCheapBatch, kCheapBatch},
            {"STIR_ 3x6", instruments.stir_, kCheapBatch, kCheapBatch},
            {"Swap_ 10Y annual-vs-3M", instruments.swap_, kLongPrecomputeBatch, kLongPriceBatch},
            {"OISSwap_ 10Y annual", instruments.oisSwap_, kLongPrecomputeBatch, kLongPriceBatch},
            {"BasisSwap_ 10Y 3M-vs-6M PASSIVE", instruments.basisSwap_, kLongPrecomputeBatch, kLongPriceBatch},
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
            kWarmups,
            kRepeats);
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
            kWarmups,
            kRepeats);
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
            result.emplace_back(
                new FRA_(today, Date::AddMonths(today, startMonth), Date::AddMonths(today, startMonth + 3), 0.0, projected3m));
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
            result.emplace_back(new BasisSwap_(today,
                                               today,
                                               Date::AddMonths(today, 12 * year),
                                               0.0,
                                               projected3m,
                                               quarterlyLeg,
                                               projected6m,
                                               semiannualLeg));
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

    void RunBasket(const std::string& label, const std::vector<RateHandle_>& rates, const YieldCurve_& market) {
        double checksum = 0.0;
        const Bench::Result_ raw = Bench::Run(
            label,
            [&]() {
                for (int pass = 0; pass < kBasketPasses; ++pass)
                    for (const auto& rate : rates)
                        checksum += (*rate)(market);
            },
            kWarmups,
            kRepeats);
        Bench::DoNotOptimize(&checksum);
        REQUIRE(std::isfinite(checksum) && std::fabs(checksum) > 0.0, "Pricing BASKET checksum must be finite and non-zero");
        Bench::Print(Normalize(raw, label + " BASKET / pass", kBasketPasses));
        Bench::Print(Normalize(raw, label + " PER-INSTRUMENT", kBasketPasses * static_cast<int64_t>(rates.size())));
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
    RunBasket("Discount 20-instrument", discountRates, *market);
    RunBasket("3M projection 20-instrument", projectionRates, *market);
    RunBasket("3M-vs-6M PASSIVE 10-instrument", basisRates, *market);
    return 0;
}
```

- [ ] **Step 5: Format the new source without reordering platform headers**

Run:

```bash
clang-format -i -sort-includes=0 dal-cpp/benchmarks/ycinstrument_perf/ycinstrument_perf.cpp
git diff --check
```

Expected: both commands exit zero; `dal/platform/platform.hpp` remains the first include.

- [ ] **Step 6: Build the green target**

Run:

```bash
cmake --build build --target ycinstrument_perf -j2
```

Expected: exit zero and `build/dal-cpp/benchmarks/ycinstrument_perf/ycinstrument_perf` exists.

- [ ] **Step 7: Run the executable and verify its output contract**

Run:

```bash
output=$(./build/dal-cpp/benchmarks/ycinstrument_perf/ycinstrument_perf)
print -- "$output"
printf '%s\n' "$output" | rg -c 'PRECOMPUTE| PRICE | BASKET |PER-INSTRUMENT'
for instrument in Deposit_ FRA_ Future_ STIR_ Swap_ OISSwap_ BasisSwap_; do
    printf '%s\n' "$output" | rg -q "$instrument"
done
printf '%s\n' "$output" | rg -q 'BasisSwap_.*PASSIVE'
```

Expected: the count command prints `20`; all searches exit zero; the executable prints no warning or exception.

- [ ] **Step 8: Run the focused correctness suite**

Run:

```bash
cmake --build build --target dal_cpp_tests -j2
./build/dal-cpp/dal_cpp_tests --gtest_filter='YCInstrumentTest.*' --gtest_brief=1
```

Expected: build exits zero and every `YCInstrumentTest` passes.

- [ ] **Step 9: Record the task checkpoint**

Run:

```bash
git status --short
git diff --check
git diff --stat
```

Expected: only the design/plan plus the three Task 1 benchmark paths are changed; no build artifacts are tracked. Commit the verified Task 1 implementation in the isolated feature checkout.

---

### Task 2: Publish the benchmark in Linux and Windows CI reports

**Files:**
- Modify: `.github/workflows/cmake-linux.yml`
- Modify: `.github/workflows/cmake-windows.yml`
- Test: `.github/workflows/cmake-linux.yml`
- Test: `.github/workflows/cmake-windows.yml`

**Interfaces:**
- Consumes: installed CMake target `ycinstrument_perf` from Task 1.
- Produces: CI execution and summary output for `ycinstrument_perf` on both supported platforms.

- [ ] **Step 1: Run the red inventory check**

Run:

```bash
python3 - <<'PY'
from pathlib import Path

for name in ("cmake-linux.yml", "cmake-windows.yml"):
    text = Path(".github/workflows", name).read_text()
    assert "ycinstrument_perf" in text, f"{name} does not report ycinstrument_perf"
PY
```

Expected: `AssertionError: cmake-linux.yml does not report ycinstrument_perf`.

- [ ] **Step 2: Add the Linux reporting entry**

In `.github/workflows/cmake-linux.yml`, change the benchmark array tail to:

```yaml
            iv_brent_perf
            script_mc_perf
            ycinstrument_perf
            curve_calibration_perf
          )
```

- [ ] **Step 3: Add the Windows reporting entry**

In `.github/workflows/cmake-windows.yml`, change the benchmark array tail to:

```yaml
            iv_brent_perf
            script_mc_perf
            ycinstrument_perf
            curve_calibration_perf
          )
```

- [ ] **Step 4: Run the green inventory check and protect the regression gate boundary**

Run:

```bash
python3 - <<'PY'
from pathlib import Path

for name in ("cmake-linux.yml", "cmake-windows.yml"):
    text = Path(".github/workflows", name).read_text()
    assert text.count("ycinstrument_perf") == 1, f"{name} must report ycinstrument_perf exactly once"

gate = Path(".github/scripts/check_benchmark_regressions.py").read_text()
assert "ycinstrument_perf" not in gate, "new pricing timings must remain report-only"
PY
```

Expected: exit zero with no output.

- [ ] **Step 5: Run documentation/workflow integrity and record the task checkpoint**

Run:

```bash
python3 .github/scripts/check_docs.py
git diff --check
git status --short
```

Expected: documentation checks pass, diff check exits zero, and only intended feature files are listed. Do not alter the pre-existing `threadpool_perf`/`stacks_perf` workflow-list drift in this task.

---

### Task 3: Verify runtime behavior and characterize benchmark noise

**Files:**
- Verify: `dal-cpp/benchmarks/ycinstrument_perf/ycinstrument_perf.cpp`
- Verify: `dal-cpp/tests/curve/test_ycinstrument.cpp`
- Verify: `dal-cpp/benchmarks/CMakeLists.txt`
- Verify: `.github/workflows/cmake-linux.yml`
- Verify: `.github/workflows/cmake-windows.yml`

**Interfaces:**
- Consumes: completed benchmark executable and CI registration from Tasks 1-2.
- Produces: fresh build/test evidence and a ten-run noise report for every benchmark row.

- [ ] **Step 1: Rebuild the exact final sources**

Run:

```bash
cmake --build build --target ycinstrument_perf dal_cpp_tests -j2
```

Expected: exit zero with both targets freshly linked.

- [ ] **Step 2: Run the benchmark ten times and summarize observed spread in memory**

Run:

```bash
python3 - <<'PY'
import re
import subprocess

binary = "./build/dal-cpp/benchmarks/ycinstrument_perf/ycinstrument_perf"
rows = {}
for run in range(10):
    output = subprocess.check_output([binary], text=True)
    measured = [line for line in output.splitlines() if re.search(r"PRECOMPUTE| PRICE | BASKET |PER-INSTRUMENT", line)]
    assert len(measured) == 20, f"run {run + 1}: expected 20 rows, got {len(measured)}"
    for line in measured:
        name = line[:75].rstrip()
        match = re.search(r"([0-9.]+)\s+(ns|us|ms)\s+([0-9.]+)\s+(ns|us|ms)\s+([0-9.]+)\s+(ns|us|ms)", line[75:])
        assert match, f"cannot parse benchmark row: {line}"
        scale = {"ns": 1.0, "us": 1.0e3, "ms": 1.0e6}
        median_ns = float(match.group(1)) * scale[match.group(2)]
        rows.setdefault(name, []).append(median_ns)

for name, values in rows.items():
    best = min(values)
    worst = max(values)
    spread = 100.0 * (worst - best) / best
    print(f"{name}: best={best:.3f}ns worst={worst:.3f}ns spread={spread:.2f}%")
PY
```

Expected: ten successful invocations, 20 parsed rows per invocation, and a printed best/worst/spread line for every row. Report the observed spread; do not fail the task because a noisy shared runner exceeds 4%.

- [ ] **Step 3: Run focused and full core tests**

Run:

```bash
./build/dal-cpp/dal_cpp_tests --gtest_filter='YCInstrumentTest.*' --gtest_brief=1
./build/dal-cpp/dal_cpp_tests --gtest_brief=1
```

Expected: both commands exit zero; the first reports only `YCInstrumentTest`, and the second reports zero failed tests.

- [ ] **Step 4: Run final static and scope checks**

Run:

```bash
python3 .github/scripts/check_docs.py
git diff --check
git status --short
git diff --stat
rg -n "ycinstrument_perf" dal-cpp/benchmarks/CMakeLists.txt .github/workflows/cmake-linux.yml .github/workflows/cmake-windows.yml
rg -n "ycinstrument_perf" .github/scripts/check_benchmark_regressions.py && exit 1 || true
```

Expected: documentation and diff checks pass; the target appears once in CMake and once per workflow; it does not appear in the regression-gate script.

---

### Task 4: Complete DAL performance and code review gates

**Files:**
- Review: all files changed from `master` for this feature.
- Artifact: `.codex/artifacts/perf/ycinstrument-pricing-performance.md`
- Artifact: `.codex/artifacts/reviews/ycinstrument-pricing-performance.md`

**Interfaces:**
- Consumes: final diff and verification evidence from Tasks 1-3.
- Produces: a performance assessment and merge-readiness verdict with no open blocking findings.

- [ ] **Step 1: Run the DAL performance review**

Use `dal-performancer` to inspect the fixture, timed-region boundaries, batch duration, output normalization, repeated-run noise, and CI gate placement. Write the evidence and verdict to:

```text
.codex/artifacts/perf/ycinstrument-pricing-performance.md
```

The report must include the ten-run sample count, environment/build configuration, observed spread, and a statement that there is no baseline target on `master`, so no branch-vs-baseline regression conclusion is possible for this new executable.

- [ ] **Step 2: Run the DAL code review**

Use `dal-reviewer` to review the complete feature diff for pricing correctness, routing, cross-platform CMake/YAML integration, stable labels, dead-code-elimination protection, test evidence, and unintended production changes. Write findings-first output and an `Approve` or `Request Changes` verdict to:

```text
.codex/artifacts/reviews/ycinstrument-pricing-performance.md
```

- [ ] **Step 3: Resolve every blocking review finding and re-run its covering gate**

For each blocking finding, add or tighten an untimed benchmark correctness requirement before changing benchmark behavior, verify that the requirement fails for the reported defect, implement the smallest fix, and re-run:

```bash
cmake --build build --target ycinstrument_perf -j2
./build/dal-cpp/benchmarks/ycinstrument_perf/ycinstrument_perf
./build/dal-cpp/dal_cpp_tests --gtest_filter='YCInstrumentTest.*' --gtest_brief=1
```

Append the fix evidence to both review artifacts and repeat review until neither report contains a blocking finding.

- [ ] **Step 4: Produce the final handoff**

Run:

```bash
git status --short
git diff --check
git diff --stat
```

Report:

- exact files changed;
- 20 benchmark labels and representative timing ranges;
- focused/full test counts;
- documentation and scope-check results;
- performance-review and code-review verdicts;
- the isolated-checkout workaround for the original checkout's `.git/index.lock` restriction;
- the dedicated branch and PR URL requested by the user.

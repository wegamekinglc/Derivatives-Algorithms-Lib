# XCCY Examples and Benchmark Coverage Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add self-validating C++ and Python reset-aware XCCY examples, expand `xccy_perf` to 24 stable cases, execute it as Linux/Windows smoke coverage, and add the missing domestic-spread pricing test to PR #230.

**Architecture:** Keep production pricing, calibration, bindings, and serialization unchanged. Build deterministic fixtures entirely in examples, tests, and the existing benchmark target; reuse immutable fixing snapshots and public calibration surfaces, then wire the completed benchmark only into head-side CI smoke lists.

**Tech Stack:** C++17, DAL core/public APIs, Google Test, CMake, Python 3.10+ with pybind11 bindings, GitHub Actions YAML, DAL benchmark harness.

## Global Constraints

- Work from the isolated writable clone rooted at `/tmp/dal-pr230-examples.7HurRS`, based on PR #230 head `4523ff5a8bd1d02c0b48afc85f4a1cacad5a3c66`.
- Treat `.codex/artifacts/specs/xccy-examples-benchmarks.md` as controlling requirements.
- Do not change production API, binding, enum, serialization, or pricing/calibration behavior.
- Preserve every existing example name and all 21 existing `xccy_perf` labels and workloads.
- `xccy_perf` must emit exactly 24 unique rows and remain informational; do not add it to `.github/scripts/check_benchmark_regressions.py`.
- Use fixed dates, explicit calendars/conventions, deterministic quotes, immutable snapshots, and no shared mutable test state.
- Follow `.clang-format`; keep `<gtest/gtest.h>` first; use `TEST`, `ASSERT_*`, and test names beginning with `Test`.
- Keep documentation current-state only; implementation history stays out of `docs/` and README files.
- Run new C++ code on GCC/Clang locally and rely on exact-head CI for MSVC portability.

---

## File Map

| Path | Responsibility |
|------|----------------|
| `dal-cpp/tests/curve/test_xccypricing.cpp` | Hand-calculated domestic-leg-spread coverage only |
| `dal-cpp/examples/xccy_reset_pricing/xccy_reset_pricing.cpp` | Focused notional-mode and started-fixing tutorial |
| `dal-cpp/examples/xccy_reset_pricing/CMakeLists.txt` | New executable and install wiring |
| `dal-cpp/examples/CMakeLists.txt` | Example discovery and platform options |
| `docs/methodology/xccy_calibration.md` | Current runnable C++ example listing |
| `dal-python/examples/007.xccy_joint_calibration.py` | Installed-surface reset-aware joint calibration tutorial |
| `dal-python/README.md` | Python example discovery and run command |
| `dal-cpp/benchmarks/xccy_perf/xccy_perf.cpp` | Started basket, union snapshot, mixed staged calibration, 24-row matrix |
| `.github/workflows/cmake-linux.yml` | Head-only Linux `xccy_perf` smoke execution |
| `.github/workflows/cmake-windows.yml` | Windows `xccy_perf` smoke execution |

## Specification Traceability

| Requirements | Implemented and verified by |
|--------------|-----------------------------|
| FR1-FR4 | Task 2 C++ example, CMake registration, self-validation, and explicit run |
| FR5-FR6 | Task 3 Python installed-surface example, optimization-proof checks, and explicit runs |
| FR7-FR9 | Task 4 union snapshot, started basket, reset-aware staged row, and ordered 24-row assertion |
| FR10 | Task 5 Linux/Windows head smoke arrays and regression-script exclusion |
| FR11 | Task 1 independent domestic-spread formula and focused suite |
| FR12 | Tasks 2-3 current-state methodology/README discovery updates |
| Performance | Task 4 AADET/4-thread timing and advisory-threshold report; Task 6 full workflow |
| Determinism and compatibility | Tasks 1-4 fixed fixtures and Task 6 whole-diff review |
| Portability | Task 2 CMake platform options and Task 6 exact-head compiler/MSVC CI |
| Differentiability | Task 4 analytic-diagnostics row and Jacobian dimension validation |

---

### Task 1: Cover the domestic-leg XCCY par quote

**Files:**
- Modify: `dal-cpp/tests/curve/test_xccypricing.cpp`, after `CustomPricingMarket_` and before `TestFxResetAtValuationUsesSuppliedFixing`
- Test: `dal-cpp/tests/curve/test_xccypricing.cpp`

**Interfaces:**
- Consumes: `MakeQuarterlyConfig`, `CustomPricingMarket_`, `BuildXccyCashflowPlan`, `PriceXccyParSpread<double>`
- Produces: `TEST(XccyPricingTest, TestDomesticLegParSpreadMatchesHandCalculation)`; no production interface

- [ ] **Step 1: Add the independent hand-calculated coverage test**

```cpp
TEST(XccyPricingTest, TestDomesticLegParSpreadMatchesHandCalculation) {
    auto config = MakeQuarterlyConfig(XccyNotionalMode_::Value_::FIXED);
    config.convention_.initialNotionalExchange_ = false;
    config.convention_.finalNotionalExchange_ = false;
    config.convention_.spreadOnForeignLeg_ = false;

    const auto plan = BuildXccyCashflowPlan(Date_(2024, 1, 4), Date_(2024, 7, 4), config);
    ASSERT_EQ(plan.domesticPeriods_.size(), 2U);
    ASSERT_EQ(plan.foreignPeriods_.size(), 2U);

    constexpr double domesticDf = 0.96;
    constexpr double foreignDf = 0.99;
    constexpr double basisDf = 0.98;
    constexpr double spot = 1.10;
    const CustomPricingMarket_ curves(domesticDf, foreignDf, basisDf, spot);
    const auto market = curves.View(DateTime_(Date_(2024, 1, 1), 12, 0));

    double domesticPv = 0.0;
    double domesticAnnuity = 0.0;
    for (const auto& period : plan.domesticPeriods_) {
        const double annuity = config.domesticNotional_ * period.accrual_.dcf_ * domesticDf;
        const double forwardRate = (1.0 / domesticDf - 1.0) / period.accrual_.dcf_;
        domesticPv += forwardRate * annuity;
        domesticAnnuity += annuity;
    }

    const double foreignConversion = spot * foreignDf / basisDf;
    double foreignPv = 0.0;
    for (const auto& period : plan.foreignPeriods_) {
        const double annuity = config.foreignNotional_ * period.accrual_.dcf_ * foreignConversion;
        const double forwardRate = (1.0 / foreignDf - 1.0) / period.accrual_.dcf_;
        foreignPv += forwardRate * annuity;
    }

    const double expectedParSpread = (foreignPv - domesticPv) / domesticAnnuity;
    ASSERT_LT(expectedParSpread, 0.0);
    ASSERT_NEAR(PriceXccyParSpread<double>(plan, market, MarketFixingSnapshot_()), expectedParSpread, 1.0e-12);
}
```

This is coverage of existing production behavior, so immediate success is expected. If it fails, stop for scope review instead of changing production code.

- [ ] **Step 2: Build and run the focused test**

Run:

```bash
cmake --build build/Release-linux --target dal_cpp_tests --parallel 4
build/Release-linux/dal-cpp/dal_cpp_tests \
  --gtest_filter='XccyPricingTest.TestDomesticLegParSpreadMatchesHandCalculation'
```

Expected: one passing test.

- [ ] **Step 3: Run the entire XCCY pricing suite**

```bash
build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='XccyPricingTest.*'
```

Expected: all `XccyPricingTest` cases pass.

- [ ] **Step 4: Commit only the test**

```bash
git add dal-cpp/tests/curve/test_xccypricing.cpp
git diff --cached --name-status
git diff --cached --check
git commit -m "test(xccy): cover domestic-leg par spread"
```

Expected staged path: only `dal-cpp/tests/curve/test_xccypricing.cpp`.

---

### Task 2: Add the self-validating C++ reset-pricing example

**Files:**
- Create: `dal-cpp/examples/xccy_reset_pricing/xccy_reset_pricing.cpp`
- Create: `dal-cpp/examples/xccy_reset_pricing/CMakeLists.txt`
- Modify: `dal-cpp/examples/CMakeLists.txt`
- Modify: `docs/methodology/xccy_calibration.md`

**Interfaces:**
- Consumes: `BuildXccyCashflowPlan`, `RequiredHistoricalFixings`, `ResolveXccyNotionals<double>`, `PriceXccyParSpread<double>`, `CrossCurrencySwap_::Precompute`, `CrossCurrencyMarket_`
- Produces: installed executable `xccy_reset_pricing`; local `ModeResult_`, `MarketFixture_`, `Config`, `EvaluateFutureMode`, `SnapshotFor`, `RunExample`

- [ ] **Step 1: Verify the target is absent before implementation**

```bash
cmake --build build/Release-linux --target xccy_reset_pricing --parallel 4
```

Expected: failure because the target does not exist.

- [ ] **Step 2: Create the deterministic fixture and result model**

Use the following local types and fixture values in `xccy_reset_pricing.cpp`:

```cpp
struct ModeResult_ {
    const char* label_;
    int periodCount_ = 0;
    int resetCount_ = 0;
    int mtmDeltaCount_ = 0;
    double nextDomesticNotional_ = 0.0;
    double parQuote_ = 0.0;
};

const Date_ today(2025, 1, 16);
const Vector_<Date_> knots = {
    Date::AddMonths(today, 6), Date::AddMonths(today, 18), Date::AddMonths(today, 36),
};
```

`MarketFixture_` must own USD/EUR OIS and 3M `CurveBlock_` handles plus a USD basis curve, then expose:

```cpp
CrossCurrencyMarket_ Market(const DateTime_& valuationTime,
                            const Handle_<MarketFixingSnapshot_>& fixings) const;
```

Build piecewise-constant term structures with these forward values:

```text
USD OIS   0.015, 0.022, 0.030
USD 3M    0.025, 0.035, 0.050
EUR OIS   0.008, 0.012, 0.018
EUR 3M    0.018, 0.025, 0.032
USD basis 0.001, 0.003, 0.006
FX spot   1.10
```

Do not set `XGLOBAL`; valuation time is explicit.

- [ ] **Step 3: Implement the common configuration**

```cpp
CrossCurrencySwapConfig_ Config(XccyNotionalMode_ mode) {
    CrossCurrencySwapConfig_ result;
    result.pair_ = CurrencyPair_(Ccy_("USD"), Ccy_("EUR"));
    result.domesticNotional_ = 110.0;
    result.foreignNotional_ = 100.0;
    result.notionalMode_ = mode;
    result.convention_.initialNotionalExchange_ = true;
    result.convention_.finalNotionalExchange_ = true;
    result.convention_.spreadOnForeignLeg_ = true;
    result.convention_.domesticIndex_ = IndexConvention();
    result.convention_.foreignIndex_ = IndexConvention();
    result.convention_.domesticLeg_ = LegConvention();
    result.convention_.foreignLeg_ = LegConvention();
    result.fxReset_.fixingLag_ = 0;
    result.fxReset_.fixingHolidays_ = Holidays::None();
    result.fxReset_.fixingConvention_ = BizDayConvention_("Unadjusted");
    result.fxReset_.fixingHour_ = 10;
    result.fxReset_.fixingMinute_ = 30;
    result.domesticRateFixing_ = {"USD-XCCY-RESET-3M", 11, 0};
    result.foreignRateFixing_ = {"EUR-XCCY-RESET-3M", 11, 0};
    return result;
}
```

Both legs use 3M, ACT/365F, OIS collateral, no holidays, unadjusted dates; both indices enable projection curves.

- [ ] **Step 4: Implement and validate the future-mode table**

Use `start = Date::AddMonths(today, 3)`, `maturity = Date::AddMonths(start, 24)`, and valuation `DateTime_(today, 9, 0)`. For `FIXED`, `RESETTABLE`, and `MARK_TO_MARKET`, build one plan and swap, compute the par quote via `Precompute`, and resolve notionals through a local `XccyMarketView_<double>` assembled from `CrossCurrencyMarket_` accessors exactly as `CrossCurrencySwapKernelRate_` does.

The validation must implement these exact rules:

```cpp
fixed.resetCount_ == 0 && fixed.mtmDeltaCount_ == 0;
resettable.resetCount_ == resettable.periodCount_ - 1 && resettable.mtmDeltaCount_ == 0;
mtm.resetCount_ == mtm.periodCount_ - 1 && mtm.mtmDeltaCount_ == mtm.periodCount_ - 1;
std::fabs(fixed.parQuote_ - resettable.parQuote_) > 1.0e-12;
std::fabs(fixed.parQuote_ - mtm.parQuote_) > 1.0e-12;
std::fabs(resettable.parQuote_ - mtm.parQuote_) > 1.0e-12;
```

All quotes and next notionals must be finite. Print mode, reset count, MTM-delta count, next domestic notional, and par quote in basis points.

- [ ] **Step 5: Implement and validate the started-MTM snapshot section**

Use valuation `DateTime_(today, 12, 0)`, start `Date::AddMonths(today, -3)`, maturity `Date::AddMonths(start, 24)`, and MTM mode. For every request from `RequiredHistoricalFixings`, populate exactly one of:

```cpp
values["USD-XCCY-RESET-3M"][request.fixingTime_] = 0.040;
values["EUR-XCCY-RESET-3M"][request.fixingTime_] = 0.030;
values[FxIndexName(config.pair_)][request.fixingTime_] = 1.20;
```

Reject unknown identities. Require a non-empty request set containing USD, EUR, and FX requests, retain the immutable snapshot in the market, print every request, compute the par quote, and require it to be finite.

- [ ] **Step 6: Add explicit process failure behavior**

```cpp
int main() {
    try {
        RegisterAll_::Init();
        return RunExample() ? 0 : 1;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
```

- [ ] **Step 7: Register the target and install rule**

Create `dal-cpp/examples/xccy_reset_pricing/CMakeLists.txt`:

```cmake
file(GLOB_RECURSE XCCY_RESET_PRICING_FILES CONFIGURE_DEPENDS "*.hpp" "*.cpp")
add_executable(xccy_reset_pricing ${XCCY_RESET_PRICING_FILES})
target_link_libraries(xccy_reset_pricing dal_library)

if(DAL_USE_XAD_AAD)
    target_link_libraries(xccy_reset_pricing XAD::xad)
elseif(DAL_USE_CODIPACK_AAD)
    target_link_libraries(xccy_reset_pricing CoDiPack)
elseif(DAL_USE_ADEPT_AAD)
    target_link_libraries(xccy_reset_pricing adept)
endif()

if(NOT MSVC)
    target_link_libraries(xccy_reset_pricing pthread)
endif()

install(TARGETS xccy_reset_pricing
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
```

Add `add_subdirectory(xccy_reset_pricing)` and `xccy_reset_pricing` to the matching lists in `dal-cpp/examples/CMakeLists.txt`.

- [ ] **Step 8: Build, run, format, and document**

```bash
cmake --preset Release-linux -S . -B build/Release-linux
cmake --build build/Release-linux --target xccy_reset_pricing --parallel 4
build/Release-linux/dal-cpp/examples/xccy_reset_pricing/xccy_reset_pricing
clang-format -i -sort-includes=0 dal-cpp/examples/xccy_reset_pricing/xccy_reset_pricing.cpp
cmake --build build/Release-linux --target xccy_reset_pricing --parallel 4
build/Release-linux/dal-cpp/examples/xccy_reset_pricing/xccy_reset_pricing
```

Expected: counts `0/0`, `7/0`, `7/7`; finite pairwise-distinct quotes; historical USD/EUR/FX requests; exit 0.

Replace the singular end-to-end example paragraph in `docs/methodology/xccy_calibration.md` with a current-state `Runnable Examples` section listing `xccy_reset_pricing` and `xccy_mtm_calibration`.

- [ ] **Step 9: Commit the C++ example slice**

```bash
git add dal-cpp/examples/CMakeLists.txt \
  dal-cpp/examples/xccy_reset_pricing/CMakeLists.txt \
  dal-cpp/examples/xccy_reset_pricing/xccy_reset_pricing.cpp \
  docs/methodology/xccy_calibration.md
git diff --cached --name-status
git diff --cached --check
git commit -m "feat(xccy): add reset pricing example"
```

---

### Task 3: Add the installed-surface Python joint example

**Files:**
- Create: `dal-python/examples/007.xccy_joint_calibration.py`
- Modify: `dal-python/README.md`

**Interfaces:**
- Consumes: bound builders and properties already exercised by `dal-python/tests/test_xccy_joint.py` and `test_xccy_resettable.py`
- Produces: executable `main() -> int`, `require`, `validate_ranges`, `validate_result`; no binding changes

- [ ] **Step 1: Verify the script is absent**

```bash
dal-python/.venv/bin/python dal-python/examples/007.xccy_joint_calibration.py
```

Expected: failure because the file does not exist.

- [ ] **Step 2: Define exact dates, observations, and failure helper**

```python
TODAY = dal.Date_(2025, 6, 20)
START = dal.Date_(2025, 3, 20)
XCCY_MATURITY = dal.Date_(2026, 3, 20)
CURVE_MATURITY = dal.Date_(2026, 6, 20)
VALUATION_TIME = dal.DateTime_(TODAY, 12, 0)
TOLERANCE = 1.0e-9

def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)
```

Build a snapshot with USD observations `0.040` at `START 11:00` and `0.041` at `TODAY 11:00`; EUR observations `0.030` and `0.031` at the same respective times; and `FX[EUR/USD] = 1.20` at `TODAY 10:30`.

- [ ] **Step 3: Build the known-convergent three-block joint fixture**

Use only bound names. Create one OIS deposit declaration for USD at `0.040`, one for EUR at `0.030`, and one knot per declaration at `CURVE_MATURITY`. Each declaration uses ACT/365F, OIS collateral, `PIECEWISE_CONSTANT_FWD`, `LOG_LINEAR`, discount calibration, and `initial_guess_per_node = [market_rate]`.

Build one started MTM XCCY swap with 3M ACT/365F legs and non-projection OIS indices, initial/final exchanges, foreign-leg spread, USD/EUR fixing identities at 11:00, FX reset lag 0/FOLLOWING/10:30, notionals 110/100, and quoted spread `0.001`.

The basis declaration has that instrument, knot `XCCY_MATURITY`, PWC-forward parameterization, smoothing weight 1.0, and initial guess `[0.001]`. The joint builder uses the explicit snapshot, USD collateral, FX spot 1.10, exact mode defaults, initial guess 0.01, tolerance `TOLERANCE`, and 400 evaluations.

- [ ] **Step 4: Implement optimization-proof result validation**

```python
def validate_ranges(ranges, total: int, label: str) -> None:
    expected_offset = 0
    for block in ranges:
        require(block.offset == expected_offset, f"{label} range {block.name} is not contiguous")
        require(block.size > 0, f"{label} range {block.name} is empty")
        expected_offset += block.size
    require(expected_offset == total, f"{label} ranges cover {expected_offset}, expected {total}")

def validate_result(result) -> None:
    require(result.converged, "joint XCCY calibration did not converge")
    require(len(result.residuals) == result.jacobian_at_solution.rows(), "residual/Jacobian row mismatch")
    validate_ranges(result.residual_ranges, len(result.residuals), "residual")
    validate_ranges(result.parameter_ranges, result.jacobian_at_solution.cols(), "parameter")
    forwards = result.fx_forward_curve.forwards
    require(len(forwards) == len(result.fx_forward_curve.dates) and len(forwards) > 0, "invalid FX forward layout")
    require(all(math.isfinite(value) for value in forwards), "non-finite FX forward")
    require(math.isfinite(result.joint_max_abs_residual), "non-finite maximum residual")
    require(result.joint_max_abs_residual <= TOLERANCE, "maximum residual exceeds tolerance")
```

Use no bare `assert`; checks must remain active under `python -O`.

- [ ] **Step 5: Print diagnostics and add the module entry point**

Print convergence, maximum residual, Jacobian dimensions, named parameter and residual half-open ranges, and every FX-forward date/value. Use:

```python
if __name__ == "__main__":
    raise SystemExit(main())
```

- [ ] **Step 6: Build and run normally and under optimization**

```bash
UV_CACHE_DIR=/tmp/dal-pr230-uv-cache NUM_CORES=4 bash ./build_linux.sh --full
PYTHONPATH="$PWD/build/stage/Release-linux" \
  dal-python/.venv/bin/python dal-python/examples/007.xccy_joint_calibration.py
PYTHONPATH="$PWD/build/stage/Release-linux" \
  dal-python/.venv/bin/python -O dal-python/examples/007.xccy_joint_calibration.py
```

Expected: converged `3x3` solve, three contiguous parameter ranges, three contiguous residual ranges, finite FX forwards, residual at most `1e-9`, exit 0 in both modes.

- [ ] **Step 7: Document and commit the Python slice**

Under the current resettable/joint XCCY section in `dal-python/README.md`, link the script, state that it uses an explicit started-trade snapshot, list the printed diagnostics, and show the installed-package run command. Then:

```bash
git add dal-python/examples/007.xccy_joint_calibration.py dal-python/README.md
git diff --cached --name-status
git diff --cached --check
git commit -m "feat(python): add joint XCCY example"
```

---

### Task 4: Expand `xccy_perf` to the exact 24-row matrix

**Files:**
- Modify: `dal-cpp/benchmarks/xccy_perf/xccy_perf.cpp`

**Interfaces:**
- Consumes: `BuildXccyCashflowPlan`, `RequiredHistoricalFixings`, `CrossCurrencySwap_::TimeSpan`, existing `PricingCase_`, `MakeConfig`, `PrecomputeAll`, basis calibration helpers
- Produces: `HistoricalFixingValue`, `MakeHistoricalFixingSnapshot`, `MakeResetAwareCalibrationSpec`; 24 stable rows

- [ ] **Step 1: Record the 21-row baseline**

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF \
  -DDAL_BUILD_PUBLIC=OFF -DDAL_CPP_BUILD_TESTS=OFF \
  -DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_CPP_BUILD_BENCHMARKS=ON \
  -DDAL_BUILD_PYTHON=OFF -DDAL_USE_XAD_AAD=OFF \
  -DDAL_USE_CODIPACK_AAD=OFF -DDAL_USE_ADEPT_AAD=OFF \
  -DDAL_ENABLE_NATIVE_ARCH=ON
cmake --build build --target xccy_perf --parallel 4
DAL_NUM_THREADS=4 build/dal-cpp/benchmarks/xccy_perf/xccy_perf > /tmp/xccy_perf-before.txt
```

Expected: 21 rows; the two started-basket labels and reset-aware calibration label are absent.

- [ ] **Step 2: Add one authoritative union snapshot helper**

```cpp
double HistoricalFixingValue(const FixingRequest_& request, const CrossCurrencySwapConfig_& config) {
    if (request.indexName_ == config.domesticRateFixing_.indexName_)
        return 0.04;
    if (request.indexName_ == config.foreignRateFixing_.indexName_)
        return 0.03;
    REQUIRE(request.indexName_ == FxIndexName(config.pair_), "Unexpected XCCY benchmark fixing identity");
    return 1.20;
}

Handle_<MarketFixingSnapshot_> MakeHistoricalFixingSnapshot(const std::vector<SwapHandle_>& instruments,
                                                            const DateTime_& valuationTime,
                                                            const CrossCurrencySwapConfig_& config);
```

For each instrument, obtain its start/maturity via `TimeSpan()`, build its plan, collect `RequiredHistoricalFixings`, and insert into one nested map. Existing keys must have the same value; `REQUIRE` on conflicts. Construct one immutable snapshot after all requests are deduplicated and before any `Precompute` call.

- [ ] **Step 3: Populate the ten-instrument started-MTM basket**

Replace `MakeInProgressMtmCase` with ten MTM swaps sharing `start = Date::AddMonths(fixture.today_, -3)` and maturities `Date::AddMonths(start, 12 * year)` for years 1 through 10. Build the union snapshot first, construct one intraday market with a 0.0020 basis curve, then precompute all rates and return:

```cpp
return PricingCase_{"in-progress MTM", instruments.back(), rates.back(), rates, market};
```

Run the benchmark. Expected: 23 rows and these two new labels:

```text
XCCY in-progress MTM 10-instrument BASKET / pass
XCCY in-progress MTM 10-instrument PER-INSTRUMENT
```

- [ ] **Step 4: Add the mixed reset-aware staged fixture**

Implement:

```cpp
CrossCurrencyCalibrationSpec_ MakeResetAwareCalibrationSpec(const Fixture_& fixture);
```

Use valuation `DateTime_(fixture.today_, 12, 0)`, one started MTM trade from `today - 3M` to `start + 12M`, and fourteen future trades starting `today + 1M` with maturities at 12, 18, 24, 30, 36, 42, 48, 54, 60, 72, 84, 96, 108, and 120 months from today. Alternate future `RESETTABLE`/`MARK_TO_MARKET`, starting with `RESETTABLE`.

Build one union snapshot across all 15 zero-quote prototypes. Quote them from the known intraday market using existing blocks, FX 1.10, USD collateral, snapshot, and the existing five-knot 0.0020 PWC basis curve. Populate the staged spec with the same blocks, knots, snapshot, tolerance `1e-8`, fit tolerance `1e-7`, and initial guess 0.0.

Extend the untimed `ValidateBasisCalibration` check so diagnostic vectors have 15 entries, analytic forward Jacobian is `15x5`, and effective inverse is `5x15` when requested.

- [ ] **Step 5: Add exactly one timed staged row**

```cpp
const CrossCurrencyCalibrationSpec_ resetAwareBasisSpec = MakeResetAwareCalibrationSpec(fixture);
ValidateBasisCalibration(resetAwareBasisSpec, basisAnalyticDiagnostics);
RunBasisCalibration("XCCY reset-aware basis ANALYTIC +DIAG (15 instruments, 5 knots)",
                    resetAwareBasisSpec,
                    basisAnalyticDiagnostics);
```

Place the row after the three existing basis-only rows and before joint rows. Add no other reset-aware solver/mode combination.

- [ ] **Step 6: Format, build, time, and verify the matrix**

```bash
clang-format -i -sort-includes=0 dal-cpp/benchmarks/xccy_perf/xccy_perf.cpp
cmake --build build --target xccy_perf --parallel 4
DAL_NUM_THREADS=4 /usr/bin/time -f '%e' -o /tmp/xccy_perf.seconds \
  build/dal-cpp/benchmarks/xccy_perf/xccy_perf > /tmp/xccy_perf.txt
```

Parse benchmark rows with the same regular expression used by `.github/scripts/check_benchmark_regressions.py` and require this exact order:

```python
expected = [
    "XCCY fixed 10Y PRECOMPUTE / operation",
    "XCCY fixed 10Y PRICE / operation",
    "XCCY fixed 10-instrument BASKET / pass",
    "XCCY fixed 10-instrument PER-INSTRUMENT",
    "XCCY resettable 10Y PRECOMPUTE / operation",
    "XCCY resettable 10Y PRICE / operation",
    "XCCY resettable 10-instrument BASKET / pass",
    "XCCY resettable 10-instrument PER-INSTRUMENT",
    "XCCY MTM 10Y PRECOMPUTE / operation",
    "XCCY MTM 10Y PRICE / operation",
    "XCCY MTM 10-instrument BASKET / pass",
    "XCCY MTM 10-instrument PER-INSTRUMENT",
    "XCCY in-progress MTM 10Y PRECOMPUTE / operation",
    "XCCY in-progress MTM 10Y PRICE / operation",
    "XCCY in-progress MTM 10-instrument BASKET / pass",
    "XCCY in-progress MTM 10-instrument PER-INSTRUMENT",
    "XCCY basis-only CALIBRATION (15 instruments, 5 knots)",
    "XCCY basis-only ANALYTIC SOLVE (15 instruments, 5 knots)",
    "XCCY basis-only BUMPED +DIAG (15 instruments, 5 knots)",
    "XCCY reset-aware basis ANALYTIC +DIAG (15 instruments, 5 knots)",
    "XCCY joint ANALYTIC SOLVE (15 XCCY, 3x5 knots)",
    "XCCY joint ANALYTIC +DIAG (15 XCCY, 3x5 knots)",
    "XCCY joint BUMPED +DIAG (15 XCCY, 3x5 knots)",
    "XCCY joint ANALYTIC APPROXIMATE (15 XCCY, 3x5 knots)",
]
assert actual == expected, (len(actual), actual)
assert len(actual) == len(set(actual)) == 24
print("xccy_perf: 24 unique rows")
```

Record `/tmp/xccy_perf.seconds`. Above 15 seconds on the same AADET/4-thread host is an investigation trigger, not a portable failure gate.

- [ ] **Step 7: Commit only the benchmark source**

```bash
git add dal-cpp/benchmarks/xccy_perf/xccy_perf.cpp
git diff --cached --name-status
git diff --cached --check
git commit -m "perf(xccy): extend reset-aware benchmark coverage"
```

---

### Task 5: Run `xccy_perf` in Linux and Windows benchmark smoke jobs

**Files:**
- Modify: `.github/workflows/cmake-linux.yml`
- Modify: `.github/workflows/cmake-windows.yml`
- Must remain unchanged: `.github/scripts/check_benchmark_regressions.py`

**Interfaces:**
- Consumes: installed/built `xccy_perf` from Task 4
- Produces: `### xccy_perf` output in both benchmark job summaries; no regression-gate registration

- [ ] **Step 1: Add the target to both head-side arrays**

Append after `curve_calibration_perf` in each relevant Bash array:

```yaml
            curve_calibration_perf
            xccy_perf
```

Locations: Linux `Run head benchmarks`; Windows `Run benchmarks`. Do not alter the Linux base build or paired comparison commands.

- [ ] **Step 2: Verify exact workflow scope**

```bash
test "$(rg -c '^            xccy_perf$' .github/workflows/cmake-linux.yml)" -eq 1
test "$(rg -c '^            xccy_perf$' .github/workflows/cmake-windows.yml)" -eq 1
if rg -n 'xccy_perf' .github/scripts/check_benchmark_regressions.py; then exit 1; fi
git diff --check
```

Expected: one array entry per workflow, no regression-script entry, clean diff.

- [ ] **Step 3: Commit only CI wiring**

```bash
git add .github/workflows/cmake-linux.yml .github/workflows/cmake-windows.yml
git diff --cached --name-status
git diff --cached --check
git commit -m "ci: run XCCY benchmark smoke coverage"
```

---

### Task 6: Full verification, DAL review, and PR #230 update

**Files:**
- Verify all files changed since `4523ff5a8bd1d02c0b48afc85f4a1cacad5a3c66`
- Do not create process/history documentation outside `.codex/artifacts/`

**Interfaces:**
- Consumes: Tasks 1-5 commits and controlling spec
- Produces: verified exact PR head, updated PR body, no blocking DAL findings

- [ ] **Step 1: Run the full clean workflow**

```bash
UV_CACHE_DIR=/tmp/dal-pr230-uv-cache NUM_CORES=4 bash ./build_linux.sh --full
```

Expected: core/public CTest, direct core/public tests, Python tests, portable Excel smoke, examples, benchmarks, Machinist drift, and documentation checks all succeed.

- [ ] **Step 2: Re-run the new observable surfaces explicitly**

```bash
build/Release-linux/dal-cpp/examples/xccy_reset_pricing/xccy_reset_pricing
PYTHONPATH="$PWD/build/stage/Release-linux" \
  dal-python/.venv/bin/python dal-python/examples/007.xccy_joint_calibration.py
PYTHONPATH="$PWD/build/stage/Release-linux" \
  dal-python/.venv/bin/python -O dal-python/examples/007.xccy_joint_calibration.py
DAL_NUM_THREADS=4 build/dal-cpp/benchmarks/xccy_perf/xccy_perf > /tmp/xccy_perf-final.txt
python3 .github/scripts/check_docs.py
git diff --check
```

Expected: both examples exit 0, optimized Python checks remain active, benchmark emits 24 unique rows, docs/diff checks pass.

- [ ] **Step 3: Run the DAL reviewer gate**

Review `4523ff5a8bd1d02c0b48afc85f4a1cacad5a3c66..HEAD` against every FR/NFR and check:

```text
Domestic-spread sign and domestic annuity are independent and correct.
No production code or binding changed.
C++ example uses explicit state and consumes its snapshot.
Python example uses only bound names and no bare assert.
All 21 old benchmark labels remain unchanged; total is 24 unique rows.
No paired-regression script change exists.
Both workflow arrays contain xccy_perf once.
Docs are current-state only and generated files are untouched.
```

Any blocking finding returns to the owning task, followed by focused and full re-verification.

- [ ] **Step 4: Verify scope and commit state**

```bash
git status --short --branch
git diff --check origin/codex/xccy-resettable-mtm...HEAD
git log --oneline 4523ff5a8bd1d02c0b48afc85f4a1cacad5a3c66..HEAD
```

Expected: clean tree and five scoped implementation commits.

- [ ] **Step 5: Push and update PR metadata**

Fetch first and require the remote PR head to be an ancestor of local `HEAD`; then push with the known SSH-config bypass if the system SSH wrapper fails. Update the PR body through `gh api` if `gh pr edit` hits the deprecated Projects Classic GraphQL error. Include the new examples, 24-row `xccy_perf`, Linux/Windows smoke execution, domestic-spread coverage, and fresh verification totals.

- [ ] **Step 6: Verify exact-head CI and review state**

```bash
head_sha=$(gh pr view 230 --json headRefOid --jq .headRefOid)
git rev-parse HEAD
gh api "repos/wegamekinglc/Derivatives-Algorithms-Lib/commits/${head_sha}/check-runs?per_page=100"
gh pr view 230 --json url,state,isDraft,mergeable,reviewDecision,headRefOid
```

Expected: local, remote branch, and PR head SHAs match; all exact-head check runs complete successfully; PR remains open, ready, and mergeable. Do not merge unless the user explicitly requests it.

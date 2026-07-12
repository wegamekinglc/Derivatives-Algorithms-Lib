# XCCY Resettable/MTM Notional and In-Progress Trades Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add fixed, resettable, and mark-to-market XCCY notionals; deterministic fixing-aware valuation of in-progress trades; staged basis calibration; and simultaneous domestic/foreign/basis calibration with analytic Jacobians across C++, Python, and Excel.

**Architecture:** Precompute an immutable XCCY cashflow plan, resolve historical values from one immutable market-fixing snapshot, and value the plan through one scalar-templated pricing kernel. Basis-only and three-block calibration both call that kernel; the joint solver builds all active curves from one parameter vector and never nests calibration calls.

**Tech Stack:** C++17, DAL curve/discount abstractions, DAL AAD tape and `Underdetermined` solver, Google Test, pybind11/pytest, Machinist-generated Excel bindings, CMake, and DAL's `Bench::Run` micro-benchmark harness.

## Global Constraints

- `XccyNotionalMode_` has exactly `FIXED`, `RESETTABLE`, and `MARK_TO_MARKET`; do not retain two independent notional-mode booleans.
- The foreign notional is fixed. The domestic coupon notional resets from the second domestic accrual period at `foreignNotional * FX(fixingTime)`.
- `MARK_TO_MARKET` additionally exchanges `newDomesticNotional - previousDomesticNotional` on the reset effective date.
- Reset effective dates equal domestic coupon accrual starts. Independent reset schedules, intraperiod resets, and foreign-leg resets are out of scope.
- Pricing and collateral currency must equal the currency pair's domestic currency. Reject third-currency collateral.
- One immutable `MarketFixingSnapshot_` carries domestic rate, foreign rate, and FX reset fixings.
- A fixing strictly before valuation time is mandatory; at valuation time use supplied fixing or the active forward; after valuation time use the active forward.
- Exclude cashflows with `paymentDate < valuationDate`; include `paymentDate == valuationDate` at discount factor one.
- Historical fixings are passive AAD constants. Future rates and FX resets remain differentiable through their active curves.
- Preserve existing fixed-notional C++/Python builders and the staged basis-only calibration entry point.
- The three-block solve uses one parameter vector ordered as domestic curves, foreign curves, then basis; residuals are domestic instruments, foreign instruments, then XCCY instruments.
- Analytic Jacobian requests on the new XCCY paths fail with an eligibility reason instead of silently changing to bumped mode.
- C++ follows `.clang-format` and `.codex/skills/dal-agent-team/references/shared-rules.md`. Tests use `TEST`, `ASSERT_*`, and explicit AAD tape cleanup.
- Functional and Jacobian tests must pass before benchmark measurement.

---

## File Map

### Core protocol, fixings, and pricing

- Create `dal-cpp/dal/curve/xccynotionalmode.hpp`: Machinist enum source.
- Modify `dal-cpp/dal/protocol/rateconvention.hpp`: remove the two notional booleans from `CrossCurrencyConvention_`.
- Modify `dal-cpp/dal/curve/xccyinstrument.hpp`: fixing identities, reset convention, swap config, cashflow-plan access, and legacy constructor adapter.
- Modify `dal-cpp/dal/curve/xccyinstrument.cpp`: plan construction and passive `Rate_` adapter.
- Create `dal-cpp/dal/indice/fixingsnapshot.hpp` and `.cpp`: immutable normalized snapshot and global-store capture.
- Modify `dal-cpp/dal/indice/index/fx.hpp` and `.cpp`: public canonical direct/reverse FX index-name helpers.
- Create `dal-cpp/dal/curve/xccypricing.hpp` and `.cpp`: cashflow-plan structs, typed market view, fixing resolution, and pricing kernel.

### Calibration

- Modify `dal-cpp/dal/curve/xccycalibration.hpp` and `.cpp`: valuation context, domestic collateral currency, fixing snapshot, analytic/bumped options, and kernel integration.
- Create `dal-cpp/dal/curve/jointcalibration_internal.hpp`: reusable curve-slot, parameter slicing, curve construction, smoothing, and diagnostics helpers extracted from `jointcalibration.cpp`.
- Modify `dal-cpp/dal/curve/jointcalibration.cpp`: consume the extracted helpers without behavior change.
- Create `dal-cpp/dal/curve/xccyjointcalibration.hpp` and `.cpp`: the single domestic/foreign/basis solve and layout metadata.

### Tests and surfaces

- Create `dal-cpp/tests/indice/test_fixingsnapshot.cpp`.
- Create `dal-cpp/tests/curve/test_xccypricing.cpp`.
- Modify `dal-cpp/tests/curve/test_xccymarket.cpp`.
- Create `dal-cpp/tests/curve/test_xccyjointcalibration.cpp` and `test_xccyjointjacobian.cpp`.
- Modify `dal-cpp/tests/currency/test_currencydata.cpp`.
- Modify `dal-public/src/curveprotocol.hpp`, `curveinstrument.hpp`, `xccycalibration.hpp`, and `xccycalibration.cpp`.
- Modify `dal-public/tests/test_curve_protocol.cpp`, `test_curve_instrument.cpp`, and `test_xccy_calibration.cpp`.
- Modify `dal-python/src/bindings/curve.cpp`; create `dal-python/tests/test_xccy_resettable.py` and `test_xccy_joint.py`.
- Modify `dal-excel/src/__curve_storable.hpp`, `__curveprotocol.cpp`, `__curveinstrument.cpp`, and `__xccycalibration.cpp`; regenerate core and Excel `dal-*/auto/MG_*` outputs.

### Benchmark and documentation

- Create `dal-cpp/benchmarks/xccy_perf/CMakeLists.txt` and `xccy_perf.cpp`; modify `dal-cpp/benchmarks/CMakeLists.txt`.
- Create `dal-cpp/examples/xccy_mtm_calibration/CMakeLists.txt` and `xccy_mtm_calibration.cpp`; modify `dal-cpp/examples/CMakeLists.txt`.
- Modify `docs/methodology/xccy_calibration.md`, `docs/methodology/yield_curve_jacobian.md`, `docs/public-api.md`, `docs/README.md`, `dal-python/README.md`, `dal-excel/README.md`, and `CHANGELOG.md`.

---

### Task 1: Replace Boolean Notional Flags with a Generated Mode and Config Types

**Files:**
- Create: `dal-cpp/dal/curve/xccynotionalmode.hpp`
- Modify: `dal-cpp/dal/protocol/rateconvention.hpp`
- Modify: `dal-cpp/dal/curve/xccyinstrument.hpp`
- Modify: `dal-cpp/tests/currency/test_currencydata.cpp`
- Modify: `dal-cpp/tests/curve/test_xccymarket.cpp`
- Generate: `dal-cpp/dal/auto/MG_XccyNotionalMode_enum.hpp`
- Generate: `dal-cpp/dal/auto/MG_XccyNotionalMode_enum.inc`
- Generate: matching `dal-excel/auto/MG_XccyNotionalMode_*` artifacts

**Interfaces:**
- Produces: `XccyNotionalMode_`, `FixingIdentity_`, `FxResetConvention_`, and `CrossCurrencySwapConfig_`.
- Preserves: the current `CrossCurrencySwap_` constructor as a `FIXED` adapter until the public facade migrates.

- [ ] **Step 1: Write the failing enum/config tests**

Add assertions that the currency default is one single mode and that reset timing is explicit:

```cpp
TEST(CurrencyDataTest, TestXcsDefaultConventionHasNoNotionalModeBooleans) {
    const CrossCurrencyConvention_& xcs = Ccy::Conventions::Xcs()(Ccy_("USD"));
    ASSERT_TRUE(xcs.initialNotionalExchange_);
    ASSERT_TRUE(xcs.finalNotionalExchange_);
    ASSERT_TRUE(xcs.spreadOnForeignLeg_);
}

TEST(XccyMarketTest, TestResetConfigRequiresExplicitFixingIdentity) {
    CrossCurrencySwapConfig_ config;
    config.notionalMode_ = XccyNotionalMode_::Value_::RESETTABLE;
    ASSERT_THROW(static_cast<void>(CrossCurrencySwap_(Date_(2024, 1, 2), Date_(2024, 1, 4), Date_(2025, 1, 4),
                                                        0.0, config)),
                 Dal::Exception_);
}
```

- [ ] **Step 2: Build to verify the new types are missing**

Run: `cmake --build build --target dal_cpp_tests -j 4`

Expected: compilation fails because `CrossCurrencySwapConfig_` and `XccyNotionalMode_` are undefined.

- [ ] **Step 3: Add Machinist enum markup and core config structs**

Create `xccynotionalmode.hpp` with:

```cpp
/*IF--------------------------------------------------------------------------
enumeration XccyNotionalMode
    Cross-currency notional evolution rule
switchable
alternative FIXED
alternative RESETTABLE
alternative MARK_TO_MARKET
-IF-------------------------------------------------------------------------*/

#pragma once
#include <dal/auto/MG_XccyNotionalMode_enum.hpp>
```

In `xccyinstrument.hpp`, add:

```cpp
struct FixingIdentity_ {
    String_ indexName_;
    int fixingHour_ = -1;
    int fixingMinute_ = -1;
};

struct FxResetConvention_ {
    int fixingLag_ = -1;
    Holidays_ fixingHolidays_;
    BizDayConvention_ fixingConvention_ = BizDayConvention_("Preceding");
    int fixingHour_ = -1;
    int fixingMinute_ = -1;
};

struct CrossCurrencySwapConfig_ {
    CurrencyPair_ pair_;
    double domesticNotional_ = 100.0;
    double foreignNotional_ = 100.0;
    CrossCurrencyConvention_ convention_;
    XccyNotionalMode_ notionalMode_ = XccyNotionalMode_::Value_::FIXED;
    FxResetConvention_ fxReset_;
    FixingIdentity_ domesticRateFixing_;
    FixingIdentity_ foreignRateFixing_;
};
```

Delete `resettableNotional_` and `markToMarketNotional_` from `CrossCurrencyConvention_`. Add constructor validation: positive finite notionals, distinct pair currencies, valid maturity, and explicit reset lag/time for non-`FIXED` modes.

- [ ] **Step 4: Regenerate and check generated sources**

Run: `cmake --build build --target dal_generate -j 4`

Run: `cmake --build build --target dal_check_generated -j 4`

Expected: both commands succeed and core plus Excel generated enum files are present.

- [ ] **Step 5: Run the protocol tests**

Run: `bin/dal_cpp_tests --gtest_filter=CurrencyDataTest.*:XccyMarketTest.TestResetConfigRequiresExplicitFixingIdentity`

Expected: all selected tests pass.

- [ ] **Step 6: Commit the protocol unit**

```bash
git add dal-cpp/dal/curve/xccynotionalmode.hpp dal-cpp/dal/protocol/rateconvention.hpp dal-cpp/dal/curve/xccyinstrument.hpp dal-cpp/dal/auto dal-excel/auto dal-cpp/tests/currency/test_currencydata.cpp dal-cpp/tests/curve/test_xccymarket.cpp
git commit -m "feat(xccy): define notional modes and reset config"
```

---

### Task 2: Add Immutable Market Fixing Snapshots

**Files:**
- Create: `dal-cpp/dal/indice/fixingsnapshot.hpp`
- Create: `dal-cpp/dal/indice/fixingsnapshot.cpp`
- Modify: `dal-cpp/dal/indice/index/fx.hpp`
- Modify: `dal-cpp/dal/indice/index/fx.cpp`
- Create: `dal-cpp/tests/indice/test_fixingsnapshot.cpp`

**Interfaces:**
- Consumes: `CurrencyPair_`, `DateTime_`, and the existing `Global::Fixings_::History` store.
- Produces:

```cpp
struct FixingRequest_ { String_ indexName_; DateTime_ fixingTime_; };

class MarketFixingSnapshot_ {
public:
    using history_t = std::map<DateTime_, double>;
    using values_t = std::map<String_, history_t>;
    explicit MarketFixingSnapshot_(const values_t& values = values_t());
    [[nodiscard]] optional<double> Find(const String_& indexName, const DateTime_& fixingTime) const;
    [[nodiscard]] double Require(const String_& indexName, const DateTime_& fixingTime, const String_& context) const;
};

Handle_<MarketFixingSnapshot_> SnapshotGlobalFixings(const Vector_<FixingRequest_>& requests);
String_ FxIndexName(const CurrencyPair_& pair);
String_ ReverseFxIndexName(const CurrencyPair_& pair);
```

- [ ] **Step 1: Write failing snapshot tests**

Cover direct lookup, reverse FX reciprocal, consistent/inconsistent two-way quotes, invalid numbers, missing required values, and global-store immutability:

```cpp
TEST(FixingSnapshotTest, TestSnapshotDoesNotObserveLaterGlobalMutation) {
    const DateTime_ fixing(Date_(2024, 1, 2), 11, 0);
    XGLOBAL::StoreFixings("FX[EUR/USD]", FixHistory_({{fixing, 1.10}}), false);
    const auto snapshot = SnapshotGlobalFixings({{"FX[EUR/USD]", fixing}});
    XGLOBAL::StoreFixings("FX[EUR/USD]", FixHistory_({{fixing, 1.20}}), false);
    ASSERT_NEAR(snapshot->Require("FX[EUR/USD]", fixing, "test"), 1.10, 1.0e-12);
}
```

- [ ] **Step 2: Run the test and verify failure**

Run: `cmake --build build --target dal_cpp_tests -j 4`

Expected: compilation fails because `fixingsnapshot.hpp` and its types do not exist.

- [ ] **Step 3: Implement normalized immutable storage**

The constructor copies input, checks every value with `std::isfinite(value) && value > 0.0`, and checks direct/reverse FX pairs with:

```cpp
REQUIRE(std::fabs(direct * reverse - 1.0) <= 1.0e-10,
        "Inconsistent direct/reverse FX fixings for " + directName + " at " + DateTime::ToString(fixingTime));
```

`SnapshotGlobalFixings` deduplicates requests, reads the global histories once, and stores only requested timestamps. `Require` names the index, timestamp, and supplied context in its exception.

- [ ] **Step 4: Expose canonical FX names**

Move the current private naming formula into free functions used by both `Index::Fx_` and the snapshot resolver:

```cpp
String_ FxIndexName(const Ccy_& domestic, const Ccy_& foreign) {
    return String_("FX[") + foreign.String() + "/" + domestic.String() + "]";
}
```

- [ ] **Step 5: Run fixing tests**

Run: `bin/dal_cpp_tests --gtest_filter=FixingSnapshotTest.*:IndexFxTest.*`

Expected: all selected tests pass.

- [ ] **Step 6: Commit the snapshot unit**

```bash
git add dal-cpp/dal/indice/fixingsnapshot.hpp dal-cpp/dal/indice/fixingsnapshot.cpp dal-cpp/dal/indice/index/fx.hpp dal-cpp/dal/indice/index/fx.cpp dal-cpp/tests/indice/test_fixingsnapshot.cpp
git commit -m "feat(fixings): add immutable market snapshots"
```

---

### Task 3: Precompute XCCY Cashflow and Reset Plans

**Files:**
- Create: `dal-cpp/dal/curve/xccypricing.hpp`
- Create: `dal-cpp/dal/curve/xccypricing.cpp`
- Modify: `dal-cpp/dal/curve/xccyinstrument.hpp`
- Modify: `dal-cpp/dal/curve/xccyinstrument.cpp`
- Create: `dal-cpp/tests/curve/test_xccypricing.cpp`

**Interfaces:**
- Consumes: `CrossCurrencySwapConfig_`, `BuildLegPeriods`, and `SchedulePeriod_`.
- Produces:

```cpp
struct XccyCouponPeriod_ {
    SchedulePeriod_ schedule_;
    AccrualPeriod_ accrual_;
    String_ rateIndexName_;
    DateTime_ rateFixingTime_;
};

struct XccyResetEvent_ {
    Date_ effectiveDate_;
    DateTime_ fxFixingTime_;
    int domesticPeriodIndex_ = 0;
};

struct XccyCashflowPlan_ {
    Vector_<XccyCouponPeriod_> domesticPeriods_;
    Vector_<XccyCouponPeriod_> foreignPeriods_;
    Vector_<XccyResetEvent_> resets_;
    CrossCurrencySwapConfig_ config_;
    Date_ start_;
    Date_ maturity_;
};

XccyCashflowPlan_ BuildXccyCashflowPlan(const Date_& start, const Date_& maturity, const CrossCurrencySwapConfig_& config);
Vector_<FixingRequest_> RequiredHistoricalFixings(const XccyCashflowPlan_& plan, const DateTime_& valuationTime);
```

- [ ] **Step 1: Write failing plan tests**

Add exact assertions for quarterly periods, a short stub, fixing lag/holiday adjustment, first-period no-reset behavior, second-period reset, and MTM event dates:

```cpp
TEST(XccyPricingTest, TestQuarterlyMtmPlanResetsFromSecondPeriod) {
    const auto plan = BuildXccyCashflowPlan(Date_(2024, 1, 4), Date_(2025, 1, 4), MakeQuarterlyMtmConfig());
    ASSERT_EQ(plan.domesticPeriods_.size(), 4U);
    ASSERT_EQ(plan.resets_.size(), 3U);
    ASSERT_EQ(plan.resets_[0].effectiveDate_, plan.domesticPeriods_[1].schedule_.accrualStart_);
    ASSERT_EQ(plan.resets_[0].domesticPeriodIndex_, 1);
}
```

- [ ] **Step 2: Run and confirm failure**

Run: `cmake --build build --target dal_cpp_tests -j 4`

Expected: compilation fails because the plan API is missing.

- [ ] **Step 3: Implement plan construction only**

Build both coupon schedules with `BuildLegPeriods`, combine each schedule fixing date with the configured hour/minute, and generate reset events only for domestic indices `1..N-1`. Do not access curves or fixings in this function.

For `FIXED`, return an empty reset vector. For non-fixed modes, validate each reset effective date equals the target domestic accrual start and that reset events are strictly increasing.

- [ ] **Step 4: Implement fixing-request enumeration**

Filter coupons with `paymentDate >= valuationTime.Date()`. Add rate requests only when `rateFixingTime_ < valuationTime`; add FX requests only when `fxFixingTime_ < valuationTime`. Sort by index then time and deduplicate.

- [ ] **Step 5: Run plan tests**

Run: `bin/dal_cpp_tests --gtest_filter=XccyPricingTest.Test*Plan*:XccyPricingTest.TestRequiredHistoricalFixings*`

Expected: all selected tests pass.

- [ ] **Step 6: Commit the planning unit**

```bash
git add dal-cpp/dal/curve/xccypricing.hpp dal-cpp/dal/curve/xccypricing.cpp dal-cpp/dal/curve/xccyinstrument.hpp dal-cpp/dal/curve/xccyinstrument.cpp dal-cpp/tests/curve/test_xccypricing.cpp
git commit -m "feat(xccy): precompute reset and cashflow plans"
```

---

### Task 4: Implement the Typed Pricing Kernel and In-Progress Valuation

**Files:**
- Modify: `dal-cpp/dal/curve/xccypricing.hpp`
- Modify: `dal-cpp/dal/curve/xccypricing.cpp`
- Modify: `dal-cpp/dal/curve/xccyinstrument.cpp`
- Modify: `dal-cpp/tests/curve/test_xccypricing.cpp`
- Modify: `dal-cpp/tests/curve/test_xccymarket.cpp`

**Interfaces:**
- Consumes: `XccyCashflowPlan_`, `MarketFixingSnapshot_`, and domestic/foreign/basis typed curves.
- Produces:

```cpp
template <class T_> struct XccyMarketView_ {
    DateTime_ valuationTime_;
    CurrencyPair_ pair_;
    Ccy_ collateralCurrency_;
    T_ fxSpot_;
    const Tape::JointCurveBlock_<T_>* domestic_ = nullptr;
    const Tape::JointCurveBlock_<T_>* foreign_ = nullptr;
    const Tape::DiscountCurve_<T_>* basis_ = nullptr;
};

template <class T_>
T_ PriceXccyParSpread(const XccyCashflowPlan_& plan,
                      const XccyMarketView_<T_>& market,
                      const MarketFixingSnapshot_& fixings);
```

- [ ] **Step 1: Write failing fixing-source tests**

Cover past/equal/future rate and FX timestamps. Assert missing historical values throw and future values respond to changed forecast/basis curves.

- [ ] **Step 2: Write the failing hand-calculated MTM test**

Use two domestic periods, fixed foreign notional `100`, initial domestic notional `110`, second-period FX fixing `1.20`, unit discount factors, zero floating coupons, and enabled initial/final exchanges. Assert the domestic MTM delta is `+10`, the final domestic exchange is `120`, and the kernel par-spread numerator matches the explicit signed cashflow sum.

```cpp
ASSERT_NEAR(resolved.domesticNotionals_[0], 110.0, 1.0e-12);
ASSERT_NEAR(resolved.domesticNotionals_[1], 120.0, 1.0e-12);
ASSERT_NEAR(resolved.mtmDeltas_[0], 10.0, 1.0e-12);
ASSERT_NEAR(actualParSpread, explicitNumerator / explicitForeignAnnuity, 1.0e-12);
```

- [ ] **Step 3: Write failing in-progress tests**

Construct a trade with one paid coupon, one past-fixed/future-paid coupon, one valuation-date payment, and one future coupon. Assert the first is absent, the second uses the snapshot, the valuation-date discount factor is one, and the future coupon changes with its forecast curve.

- [ ] **Step 4: Implement scalar-templated resolution**

Use this branch rule for rate and FX values:

```cpp
if (fixingTime < market.valuationTime_)
    return T_(fixings.Require(indexName, fixingTime, context));
if (fixingTime == market.valuationTime_) {
    const optional<double> supplied = fixings.Find(indexName, fixingTime);
    if (supplied)
        return T_(*supplied);
}
return activeForward();
```

Resolve domestic notionals first, then coupons and notional events. For `RESETTABLE`, omit intermediate deltas. For `MARK_TO_MARKET`, discount each delta on its effective date. Filter historical payment events before fixing lookup.

- [ ] **Step 5: Replace the fixed-only `CrossCurrencySwapRate_` body**

Keep `CrossCurrencySwap_::Rate_::operator()(const CrossCurrencyMarket_&)` for compatibility, but make it adapt the passive market to `XccyMarketView_<double>` and call `PriceXccyParSpread<double>`. Remove the two “not implemented” checks and the “in-progress swaps not supported” check.

- [ ] **Step 6: Run pricing tests**

Run: `bin/dal_cpp_tests --gtest_filter=XccyPricingTest.*:XccyMarketTest.TestCrossCurrencySwap*`

Expected: fixing, manual MTM, in-progress, fixed regression, missing-fixing, and matured-trade tests pass.

- [ ] **Step 7: Commit the pricing unit**

```bash
git add dal-cpp/dal/curve/xccypricing.hpp dal-cpp/dal/curve/xccypricing.cpp dal-cpp/dal/curve/xccyinstrument.cpp dal-cpp/tests/curve/test_xccypricing.cpp dal-cpp/tests/curve/test_xccymarket.cpp
git commit -m "feat(xccy): price reset and in-progress cashflows"
```

---

### Task 5: Integrate the Kernel into Basis-Only Calibration and Add Analytic Jacobians

**Files:**
- Modify: `dal-cpp/dal/curve/xccycalibration.hpp`
- Modify: `dal-cpp/dal/curve/xccycalibration.cpp`
- Modify: `dal-cpp/tests/curve/test_xccymarket.cpp`
- Create: `dal-cpp/tests/curve/test_xccybasisjacobian.cpp`

**Interfaces:**
- Adds to `CrossCurrencyCalibrationSpec_`: `DateTime_ valuationTime_`, `Ccy_ collateralCurrency_`, and `Handle_<MarketFixingSnapshot_> fixings_`.
- Produces:

```cpp
struct CrossCurrencyCalibrationOptions_ {
    CurveJacobianMode_ jacobianMode_ = CurveJacobianMode_::Value_::ANALYTIC;
    bool computeEffJacobianInverse_ = true;
    bool computeForwardJacobian_ = true;
};

CrossCurrencyCalibrationResult_ CalibrateCrossCurrencyMarket(
    const CrossCurrencyCalibrationSpec_& spec,
    const CrossCurrencyCalibrationOptions_& options);
```

- [ ] **Step 1: Write failing staged-calibration tests**

Add one future-start MTM instrument and one in-progress MTM instrument with a supplied snapshot. Generate quotes from a known flat basis, calibrate from zero, and assert both model rates reprice within `1.0e-8`.

- [ ] **Step 2: Write the failing basis Jacobian comparison**

At a fixed parameter vector, evaluate `XccyCalibrationFunc_::Gradient` in analytic mode and compare every element with central differences using `h = 1.0e-6` and tolerance `1.0e-7`. Assert columns associated only with historical FX resets do not acquire extra fixing sensitivity.

- [ ] **Step 3: Run and verify failure**

Run: `cmake --build build --target dal_cpp_tests -j 4`

Expected: compilation fails because the options overload and analytic XCCY residual path are missing.

- [ ] **Step 4: Make valuation context explicit**

Stop reading `Global::Dates_::EvaluationDate()` inside `CrossCurrencyMarket_`. Store the valuation date/time and collateral currency in the market/spec, validate `collateralCurrency_ == basisPair_.domestic_`, and keep the existing `today_` adapter for callers that do not set `valuationTime_`.

- [ ] **Step 5: Add the AAD gradient**

Build the basis curve with `BuildDiscountCurveT<AAD::Number_>`, build passive domestic/foreign typed views, call `PriceXccyParSpread<AAD::Number_>` for every residual, and return `XCurveJacobian_(HarvestCurveJacobian(...))`. If the requested basis parameterization or instrument plan is ineligible, throw an exception naming the reason.

- [ ] **Step 6: Return diagnostics according to options**

Pass `effJacobianInverse_` and forward Jacobian pointers to `Underdetermined::Find` only when requested. Keep them empty in approximate mode and state that behavior in result comments.

- [ ] **Step 7: Run staged and Jacobian tests**

Run: `bin/dal_cpp_tests --gtest_filter=XccyMarketTest.*:XccyBasisJacobianTest.*`

Expected: staged fixed/reset/MTM calibration, started-trade calibration, analytic/central-difference comparison, and bumped mode pass.

- [ ] **Step 8: Commit basis calibration integration**

```bash
git add dal-cpp/dal/curve/xccycalibration.hpp dal-cpp/dal/curve/xccycalibration.cpp dal-cpp/tests/curve/test_xccymarket.cpp dal-cpp/tests/curve/test_xccybasisjacobian.cpp
git commit -m "feat(xccy): calibrate basis with reset-aware AAD pricing"
```

---

### Task 6: Add the Three-Block Domestic/Foreign/Basis Joint Solver

**Files:**
- Create: `dal-cpp/dal/curve/jointcalibration_internal.hpp`
- Modify: `dal-cpp/dal/curve/jointcalibration.cpp`
- Create: `dal-cpp/dal/curve/xccyjointcalibration.hpp`
- Create: `dal-cpp/dal/curve/xccyjointcalibration.cpp`
- Create: `dal-cpp/tests/curve/test_xccyjointcalibration.cpp`
- Create: `dal-cpp/tests/curve/test_xccyjointjacobian.cpp`
- Modify: `dal-cpp/tests/curve/test_joint_calibration.cpp`
- Modify: `dal-cpp/tests/curve/test_joint_analytic_jacobian.cpp`

**Interfaces:**
- Consumes: `JointCurveDeclaration_`, the typed XCCY kernel, `Underdetermined`, and `MarketFixingSnapshot_`.
- Produces:

```cpp
struct CalibrationBlockRange_ {
    String_ name_;
    int offset_ = 0;
    int size_ = 0;
};

struct JointCurrencyCurveSpec_ {
    Ccy_ ccy_;
    DayBasis_ liborBasis_ = DayBasis_("ACT_365F");
    Vector_<JointCurveDeclaration_> curves_;
};

struct XccyBasisCurveDeclaration_ {
    String_ curveName_ = "xccy_basis";
    Vector_<Handle_<CrossCurrencySwap_>> instruments_;
    Vector_<Date_> knotDates_;
    CurveParameterization_ parameterization_ = CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD;
    double smoothingWeight_ = 1.0;
    Vector_<double> initialGuessPerNode_;
};

struct JointXccyCalibrationSpec_ {
    DateTime_ valuationTime_;
    CurrencyPair_ pair_;
    Ccy_ collateralCurrency_;
    double fxSpot_ = 0.0;
    JointCurrencyCurveSpec_ domestic_;
    JointCurrencyCurveSpec_ foreign_;
    XccyBasisCurveDeclaration_ basis_;
    Handle_<MarketFixingSnapshot_> fixings_;
    CurveSolverOptions_ solver_;
};
```

`JointXccyCalibrationResult_` contains domestic/foreign curve-block handles, basis curve, FX forwards, grouped diagnostics, `jacobianAtSolution_`, `effJacobianInverse_`, and vectors of parameter/residual `CalibrationBlockRange_`.

- [ ] **Step 1: Write a failing synthetic recovery test**

Build known domestic and foreign OIS/3M curves plus a known basis curve. Quote domestic instruments, foreign instruments, and 1Y–5Y resettable/MTM XCCY swaps from that market. Start all curve parameters away from the truth, solve once, and assert every instrument group reprices within `1.0e-8`.

- [ ] **Step 2: Write failing validation tests**

Cover mismatched currencies, third-currency collateral, unordered knots, missing curve slots, no remaining XCCY annuity, invalid spot, duplicate declarations, and empty instrument groups. Error assertions must match the offending pair or slot name.

- [ ] **Step 3: Write the failing stacked Jacobian test**

Compare analytic and central-difference matrices for every domestic, foreign, and basis column. Assert the range metadata exactly partitions all columns and rows without gaps or overlap.

- [ ] **Step 4: Extract reusable joint-curve helpers without changing existing behavior**

Move slot definitions, validation, parameter slicing, typed curve construction, smoothing assembly, and diagnostics from the anonymous namespace in `jointcalibration.cpp` into `jointcalibration_internal.hpp`. Keep the original `CalibrateJointMultiCurve` public types and output unchanged.

Run: `bin/dal_cpp_tests --gtest_filter=JointCalibrationTest.*:JointAnalyticJacobianTest.*`

Expected: all existing joint tests pass before adding XCCY solve logic.

- [ ] **Step 5: Implement one residual function**

Build the unknown vector in this exact order:

```text
domestic declaration 0..N | foreign declaration 0..M | basis nodes
```

Build one active domestic block, one active foreign block, and one active basis curve per evaluation. Evaluate domestic and foreign `YCInstrument_` residuals through their existing typed rates, then XCCY residuals through `PriceXccyParSpread<T_>`. Do not call `CalibrateJointMultiCurve` or `CalibrateCrossCurrencyMarket` from the residual function.

- [ ] **Step 6: Add exact, approximate, analytic, and bumped paths**

Use block-diagonal smoothing over all declarations. Exact mode returns both effective inverse and optional forward Jacobian; approximate mode returns neither inverse and marks diagnostics accordingly. Analytic eligibility must include every domestic/foreign instrument plus every XCCY plan.

- [ ] **Step 7: Run joint XCCY tests**

Run: `bin/dal_cpp_tests --gtest_filter=XccyJointCalibrationTest.*:XccyJointJacobianTest.*:JointCalibrationTest.*:JointAnalyticJacobianTest.*`

Expected: synthetic recovery, validation, exact/approximate, analytic/bumped, layout, and all pre-existing joint tests pass.

- [ ] **Step 8: Commit the joint solver unit**

```bash
git add dal-cpp/dal/curve/jointcalibration_internal.hpp dal-cpp/dal/curve/jointcalibration.cpp dal-cpp/dal/curve/xccyjointcalibration.hpp dal-cpp/dal/curve/xccyjointcalibration.cpp dal-cpp/tests/curve/test_xccyjointcalibration.cpp dal-cpp/tests/curve/test_xccyjointjacobian.cpp dal-cpp/tests/curve/test_joint_calibration.cpp dal-cpp/tests/curve/test_joint_analytic_jacobian.cpp
git commit -m "feat(xccy): jointly calibrate domestic foreign and basis curves"
```

---

### Task 7: Add Public C++ Builders and Compatibility Tests

**Files:**
- Modify: `dal-public/src/curveprotocol.hpp`
- Modify: `dal-public/src/curveinstrument.hpp`
- Modify: `dal-public/src/xccycalibration.hpp`
- Modify: `dal-public/src/xccycalibration.cpp`
- Modify: `dal-public/tests/test_curve_protocol.cpp`
- Modify: `dal-public/tests/test_curve_instrument.cpp`
- Modify: `dal-public/tests/test_xccy_calibration.cpp`

**Interfaces:**
- Produces: `FxResetConventionNew`, `MarketFixingSnapshotNew`, `CrossCurrencySwapConfigBuilder_`, the config overload of `CrossCurrencySwapNew`, `JointXccyCalibrationSpecBuilder_`, `CalibrateJointXccyMarket`, and result accessors.
- Preserves: the current long `CrossCurrencySwapNew` overload as `FIXED`.

- [ ] **Step 1: Write failing builder round-trip tests**

Set every reset, fixing identity, collateral, snapshot, curve declaration, solver, and instrument field; call `Build()`; assert each core spec field is preserved. Add a compile-and-run test for the legacy long overload.

- [ ] **Step 2: Run and verify failure**

Run: `cmake --build build --target dal_public_tests -j 4`

Expected: compilation fails because the new public builders are undefined.

- [ ] **Step 3: Implement config-focused public factories**

Keep required dates/rate as function arguments and move optional behavior into config:

```cpp
Handle_<CrossCurrencySwap_> CrossCurrencySwapNew(const Date_& tradeDate,
                                                  const Date_& start,
                                                  const Date_& maturity,
                                                  double marketRate,
                                                  const CrossCurrencySwapConfig_& config);
```

`MarketFixingSnapshotNew` accepts `std::map<String_, std::map<DateTime_, double>>`. The joint builder owns flat solver fields only if Python member-pointer compatibility requires it; `Build()` converts them into one `CurveSolverOptions_`.

- [ ] **Step 4: Run public tests**

Run: `bin/dal_public_tests --gtest_filter=CurveProtocolTest.*:CurveInstrumentTest.TestCrossCurrency*:XccyCalibrationTest.*`

Expected: old and new instrument builders, snapshot, staged builder, joint builder, and result layout pass.

- [ ] **Step 5: Commit the public API unit**

```bash
git add dal-public/src/curveprotocol.hpp dal-public/src/curveinstrument.hpp dal-public/src/xccycalibration.hpp dal-public/src/xccycalibration.cpp dal-public/tests/test_curve_protocol.cpp dal-public/tests/test_curve_instrument.cpp dal-public/tests/test_xccy_calibration.cpp
git commit -m "feat(public): expose resettable and joint XCCY builders"
```

---

### Task 8: Add Python Bindings and Tests

**Files:**
- Modify: `dal-python/src/bindings/curve.cpp`
- Create: `dal-python/tests/test_xccy_resettable.py`
- Create: `dal-python/tests/test_xccy_joint.py`
- Modify: `dal-python/tests/test_xccy_calibration.py`

**Interfaces:**
- Consumes: all public builders and result types from Task 7.
- Produces: Python enum/config/snapshot/joint builder classes and keyword-based functions.

- [ ] **Step 1: Write failing Python tests**

Create a nested fixing mapping, build an in-progress MTM swap, assert missing historical values raise, run a small joint calibration, and read parameter/residual ranges plus the Jacobian dimensions:

```python
snapshot = dal.MarketFixingSnapshot_New({
    "USD-SOFR-3M": {rate_fixing_time: 0.0525},
    "EUR-EURIBOR-3M": {foreign_fixing_time: 0.0310},
    "FX[EUR/USD]": {fx_fixing_time: 1.10},
})
assert snapshot is not None
assert result.jacobianAtSolution_.rows() == sum(r.size_ for r in result.residualRanges_)
assert result.jacobianAtSolution_.cols() == sum(r.size_ for r in result.parameterRanges_)
```

- [ ] **Step 2: Run and verify failure**

Run: `python -m pytest dal-python/tests/test_xccy_resettable.py dal-python/tests/test_xccy_joint.py -q`

Expected: import or attribute failures for the new bindings.

- [ ] **Step 3: Bind enum, configs, snapshots, builders, and results**

Expose new fields with snake_case aliases while retaining existing underscore field names used by current tests. Convert nested Python dictionaries to the C++ snapshot map and convert instrument lists to `Handle_` collections with the existing const-pointer ownership pattern.

- [ ] **Step 4: Keep the old Python call unchanged**

Retain the current `CrossCurrencySwap_New(trade_date, start, maturity, market_rate, currencies, ...)` registration. Add a config overload distinguished by its final argument type; do not reorder the old arguments.

- [ ] **Step 5: Run Python XCCY tests**

Run: `python -m pytest dal-python/tests/test_xccy_calibration.py dal-python/tests/test_xccy_resettable.py dal-python/tests/test_xccy_joint.py -q`

Expected: all selected tests pass.

- [ ] **Step 6: Commit the Python unit**

```bash
git add dal-python/src/bindings/curve.cpp dal-python/tests/test_xccy_calibration.py dal-python/tests/test_xccy_resettable.py dal-python/tests/test_xccy_joint.py
git commit -m "feat(python): bind resettable and joint XCCY APIs"
```

---

### Task 9: Add Excel Handles, Functions, and Generated Sources

**Files:**
- Modify: `dal-excel/src/__curve_storable.hpp`
- Modify: `dal-excel/src/__curveprotocol.cpp`
- Modify: `dal-excel/src/__curveinstrument.cpp`
- Modify: `dal-excel/src/__xccycalibration.cpp`
- Generate: `dal-excel/auto/MG_XccyResetConvention_New_*`
- Generate: `dal-excel/auto/MG_MarketFixingSnapshot_New_*`
- Generate: `dal-excel/auto/MG_CrossCurrencySwap_Config_New_*`
- Generate: `dal-excel/auto/MG_Calibrate_JointXccy_*`
- Generate: joint result accessor sources and HTML help

**Interfaces:**
- Produces worksheet functions `XCCYRESETCONVENTION.NEW`, `MARKETFIXINGSNAPSHOT.NEW`, `CROSSCURRENCYSWAP.CONFIG.NEW`, `CALIBRATE.JOINTXCCY`, and joint result accessors.
- Preserves `CROSSCURRENCYSWAP.NEW` and `CALIBRATE.XCCYMARKET` registrations.

- [ ] **Step 1: Add storable wrappers**

Add `StorableCrossCurrencySwapConfig_`, `StorableMarketFixingSnapshot_`, and `StorableJointXccyCalibrationResult_`. The result wrapper retains handles to both solved curve blocks and the basis curve so Excel accessors cannot outlive them.

- [ ] **Step 2: Add Machinist public declarations**

Use handles for nested configs and results. `MARKETFIXINGSNAPSHOT.NEW` accepts parallel index-name, fixing-datetime, and value arrays and validates equal length before building the normalized map. `CALIBRATE.JOINTXCCY` keeps solver values in the existing two-column settings dictionary.

- [ ] **Step 3: Implement result accessors**

Expose `domesticBlock`, `foreignBlock`, `basisCurve`, `fxForwards`, `marketRates`, `modelRates`, `residuals`, `jacobian`, `parameterRanges`, and `residualRanges`. Unknown attributes list all accepted names in the error.

- [ ] **Step 4: Regenerate and verify generated drift**

Run: `cmake --build build --target dal_generate -j 4`

Run: `cmake --build build --target dal_check_generated -j 4`

Expected: generation and drift check succeed.

- [ ] **Step 5: Build Excel on Windows**

Run: `cmake --build build --config Release --target dal_excel`

Expected: the add-in and all generated public functions compile successfully.

- [ ] **Step 6: Commit the Excel unit**

```bash
git add dal-excel/src/__curve_storable.hpp dal-excel/src/__curveprotocol.cpp dal-excel/src/__curveinstrument.cpp dal-excel/src/__xccycalibration.cpp dal-excel/auto dal-cpp/dal/auto
git commit -m "feat(excel): expose resettable and joint XCCY functions"
```

---

### Task 10: Add `xccy_perf` and Run Regression Benchmarks

**Files:**
- Create: `dal-cpp/benchmarks/xccy_perf/CMakeLists.txt`
- Create: `dal-cpp/benchmarks/xccy_perf/xccy_perf.cpp`
- Modify: `dal-cpp/benchmarks/CMakeLists.txt`

**Interfaces:**
- Consumes: passive pricing, basis-only calibration, joint calibration, analytic/bumped options, and immutable snapshots.
- Produces: normalized precompute/price/basket rows and absolute calibration rows through `Bench::Print`.

- [ ] **Step 1: Register an initially failing benchmark target**

Add `xccy_perf` to `DAL_BENCHMARK_TARGETS` and create its `CMakeLists.txt` using the same DAL/AAD/thread link pattern as `curve_calibration_perf`.

Run: `cmake --build build --target xccy_perf -j 4`

Expected: build fails until `xccy_perf.cpp` is added.

- [ ] **Step 2: Implement validated pricing workloads**

Use 3 warmups and 10 repeats. Build 10Y quarterly fixed/resettable/MTM instruments, an in-progress MTM instrument with historical domestic-rate, foreign-rate, and FX fixings, and ten-instrument baskets. Normalize batched results to per-operation or per-instrument nanoseconds. Require finite, nonzero checksums before printing.

- [ ] **Step 3: Implement validated calibration workloads**

Use 15 XCCY instruments. Add basis-only exact analytic solve-only, analytic plus diagnostics, and bumped plus diagnostics cases. Add the same three cases for full three-block exact calibration plus one approximate analytic case. Before timing, run once and require every reported residual norm is within the configured tolerance.

- [ ] **Step 4: Build and smoke-run the new benchmark**

Run: `cmake --build build --config Release --target xccy_perf -j 4`

Run: `build/dal-cpp/benchmarks/xccy_perf/xccy_perf`

Expected: every required named row prints min/median/max/repeat columns and the process exits zero.

- [ ] **Step 5: Establish the branch/baseline comparison**

At execution time, use `superpowers:using-git-worktrees` to create a clean merge-base benchmark worktree. Build both baseline and branch in Release. Run paired interleaved samples, baseline then branch then branch then baseline, until each binary has at least 10 samples. Use build-tree binaries only and reduce each row to the best observed minimum.

Report:

```text
Benchmark | Baseline min | Branch min | Delta | Verdict | Samples/environment
```

Flag only deltas greater than 4%. For reset/MTM and three-block rows that do not exist at merge base, report absolute min/median/max and mark them “new baseline”.

- [ ] **Step 6: Run the standard DAL regression set**

Run the same paired protocol for `tape_perf`, `jacobian_perf`, `pde_perf`, `rng_perf`, `interp_perf`, `krylov_perf`, `banded_perf`, and `cholesky_perf`.

Expected: no repeatable branch regression greater than 4%; otherwise stop packaging and investigate the responsible hot path.

- [ ] **Step 7: Commit benchmark coverage**

```bash
git add dal-cpp/benchmarks/CMakeLists.txt dal-cpp/benchmarks/xccy_perf/CMakeLists.txt dal-cpp/benchmarks/xccy_perf/xccy_perf.cpp
git commit -m "perf(xccy): benchmark reset pricing and joint calibration"
```

---

### Task 11: Add Example, Documentation, Changelog, and Final Verification

**Files:**
- Create: `dal-cpp/examples/xccy_mtm_calibration/CMakeLists.txt`
- Create: `dal-cpp/examples/xccy_mtm_calibration/xccy_mtm_calibration.cpp`
- Modify: `dal-cpp/examples/CMakeLists.txt`
- Modify: `docs/methodology/xccy_calibration.md`
- Modify: `docs/methodology/yield_curve_jacobian.md`
- Modify: `docs/public-api.md`
- Modify: `docs/README.md`
- Modify: `dal-python/README.md`
- Modify: `dal-excel/README.md`
- Modify: `CHANGELOG.md`

**Interfaces:**
- Consumes: the final public C++, Python, and Excel surfaces.
- Produces: one compilable current-state example and reconciled documentation.

- [ ] **Step 1: Add a compiling end-to-end example**

The example must be 20–50 lines in its main call sequence and show: explicit fixing identities, one snapshot containing rate and FX values, an in-progress MTM swap, domestic/foreign/basis declarations, `CalibrateJointXccyMarket`, and Jacobian range output. Generate synthetic quotes from a known market so it runs without external data.

- [ ] **Step 2: Build and run the example**

Run: `cmake --build build --target xccy_mtm_calibration -j 4`

Run: `build/dal-cpp/examples/xccy_mtm_calibration/xccy_mtm_calibration`

Expected: the program reports converged domestic, foreign, and basis blocks; maximum residual below `1.0e-8`; and nonempty Jacobian layout.

- [ ] **Step 3: Reconcile methodology and API docs**

Document the exact notional formulas, reset/fixing timing, payment-date rule, generalized fixing snapshot, domestic-collateral restriction, staged versus joint calibration, parameter/residual layout, and analytic/bumped behavior. Keep historical migration detail out of methodology docs.

- [ ] **Step 4: Add changelog entry**

Record the new XCCY capability and the breaking replacement of the two public convention booleans by `XccyNotionalMode_`. Include compatibility of the old convenience builder and new Python/Excel functions.

- [ ] **Step 5: Run targeted functional suites**

Run: `bin/dal_cpp_tests --gtest_filter=FixingSnapshotTest.*:XccyPricingTest.*:XccyMarketTest.*:XccyBasisJacobianTest.*:XccyJointCalibrationTest.*:XccyJointJacobianTest.*:JointCalibrationTest.*:JointAnalyticJacobianTest.*`

Run: `bin/dal_public_tests --gtest_filter=CurveProtocolTest.*:CurveInstrumentTest.*:XccyCalibrationTest.*`

Run: `python -m pytest dal-python/tests/test_xccy_calibration.py dal-python/tests/test_xccy_resettable.py dal-python/tests/test_xccy_joint.py -q`

Expected: every selected suite passes.

- [ ] **Step 6: Run full build, tests, generation check, and benchmark smoke**

Run: `bash ./build_linux.sh`

Run: `cmake --build build --target dal_check_generated -j 4`

Run: `(cd build && ctest --output-on-failure)`

Run: `build/dal-cpp/benchmarks/xccy_perf/xccy_perf`

Expected: full build and CTest pass, no generated drift exists, and the benchmark exits zero.

- [ ] **Step 7: Run formatting and diff checks**

Format only changed C++ files with the repository `.clang-format`, then run:

Run: `git diff --check`

Expected: no whitespace errors and no unrelated user changes in the diff.

- [ ] **Step 8: Request DAL review and performance gate**

Use `dal-reviewer` for correctness, style, bindings, generated files, docs, and merge readiness. Use `dal-performancer` to publish the paired regression table with sample counts and environmental notes. Resolve every blocking or significant finding and rerun the affected verification command.

- [ ] **Step 9: Commit documentation and example**

```bash
git add dal-cpp/examples/xccy_mtm_calibration dal-cpp/examples/CMakeLists.txt docs/methodology/xccy_calibration.md docs/methodology/yield_curve_jacobian.md docs/public-api.md docs/README.md dal-python/README.md dal-excel/README.md CHANGELOG.md
git commit -m "docs(xccy): document MTM valuation and joint calibration"
```

---

## Completion Evidence

Before declaring the feature complete, retain these artifacts in the implementation handoff:

- targeted and full test command outputs;
- generated-source drift check output;
- analytic-versus-central-difference Jacobian maximum absolute difference;
- joint synthetic recovery residuals and parameter block layout;
- the `xccy_perf` absolute result table;
- paired branch/baseline tables for existing fixed XCCY cases and the eight standard DAL benchmark gates;
- reviewer verdict and any remaining nonblocking caveats.

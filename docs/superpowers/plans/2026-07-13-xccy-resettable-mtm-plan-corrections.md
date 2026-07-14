# XCCY Resettable/MTM Implementation Plan — Normative Corrections

This file is part of
`docs/superpowers/plans/2026-07-13-xccy-resettable-mtm.md`. Apply these corrections
when executing the plan. They resolve issues found by the writing-plans type and benchmark
self-review; the Windows sandbox prevented in-place updates to the newly created plan.

## 1. Core Joint Spec Uses Core Fields

Replace the `JointXccyCalibrationSpec_` definition in Task 6 with:

```cpp
struct JointXccyCalibrationSpec_ {
    DateTime_ valuationTime_;
    CurrencyPair_ pair_;
    Ccy_ collateralCurrency_;
    double fxSpot_ = 0.0;
    JointCurrencyCurveSpec_ domestic_;
    JointCurrencyCurveSpec_ foreign_;
    XccyBasisCurveDeclaration_ basis_;
    Handle_<MarketFixingSnapshot_> fixings_;
    double tolerance_ = 1.0e-8;
    double fitTolerance_ = 1.0e-6;
    double initialGuess_ = 0.0;
    int maxEvaluations_ = 200;
    int maxRestarts_ = 20;
    CurveSolveMode_ solveMode_ = CurveSolveMode_::Value_::EXACT;
};
```

Core `dal-cpp` must not depend on `CurveSolverOptions_`, which is declared in
`dal-public/src/curvespec.hpp`. In Task 7, `JointXccyCalibrationSpecBuilder_` may own one
public `CurveSolverOptions_`; its `Build()` method copies those values into the flat core
fields above.

## 2. Use `std::optional`

The `MarketFixingSnapshot_::Find` signature and every local fixing lookup in Tasks 2 and
4 use `std::optional<double>`, not an unqualified `optional<double>`:

```cpp
[[nodiscard]] std::optional<double> Find(const String_& indexName,
                                         const DateTime_& fixingTime) const;
```

Include `<optional>` in `fixingsnapshot.hpp` and `xccypricing.hpp` where required.

## 3. Construct Global Fixing Test Data Explicitly

Replace the `FixHistory_({{...}})` expressions in Task 2 with C++17-compatible aggregate
setup:

```cpp
FixHistory_ first;
first.vals_ = {{fixing, 1.10}};
XGLOBAL::StoreFixings("FX[EUR/USD]", first, false);

const auto snapshot = SnapshotGlobalFixings({{"FX[EUR/USD]", fixing}});

FixHistory_ replacement;
replacement.vals_ = {{fixing, 1.20}};
XGLOBAL::StoreFixings("FX[EUR/USD]", replacement, false);
ASSERT_NEAR(snapshot->Require("FX[EUR/USD]", fixing, "test"), 1.10, 1.0e-12);
```

## 4. Capture a Legacy XCCY Performance Baseline Before Task 1

Execute the following benchmark phase before changing production code:

1. Create `dal-cpp/benchmarks/xccy_perf/` with only baseline-compatible workloads:
   fixed 10Y precompute, fixed 10Y passive pricing, fixed ten-instrument basket, and the
   existing basis-only calibration.
2. Register and build `xccy_perf` at the current feature-start commit.
3. Run at least 10 samples in Release and retain min/median/max plus workload sizes.
4. Commit the baseline-compatible benchmark separately with message
   `perf(xccy): establish fixed pricing baseline`.

Task 10 then extends that same target with resettable, MTM, in-progress, analytic/bumped,
and three-block workloads. Compare unchanged fixed rows against the feature-start commit,
not against a merge base that lacks the target. Continue to compare the eight standard
DAL benchmark gates against the merge base with `master` using the paired interleaved
protocol.

## 5. Benchmark Completion Evidence

The final performance report contains two baselines:

- `xccy_perf` fixed rows: feature-start benchmark commit versus final branch;
- the eight standard DAL gates: merge base with `master` versus final branch.

New reset/MTM and three-block rows publish absolute results and become the first future
baseline. A greater-than-4% delta is actionable only after paired, interleaved runs with at
least 10 samples per binary.

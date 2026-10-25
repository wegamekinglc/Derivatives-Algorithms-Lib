# Native Single-Curve Calibration Comparison

Date: 2026-10-26
Status: Approved direction; awaiting written-spec review

## Source And Problem

The user approved extending the existing native C++/.NET comparison from discount queries to curve calibration. The current worker and controller support only `discount_queries`. The new work must measure calibration itself rather than query an already calibrated curve.

## Goals

1. Compare DAL C++, QuantLib C++, and InFine C# on the existing `calibration_single_15` workload.
2. Include fresh instrument construction, initial curves, solving, all 30 discount-factor conversions, and native output array/vector creation in each timed invocation.
3. Keep raw quote generation, the independent oracle, Python, JSON, IPC, process startup, and result validation outside every timed interval.
4. Reuse the existing native worker, process rotation, evidence, provenance, and report infrastructure without changing the default `discount_queries` behavior.

## Non-Goals

- No multi-curve, joint, staged, XCCY, rateslib, GPU, or additional calibration case in this task.
- No claim that the three solvers perform identical iterations, use identical initial guesses, or have equal accuracy-cost tradeoffs.
- No production DAL algorithm changes, new public API, or competitor performance gate.
- No reuse of calibrated curves across measured invocations.

## Alternatives Considered

### Selected: extend the existing native comparison

Add an explicit case selection and a calibration fixture to the current C++ worker and controller. This preserves the proven native timing and evidence boundaries.

### Rejected: create a separate calibration-only stack

A second worker/controller would duplicate fixture validation, process rotation, provenance, and report checks without improving comparability.

### Rejected: compare only solved-curve queries

That would measure interpolation, not instrument construction and calibration, and would not answer the user's request.

## Functional Requirements

### R1: common workload and oracle

Use `calibration_single_15`: 15 annual fixed/float USD IRS starting on 2025-01-15 with maturities 1–15 years, ACT/365F, annual payments, unadjusted dates, no holidays, zero settlement/fixing/payment lags, and the same curve for discounting and forecasting.

Use the existing independently generated non-flat USD discount quotes. Calibrate log-linear discount factors with annual pillars and anchor DF=1. Return 15 solved annual node DFs followed by 15 mid-year DFs, for 30 values total.

The independent oracle is the existing scalar cashflow-derived curve. Validate every value on every warmup and measured call with `2e-8` absolute and `1e-10` relative tolerance using the existing `max(abs_tol, rel_tol * magnitude)` convention. Raw quotes and reference values are prepared outside timing.

### R2: backend methods and timing boundary

DAL constructs a fresh `CurveCalibrationSpec_` and 15 `Swap_` instruments per invocation, using `LOG_DISCOUNT`/`LOG_LINEAR`, `INPUT` knots, tolerance `1e-12`, fit tolerance `1e-10`, initial guess `0.025`, analytic Jacobian mode, and both optional Jacobian/inverse diagnostics disabled. Evaluate all 30 DFs through the solved curve.

QuantLib constructs a fresh annual `IborIndex`, 15 `SwapRateHelper` objects, and `PiecewiseYieldCurve<Discount, LogLinear>` per invocation, with bootstrap accuracy `1e-12`. Force the lazy bootstrap and evaluate all 30 DFs inside timing.

InFine constructs a fresh `PiecewiseYieldCurveBuilder` and 15 `VanillaSwap` instruments per invocation, using log-linear discount interpolation and native Brent tolerance `1e-12`. Evaluate all 30 DFs inside timing.

All three create the returned native array/vector inside timing. C++ and .NET remain different runtimes and the available solver algorithms differ; method labels must disclose the actual settings.

### R3: remove InFine JSON work from calibration timing

Before returning the calibration lambda, parse workload size/kind, quote arrays, and query dates. The timed lambda must receive ordinary managed values only. Preserve all existing supported calibration cases and oracles; this is a timing-boundary correction, not a numerical algorithm change.

### R4: worker and controller compatibility

Extend the C++ worker with optional `--case discount_queries|calibration_single_15`; omission retains `discount_queries`. Validate the fixture and emit the actual workload rather than hardcoding discount metadata.

Use `dal.native-calibration-fixture/1`, `dal.native-calibration-worker/1`, and `dal.native-calibration-comparison/1` for the new artifacts. Existing discount fixture/worker/report schemas remain unchanged.

Add `--case` to `run_native_queries.py` with the same choices and default. Generate the shared payload with `infine.fixtures(False, [args.case])`. Method labels, fixture tolerance, summary title, and boundary text must be case-specific. Include calibration scenario and relevant C# source files in source hashes.

Build calibration maturity dates with `calibration_scenarios.dates(15)`, not the separate 22-node discount-query rates payload. Use the shared InFine case's `usd_discount` quotes, queries, and reference verbatim.

### R5: evidence and acceptance

Keep three checked warmups and one native sample per fresh process. Acceptance uses two rounds of ten processes per backend, rotation and second-round reversal, one thread, and a new output directory.

Retain every worker's full result and timing in `results.json`, plus aggregate minima/median, ratios, binary/source hashes, CMake and compiler settings, QuantLib/Boost versions, InFine runtime identity, and clean Git provenance. Fail closed on incomplete samples, changed binaries/configuration/source, unsupported cases, solver failure, or incorrect values.

The acceptance output is `benchmark-results/native-cxx-calibration-comparison-final/`. Re-run the existing `discount_queries` acceptance after the shared changes to prove compatibility.

## Testing

1. Unit tests cover calibration fixture dates/quotes/reference/tolerance, case arguments and defaults, dynamic methods/summary, and unchanged discount behavior.
2. C++ integration invokes DAL and QuantLib on `calibration_single_15`, requiring 30 correct values and one positive native sample per backend.
3. Republish the pinned InFine worker after the C# adapter change so the manifest matches the new adapter. Existing InFine native integration tests then validate all supported cases after moving JSON work outside timing.
4. The real three-backend calibration acceptance completes 60 workers and 1,800 validated values (60 × 30), followed by independent report verification.
5. Existing comparison tests, affected C++ tests, native query acceptance, default configuration, and diff checks remain green.

## Documentation

Document the exact native build/run commands, case selection, workload, solver settings, and timing boundary. Link the native calibration and native query reports without comparing their ratios to the Python API reports.

## Open Questions

None. The accepted scope is the single `calibration_single_15` case across the three native backends.

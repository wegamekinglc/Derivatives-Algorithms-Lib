# Yield-Curve Instrument Pricing Performance Benchmark Design

## Source

- User request: add runtime benchmarks for every instrument priced by the yield-curve calibration framework.
- Approved scope: all seven concrete `YCInstrument_` types.
- Approved measurement split: precompute lifecycle and steady-state pricing.
- Approved structure: per-instrument microbenchmarks plus calibration-shaped pricing baskets.

## Problem Statement

DAL has end-to-end curve-calibration timing, but that timing combines curve construction,
solver iterations, Jacobians, diagnostics, and instrument pricing. It cannot attribute a
runtime change to one instrument or distinguish one-time `Precompute` work from the repeated
`Rate_::operator()` calls made by a calibration residual evaluation.

A dedicated benchmark must expose those costs without changing pricing behavior or turning
noisy wall-clock results into brittle test thresholds.

## Goals

- Benchmark every concrete built-in yield-curve instrument:
  `Deposit_`, `FRA_`, `Future_`, `STIR_`, `Swap_`, `OISSwap_`, and `BasisSwap_`.
- Measure `Precompute` lifecycle cost separately from steady-state pricing cost.
- Measure realistic discount, projection, and multi-tenor residual-style baskets.
- Use the same map-backed curve routing used by calibration.
- Produce stable, self-describing median/min/max rows on Linux and Windows CI.
- Prevent dead-code elimination and validate the untimed fixture before measuring it.

## Non-Goals

- No production pricing, calibration, curve, AAD, or public API changes.
- No end-to-end solve timing; `curve_calibration_perf` remains responsible for that surface.
- No curve-construction timing inside an instrument row.
- No absolute runtime assertions or new merge-blocking regression threshold.
- No claim that `BasisSwap_` supports the analytical-Jacobian visitor. Its rows are passive
  pricing coverage and correspond to bumped calibration behavior.
- No documentation or changelog update outside this design and implementation plan.

## Benchmark Target

Create one executable named `ycinstrument_perf`:

- `dal-cpp/benchmarks/ycinstrument_perf/ycinstrument_perf.cpp`
- `dal-cpp/benchmarks/ycinstrument_perf/CMakeLists.txt`

Register it in `dal-cpp/benchmarks/CMakeLists.txt` and in both CI benchmark-reporting arrays.
Use the existing `Dal::Bench` harness and the standard optional AAD-backend and platform link
rules used by other DAL benchmark targets. Do not add the target to the calibrated eight-target
regression gate.

## Deterministic Market Fixture

The benchmark owns one immutable pricing fixture:

- anchor date: `2024-01-15`;
- currency: `USD`;
- day basis: `ACT_365F`;
- holidays: `Holidays::None()`;
- business-day and payment conventions: `Unadjusted`;
- OIS discount curve: multi-knot piecewise-linear forward curve at 2.00%;
- 3M projection curve: the OIS curve as typed base plus a 0.50% forward spread;
- 6M projection curve: the OIS curve as typed base plus a 1.00% forward spread;
- curve knots: 3M, 6M, 1Y, 2Y, 5Y, 10Y, 20Y, and 40Y after the anchor;
- context: map-backed `CurveBlock_` containing OIS discounting and both 3M and 6M
  projection entries.

The fixture must be built before any timed region. The forward curves remain base-layered so
the benchmark includes the same discount-plus-spread evaluation used by multi-curve calibration.

## Individual Instrument Matrix

| Instrument | Representative contract | Pricing route | Analytical-Jacobian note |
|------------|-------------------------|---------------|--------------------------|
| `Deposit_` | 6M | OIS discount | Direct visitor family |
| `FRA_` | 3x6 | 3M projection | Direct visitor family |
| `Future_` | 3x6 with 15bp convexity adjustment | 3M projection | Direct visitor family |
| `STIR_` | 3x6 | OIS discount | Inherits FRA pricing |
| `Swap_` | 10Y, annual fixed and quarterly floating | OIS discount plus 3M projection | Direct visitor family |
| `OISSwap_` | 10Y, annual fixed and annual overnight | OIS discount | Inherits swap pricing |
| `BasisSwap_` | 10Y, quarterly 3M versus semiannual 6M | OIS discount plus two projections | Passive/BUMPED only |

Every matrix entry produces two rows:

1. `PRECOMPUTE lifecycle`: call `YCInstrument_::Precompute`, retain the returned pointer as
   the sink, and include replacement/destruction of the prior handle in the measured lifecycle.
2. `PRICE`: reuse one precomputed `Rate_` and call it against the immutable `CurveBlock_`.

The label must include instrument identity, representative maturity/frequencies, timing phase,
and the fact that the displayed time is normalized per operation.

## Calibration-Shaped Baskets

Build three baskets from distinct instruments and precompute their rates before timing:

1. **Discount basket, 20 instruments:** five deposits, three STIRs, and twelve OIS swaps.
2. **3M projection basket, 20 instruments:** eight FRAs, four futures, and eight vanilla swaps.
3. **Multi-tenor basket, 10 instruments:** ten 3M-versus-6M basis swaps.

A timed basket pass invokes every precomputed rate once and accumulates the model rates. The
same samples produce two printed results:

- elapsed time normalized to one complete basket pass;
- elapsed time normalized to one priced instrument.

This preserves attribution in the individual rows while exposing the loop shape used by
calibration residual evaluation.

## Measurement Semantics

- Use `Bench::Run` with three warmups and ten measured samples.
- Batch cheap individual operations 100,000 times per timed sample.
- Batch swap, OIS-swap, and basis-swap operations 5,000 times per pricing sample and 1,000
  times per precompute-lifecycle sample.
- Repeat each basket pass 1,000 times per timed sample.
- Normalize `medianNs`, `minNs`, and `maxNs` by the operation count before printing a
  per-operation row. Preserve the original repetition count.
- Print basket time once per pass and once per instrument from the same underlying samples;
  do not time the basket twice.
- Accumulate computed rates into a `double` checksum and pass its address to
  `Bench::DoNotOptimize` after each case.
- Keep case construction, curve construction, convention construction, and correctness checks
  outside timed regions.
- If an initial development run falls outside the 5-20ms target duration for a timed sample,
  adjust only that case's batch count while preserving all measurement and labeling rules.

The executable reports observations only. Cross-branch performance conclusions require paired,
interleaved Release runs with at least ten complete invocations and best-of-N minima. Changes
inside the observed 2-4% environment noise floor are inconclusive.

## Untimed Correctness Gates

Before printing timings, the executable must require that:

- every individual and basket model rate is finite;
- the 3M projected FRA rate differs from the OIS discount-only forward rate;
- the future price equals its corresponding forward rate less the configured convexity
  adjustment within `1.0e-12`;
- an STIR and an FRA with identical dates and discount-only conventions agree within
  `1.0e-12`;
- an OIS swap and a vanilla swap with identical discount-only conventions agree within
  `1.0e-12`;
- changing the 6M projection input changes the untimed basis-swap result;
- each timed pricing checksum is finite and non-zero, and each precompute sink retains a
  non-empty rate handle.

These checks protect the benchmark from silently measuring discount fallback under a
projection label or from optimizing away the pricing result. They are not runtime thresholds.

## Output Contract

The executable prints the shared benchmark header followed by exactly 20 result rows:

- 14 individual rows: seven instruments times two phases;
- six basket rows: three baskets times two normalizations.

Labels must remain within the shared 75-character name column and must use these phase tokens:
`PRECOMPUTE`, `PRICE`, `BASKET`, and `PER-INSTRUMENT`. `BasisSwap_` pricing labels must include
`PASSIVE` so the output cannot be mistaken for analytical-Jacobian coverage.

## Build And CI Integration

- Add `ycinstrument_perf` to `DAL_BENCHMARK_TARGETS` immediately before
  `curve_calibration_perf`.
- Add the same target immediately before `curve_calibration_perf` in Linux and Windows benchmark
  reporting arrays.
- Leave unrelated benchmark-list drift unchanged.
- Do not add the target to `.github/scripts/check_benchmark_regressions.py`.
- Install the executable to `bin` using the standard benchmark target permissions.

## Verification

1. Establish a red build by registering the target before its directory/source exists and
   confirm CMake fails because `ycinstrument_perf` is missing.
2. Build the target in the existing Release build tree.
3. Run the executable and confirm its 20 labels, finite checksums, and zero exit status.
4. Run the executable ten complete times and report the observed best/min/max spread without
   treating it as a gate.
5. Run `YCInstrumentTest.*`.
6. Run the full `dal_cpp_tests` suite.
7. Run `git diff --check` and inspect the exact CMake and workflow target lists.
8. Complete independent DAL performance and code reviews.

## Acceptance Criteria

- [ ] `ycinstrument_perf` builds in Release on the repository's supported CMake configuration.
- [ ] All seven concrete instrument types have separate precompute and steady-state pricing rows.
- [ ] Discount, 3M projection, and multi-tenor baskets print both basket and per-instrument time.
- [ ] All pricing construction is outside steady-state timed regions.
- [ ] Untimed correctness gates detect routing or fixture mistakes before measurement.
- [ ] The executable prints exactly 20 stable benchmark rows and exits successfully.
- [ ] Linux and Windows CI build, run, and publish the new benchmark output.
- [ ] No absolute timing threshold or new regression gate is introduced.
- [ ] Targeted and full DAL tests pass.
- [ ] Independent performance and code reviews report no blocking findings.

## Risks And Mitigations

- **Clock noise on cheap rates:** batch 100,000 calls and normalize after measurement.
- **Compiler elimination:** use virtual rate handles, a data-dependent checksum, and
  `Bench::DoNotOptimize`.
- **Projection silently falling back to discount:** require a route-sensitive FRA result before
  timing.
- **Coupon count dominating comparisons:** keep maturity and leg frequencies in every long-rate
  label; compare a row only with the same row across revisions.
- **Setup mixed into price timing:** create curves, instruments, and rate handles before every
  steady-state row.
- **Alias coverage misrepresented:** label STIR/OIS inheritance in source comments and retain
  distinct concrete timing rows.
- **Basis swap overstated as analytical support:** label it `PASSIVE` and keep it outside analytic
  claims and regression gates.

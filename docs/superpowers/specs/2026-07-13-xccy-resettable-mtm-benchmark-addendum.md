# XCCY Resettable/MTM — Benchmark Design Addendum

This addendum is part of
`docs/superpowers/specs/2026-07-13-xccy-resettable-mtm-design.md` and is normative
for the implementation plan.

## Benchmark Target

Add `dal-cpp/benchmarks/xccy_perf/` and register `xccy_perf` in
`dal-cpp/benchmarks/CMakeLists.txt`. A dedicated target is required because
`ycinstrument_perf` does not price XCCY instruments and `curve_calibration_perf`
does not exercise FX-reset resolution, basis calibration, or the three-block joint
solver.

Every timed workload must first validate finite, nonzero checksums and the expected
repricing tolerance. Batches must be large enough that timer overhead is immaterial.

## Required Workloads

### Instrument Planning and Pricing

- 10Y quarterly `FIXED`, `RESETTABLE`, and `MARK_TO_MARKET` precompute operations.
- Passive price operations for the same three all-future-reset instruments.
- An in-progress 10Y `MARK_TO_MARKET` price containing both historical and future resets
  resolved from one immutable fixing snapshot.
- Ten-instrument pricing baskets for each notional mode.

Report per-operation or per-instrument timings after batch normalization so the three
notional modes are directly comparable.

### Calibration

- Basis-only exact calibration with 15 XCCY instruments:
  - analytic solve-only;
  - analytic plus diagnostics/Jacobian;
  - bumped plus diagnostics/Jacobian.
- Full domestic/foreign/basis exact joint calibration:
  - analytic solve-only;
  - analytic plus diagnostics/Jacobian;
  - bumped plus diagnostics/Jacobian.
- Full three-block approximate joint calibration to cover regularized residual
  evaluation.

The analytic and bumped variants must use identical instruments, knots, initial guesses,
solver tolerances, and repricing acceptance. Timing must not compare workloads that do
different numerical work.

## Regression Protocol

- Use the merge base with `master` as the baseline unless another baseline is explicitly
  selected.
- Build baseline and branch in Release with benchmarks enabled.
- Run binaries from their respective build trees, never from stale installed `bin/`
  locations.
- Use paired, interleaved baseline/branch runs with at least 10 samples per binary.
- Reduce to best-of-N minima and treat a delta as actionable only when it exceeds 4%, the
  conservative end of DAL's calibrated 2–4% benchmark noise floor.
- Record sample count, environment quietness, min/median/max timings, workload size, and
  any inconclusive result.

## Acceptance Criteria

- [ ] Existing fixed-notional XCCY pricing and basis-only calibration do not regress by
  more than 4% relative to the merge-base implementation.
- [ ] The standard eight DAL gates (`tape_perf`, `jacobian_perf`, `pde_perf`, `rng_perf`,
  `interp_perf`, `krylov_perf`, `banded_perf`, and `cholesky_perf`) have no
  greater-than-4% branch regression under the same paired protocol.
- [ ] All new `xccy_perf` workloads publish absolute min/median/max results and normalized
  per-operation metrics.
- [ ] The first merged reset/MTM and three-block measurements establish the future
  regression baseline; they are not compared with nonexistent pre-feature cases.
- [ ] Analytic and bumped calibration cases meet equivalent repricing tolerances before
  their timing ratio is reported.

## Delivery Gate

Run functional and Jacobian tests before benchmarks. Performance measurement is a
post-correctness gate and must be completed before Python/Excel packaging is declared
finished, so late binding changes cannot hide a core regression.

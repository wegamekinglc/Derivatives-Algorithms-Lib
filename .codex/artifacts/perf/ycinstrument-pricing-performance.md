# Yield-Curve Instrument Pricing Performance Review

## Findings

### Blocking

None.

### Significant

None.

### Low / Advisory

1. **The ten-run artifact is too noisy for a threshold or regression verdict.** The 20 rows span
   3.59%-46.42% between the best and worst executable-level medians, with 18 of 20 rows above 4%;
   the 3M projection basket and its derived per-instrument row are the 46% outliers
   (`.superpowers/sdd/task-3-verification-report.md:64-91`). This does not require a code change:
   the executable is deliberately report-only and is absent from the calibrated regression set
   (`.github/scripts/check_benchmark_regressions.py:20-29`). A single CI run must not be used to
   claim a speedup or regression.

2. **Integer nanosecond normalization quantizes the cheapest rows.** `Normalize` divides the raw
   `int64_t` fields before printing (`dal-cpp/benchmarks/ycinstrument_perf/ycinstrument_perf.cpp:180-183`),
   while the cheap PRECOMPUTE rows are only 23-25 ns/operation
   (`.superpowers/sdd/task-3-verification-report.md:66-73`). One printed nanosecond is therefore
   about 4% of those rows, so their reported spread is partly output granularity rather than solely
   runtime variation. The underlying 250,000-operation batches are long enough to measure, so this
   is an interpretation caveat rather than a merge blocker. Future regression tooling should use
   raw batch samples or higher-precision normalization.

3. **The fixture is calibration-shaped, not a production-calendar latency model.** It deliberately
   uses zero lags, `Holidays::None()`, `Unadjusted`, and `ACT_365F`
   (`dal-cpp/benchmarks/ycinstrument_perf/ycinstrument_perf.cpp:62-85`). This improves determinism but
   understates calendar and settlement work in PRECOMPUTE relative to some real USD books. The
   benchmark still exercises the intended pricing topology: an eight-knot OIS curve, base-layered
   3M/6M forwards, map-backed routing, and realistic long-leg frequencies
   (`dal-cpp/benchmarks/ycinstrument_perf/ycinstrument_perf.cpp:88-125`). Treat the absolute numbers
   as stable fixture observations, not production SLAs.

## Verdict

**Approve.** No blocking or significant performance-methodology finding remains. The benchmark is
fit for report-only coverage and future like-for-like comparisons once a base revision also
contains the target.

## Reviewed Scope

- Diff: `401d5b7a..2b849146`
- Reviewed head: `2b849146` (`docs: clarify benchmark plan and noise evidence`)
- New target: `ycinstrument_perf`
- Implementation, target CMake, Linux/Windows reporting integration, design, implementation plan,
  and Task 3 verification evidence were reviewed in full.
- No production pricing, calibration, curve, AAD, or shared benchmark-harness implementation was
  changed by the feature.

## Resolved Review Items and Documentation Assessment

The documentation-only follow-up changed no benchmark, CMake, workflow, harness, or production
file. It resolves both prior documentation concerns:

- The implementation plan now places a prominent **Historical snapshot - do not copy** warning
  immediately before its embedded C++ draft. It directs readers to the final source and names the
  two material differences: the required `ycimp.hpp` declaration include and the measured per-case
  batch/pass counts
  (`docs/superpowers/plans/2026-07-12-ycinstrument-pricing-performance.md:96`). The stale embedded
  block can no longer reasonably be mistaken for authoritative implementation source.
- The design now calls 2-4% only a nominal expectation for quiet paired runs, separately records
  the actual unpinned-VM 3.59%-46.42% spread, links this performance artifact, and requires future
  conclusions to use the run-specific measured noise floor
  (`docs/superpowers/specs/2026-07-12-ycinstrument-pricing-performance-design.md:128-134`). This is
  consistent with the evidence and with the report-only CI decision.

The documentation-fix checkpoint reports a two-file-only commit, `git diff --check` and cached
scope success, and documentation validation for 31 Markdown files
(`.superpowers/sdd/task-4-doc-fix-report.md`). These resolved items do not alter the methodology
findings or the Approve verdict.

## Methodology Assessment

### Fixture realism and routing

The immutable fixture is constructed before measurement and contains:

- eight knots from 3M through 40Y;
- a 2% OIS discount curve;
- base-layered 3M and 6M projection curves with distinct spreads;
- a map-backed `CurveBlock_` containing discount and both projection entries
  (`dal-cpp/benchmarks/ycinstrument_perf/ycinstrument_perf.cpp:88-105`).

All seven concrete instrument identities are represented with the intended discount, projection,
and multi-tenor paths (`dal-cpp/benchmarks/ycinstrument_perf/ycinstrument_perf.cpp:107-125`). The
three baskets have realistic calibration shapes: 20 discount instruments, 20 3M-projection
instruments, and ten passive 3M-vs-6M basis swaps
(`dal-cpp/benchmarks/ycinstrument_perf/ycinstrument_perf.cpp:214-254`). `BasisSwap_` labels include
`PASSIVE`, so the output does not imply analytic-Jacobian eligibility.

Untimed gates prove that projection does not silently fall back to discount, Future convexity is
applied, STIR/OIS inheritance is preserved, and BasisSwap reads the 6M curve
(`dal-cpp/benchmarks/ycinstrument_perf/ycinstrument_perf.cpp:134-165`). Basket values are also
required to be finite and non-zero before timing
(`dal-cpp/benchmarks/ycinstrument_perf/ycinstrument_perf.cpp:265-274`).

### Timed-region boundaries and lifecycle separation

The phase split is sound:

- PRECOMPUTE repeatedly replaces a retained `RateHandle_`, so allocation, handle replacement, and
  destruction of the prior object are measured as a lifecycle
  (`dal-cpp/benchmarks/ycinstrument_perf/ycinstrument_perf.cpp:185-196`).
- PRICE creates one rate handle before `Bench::Run` and reuses it throughout the timed body
  (`dal-cpp/benchmarks/ycinstrument_perf/ycinstrument_perf.cpp:199-211`).
- Basket instruments, rate handles, and correctness checks are all built before the timed basket
  loops (`dal-cpp/benchmarks/ycinstrument_perf/ycinstrument_perf.cpp:293-318`).

“Steady-state” correctly means reuse after `YCInstrument_::Precompute`; it does not promise that a
production `Rate_::operator()` itself performs no internal schedule/context work. Curve,
instrument, convention, and test-fixture construction do not leak into PRICE rows.

### Batching and raw sample duration

The benchmark uses three warmups and ten measured samples per timed call. Case-specific batch sizes
are explicit at `dal-cpp/benchmarks/ycinstrument_perf/ycinstrument_perf.cpp:29-39`, and the shared
harness times the whole batch before sorting ten samples into median/min/max
(`dal-cpp/dal/benchmarks/bench.hpp:46-66`).

Initial development batches produced approximately 2.2-118 ms samples, so only operation/pass
counts were recalibrated. A post-calibration run placed all 17 independently timed calls in the
5.438-17.700 ms target range (`.superpowers/sdd/task-1-implementer-report.md:84-107`). The 20 printed
rows correspond to 17 timed calls because each of the three baskets prints both pass and
per-instrument normalization from one raw result.

Using the Task 3 best/worst normalized medians and the current divisors, the ten-run raw-median
envelope is approximately 5.431-20.793 ms. The upper value is the noisy 3M projection basket; the
other calibrated cases remain near the intended 5-20 ms window. This is long enough to prevent
clock-call overhead from dominating and short enough for routine Linux/Windows reporting.

### Normalization and output meaning

`Normalize` applies the same positive divisor to median, min, and max while preserving the repeat
count (`dal-cpp/benchmarks/ycinstrument_perf/ycinstrument_perf.cpp:180-183`). Basket pass and
per-instrument rows are produced from the same samples rather than timing the basket twice
(`dal-cpp/benchmarks/ycinstrument_perf/ycinstrument_perf.cpp:276-290`). The per-instrument basket
number is therefore an average across a heterogeneous basket, not the latency of any one member;
the individual rows provide attribution.

Task 3 confirmed ten complete executable invocations, exactly 20 rows in every invocation, 20
unique labels, and 28.289 seconds total wall time
(`.superpowers/sdd/task-3-verification-report.md:36-62`). Representative best-to-worst ranges were:

- cheap PRECOMPUTE: 23-25 ns/operation;
- cheap PRICE: 213-249 ns/operation;
- long PRECOMPUTE: 4.937-15.368 us/operation;
- long PRICE: 3.128-18.745 us/operation;
- baskets: 30.065-124.250 us/pass and 1.503-12.425 us/instrument
  (`.superpowers/sdd/task-3-verification-report.md:66-85`).

### Dead-code-elimination protection

Every PRICE/basket iteration contributes to a data-dependent checksum
(`dal-cpp/benchmarks/ycinstrument_perf/ycinstrument_perf.cpp:199-210`,
`dal-cpp/benchmarks/ycinstrument_perf/ycinstrument_perf.cpp:276-287`). PRECOMPUTE retains a live
rate handle. All sinks are passed through `Bench::DoNotOptimize`, whose GCC path uses an inline-asm
input and memory clobber and whose MSVC path uses `_ReadWriteBarrier`
(`dal-cpp/dal/benchmarks/bench.hpp:24-36`). Post-run REQUIRE checks also consume the sink/checksum.
This is adequate for the current non-LTO cross-translation-unit build.

## Noise Characterization and Regression Boundary

The local artifact used:

- Release, `-O3 -DNDEBUG`;
- GCC 15.2.0;
- AADet;
- `DAL_ENABLE_NATIVE_ARCH=OFF`;
- x86_64, Intel Core i9-13900HX, 32 logical processors under Microsoft full virtualization
  (`.superpowers/sdd/task-3-verification-report.md:159-180`).

The process was not pinned; turbo/governor state was uncontrolled; other host/VM activity could
not be excluded; and the run was not paired with a baseline. The 3M projection outlier confirms
that the artifact is descriptive, not a regression gate
(`.superpowers/sdd/task-3-verification-report.md:182-187`). CI uses native tuning on both Linux and
Windows (`.github/workflows/cmake-linux.yml:360-376`, `.github/workflows/cmake-windows.yml:230-255`),
so local absolute values must not be compared directly with CI output.

### No master baseline

`master` at `401d5b7a` has no `ycinstrument_perf` entry in
`dal-cpp/benchmarks/CMakeLists.txt` and no
`dal-cpp/benchmarks/ycinstrument_perf/ycinstrument_perf.cpp`; the reviewed head is `2b849146`.
Consequently, **master has no `ycinstrument_perf` baseline, so no branch-vs-baseline regression
conclusion is possible for this new executable**. The ten runs characterize only current-head
repeatability.

Once the target exists on both sides of a future change, performance conclusions should use paired,
interleaved Release runs with at least ten complete invocations per side and best-of-N minima. A
single invocation or a delta within the run-specific measured noise floor is inconclusive.

## CI Placement

The target is registered immediately before `curve_calibration_perf` in CMake
(`dal-cpp/benchmarks/CMakeLists.txt:16-17`) and in both reporting arrays
(`.github/workflows/cmake-linux.yml:399-420`, `.github/workflows/cmake-windows.yml:237-257`). Linux
runs the head executable from its build-tree path; Windows runs the installed executable. Both
capture and publish normal benchmark output.

The calibrated regression script still contains only the established eight-target set
(`.github/scripts/check_benchmark_regressions.py:20-29`), and Task 3 independently confirmed the new
name is absent (`.superpowers/sdd/task-3-verification-report.md:143-157`). Thus executable failures
remain visible, but timing values do not create a brittle merge gate.

## Verification Evidence

- Build: `ycinstrument_perf` and `dal_cpp_tests` resolved successfully
  (`.superpowers/sdd/task-3-verification-report.md:12-34`).
- Focused tests: 11/11 passed (`.superpowers/sdd/task-3-verification-report.md:93-106`).
- Full core tests: 876/876 across 79 suites passed
  (`.superpowers/sdd/task-3-verification-report.md:108-121`).
- Documentation integrity: 31 Markdown files passed; working tree diff/status/stat were clean
  (`.superpowers/sdd/task-3-verification-report.md:123-149`).
- Registration and regression-boundary checks passed
  (`.superpowers/sdd/task-3-verification-report.md:143-157`).
